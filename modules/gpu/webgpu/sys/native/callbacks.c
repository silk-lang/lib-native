#include <webgpu/webgpu.h>

_Static_assert(WGPUFeatureName_ShaderModuleCompilationOptions == 0x0005002e,
               "Update resources.SHADER_MODULE_COMPILATION_OPTIONS for the pinned Dawn ABI");

/* LIMITATION-4: Silk requires one callback invocation mode per declaration. */
void silkWebgpuRegisterError(WGPUDeviceDescriptor *descriptor,
                            WGPUUncapturedErrorCallback callback) {
    descriptor->uncapturedErrorCallbackInfo.callback = callback;
}

WGPUFuture silkWebgpuRequestDevice(WGPUAdapter adapter,
                                  WGPUDeviceDescriptor *descriptor,
                                  WGPUDeviceLostCallback lostCallback,
                                  WGPURequestDeviceCallbackInfo callbackInfo) {
    descriptor->deviceLostCallbackInfo.callback = lostCallback;
    return wgpuAdapterRequestDevice(adapter, descriptor, callbackInfo);
}

/* Dawn's native extension is deliberately opt-in per shader, not a device-wide math toggle. */
WGPUShaderModule silkWebgpuCreateShaderModuleStrict(WGPUDevice device,
                                                  const WGPUShaderModuleDescriptor *descriptor) {
    WGPUShaderModuleDescriptor strict = *descriptor;
    WGPUShaderModuleCompilationOptions options = WGPU_SHADER_MODULE_COMPILATION_OPTIONS_INIT;
    options.strictMath = WGPU_TRUE;
    options.chain.next = descriptor->nextInChain;
    strict.nextInChain = &options.chain;
    return wgpuDeviceCreateShaderModule(device, &strict);
}

/* A persistent cache for Dawn's shader translations and Vulkan pipeline caches, in a directory.
 * Each entry is one file named by a 128-bit hash of its key, holding the key (so a collision
 * reads as a miss) and the value. Dawn calls these from any thread, also several at once: each
 * write goes to a file of its own and is renamed into place, so readers see whole entries. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

typedef struct SilkWebgpuCache {
    struct SilkWebgpuCache *next;
    WGPUDawnCacheDeviceDescriptor descriptor;
    size_t length;
    char directory[];
} SilkWebgpuCache;

static void silkWebgpuCacheMakeDirectories(char *path) {
    for (char *at = path + 1; *at; at++) {
        if (*at != '/' && *at != '\\') continue;
        char saved = *at;
        *at = 0;
#ifdef _WIN32
        _mkdir(path);
#else
        mkdir(path, 0700);
#endif
        *at = saved;
    }
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0700);
#endif
}

static void silkWebgpuCachePath(const SilkWebgpuCache *cache, char *out, size_t outSize,
                                size_t keySize, const uint8_t *key) {
    uint64_t a = 0xcbf29ce484222325ull, b = 0x84222325cbf29ce4ull;
    for (size_t i = 0; i < keySize; i++) {
        a = (a ^ key[i]) * 0x100000001b3ull;
        b = (b ^ key[i] ^ (b >> 31)) * 0x9e3779b97f4a7c15ull;
    }
    snprintf(out, outSize, "%s/%016llx%016llx", cache->directory, (unsigned long long)a,
             (unsigned long long)b);
}

static size_t silkWebgpuCacheLoad(size_t keySize, uint8_t const *key, size_t valueSize,
                                  uint8_t *value, void *userdata1, void *userdata2) {
    (void)userdata2;
    const SilkWebgpuCache *cache = userdata1;
    char path[4200];
    silkWebgpuCachePath(cache, path, sizeof path, keySize, key);
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    uint64_t header[2] = {0, 0};
    size_t result = 0;
    if (fread(header, sizeof header, 1, file) == 1 && header[0] == keySize) {
        uint8_t *stored = malloc(keySize ? keySize : 1);
        if (stored && fread(stored, 1, keySize, file) == keySize &&
            memcmp(stored, key, keySize) == 0) {
            if (value == NULL || valueSize == 0) {
                result = (size_t)header[1];
            } else if (valueSize >= header[1] && fread(value, 1, header[1], file) == header[1]) {
                result = (size_t)header[1];
            }
        }
        free(stored);
    }
    fclose(file);
    return result;
}

static void silkWebgpuCacheStore(size_t keySize, uint8_t const *key, size_t valueSize,
                                 uint8_t const *value, void *userdata1, void *userdata2) {
    (void)userdata2;
    const SilkWebgpuCache *cache = userdata1;
    static unsigned long counter;
    unsigned long unique = __atomic_add_fetch(&counter, 1, __ATOMIC_RELAXED);
    char path[4200], temporary[4300];
    silkWebgpuCachePath(cache, path, sizeof path, keySize, key);
    snprintf(temporary, sizeof temporary, "%s.%p.%lu.tmp", path, (void *)&unique, unique);
    FILE *file = fopen(temporary, "wb");
    if (!file) return;
    uint64_t header[2] = {keySize, valueSize};
    int written = fwrite(header, sizeof header, 1, file) == 1 &&
                  fwrite(key, 1, keySize, file) == keySize &&
                  fwrite(value, 1, valueSize, file) == valueSize;
    if (fclose(file) != 0) written = 0;
    if (written) {
#ifdef _WIN32
        remove(path);
#endif
        if (rename(temporary, path) == 0) return;
    }
    remove(temporary);
}

/* The chain link that gives a device the cache in `directory` (made if missing). One link per
 * directory, kept for the process, since a device calls back into it for as long as it lives.
 * NULL when the directory name is too long or memory runs out: the device then has no cache. */
WGPUChainedStruct *silkWebgpuCacheChain(const uint8_t *directory, size_t length) {
    static SilkWebgpuCache *caches;
    static int lock;
    if (length == 0 || length > 4000) return NULL;
    while (__atomic_exchange_n(&lock, 1, __ATOMIC_ACQUIRE)) {
    }
    SilkWebgpuCache *cache = caches;
    while (cache && !(cache->length == length && memcmp(cache->directory, directory, length) == 0)) {
        cache = cache->next;
    }
    if (!cache) {
        cache = malloc(sizeof *cache + length + 1);
        if (cache) {
            memcpy(cache->directory, directory, length);
            cache->directory[length] = 0;
            cache->length = length;
            silkWebgpuCacheMakeDirectories(cache->directory);
            WGPUDawnCacheDeviceDescriptor descriptor = WGPU_DAWN_CACHE_DEVICE_DESCRIPTOR_INIT;
            descriptor.dawnLoadCacheDataCallbackInfo.callback = silkWebgpuCacheLoad;
            descriptor.dawnLoadCacheDataCallbackInfo.userdata1 = cache;
            descriptor.dawnStoreCacheDataCallbackInfo.callback = silkWebgpuCacheStore;
            descriptor.dawnStoreCacheDataCallbackInfo.userdata1 = cache;
            cache->descriptor = descriptor;
            cache->next = caches;
            caches = cache;
        }
    }
    __atomic_store_n(&lock, 0, __ATOMIC_RELEASE);
    return cache ? &cache->descriptor.chain : NULL;
}
