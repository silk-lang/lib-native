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

/* Dawn's shared texture memory and shared fences: Dawn extensions outside the webgpu.h schema the
 * generated bindings follow. These helpers take flat arguments so Silk never mirrors Dawn's
 * extension structs; gpu-webgpu's `shared_texture` module wraps them. */
_Static_assert(WGPUFeatureName_SharedTextureMemoryDmaBuf == 0x0005001f, "Update shared_texture.FEATURE_DMA_BUF");
_Static_assert(WGPUFeatureName_SharedTextureMemoryOpaqueFD == 0x00050020, "Update shared_texture.FEATURE_OPAQUE_FD");
_Static_assert(WGPUFeatureName_SharedTextureMemoryDXGISharedHandle == 0x00050022, "Update shared_texture.FEATURE_DXGI_SHARED_HANDLE");
_Static_assert(WGPUFeatureName_SharedTextureMemoryIOSurface == 0x00050024, "Update shared_texture.FEATURE_IOSURFACE");
_Static_assert(WGPUFeatureName_SharedFenceVkSemaphoreOpaqueFD == 0x00050026, "Update shared_texture.FEATURE_FENCE_VK_SEMAPHORE_OPAQUE_FD");
_Static_assert(WGPUFeatureName_SharedFenceSyncFD == 0x00050027, "Update shared_texture.FEATURE_FENCE_SYNC_FD");
_Static_assert(WGPUFeatureName_SharedFenceDXGISharedHandle == 0x00050029, "Update shared_texture.FEATURE_FENCE_DXGI_SHARED_HANDLE");
_Static_assert(WGPUFeatureName_SharedFenceMTLSharedEvent == 0x0005002a, "Update shared_texture.FEATURE_FENCE_MTL_SHARED_EVENT");
_Static_assert(WGPUSharedFenceType_VkSemaphoreOpaqueFD == 1 && WGPUSharedFenceType_SyncFD == 2 &&
               WGPUSharedFenceType_DXGISharedHandle == 4 && WGPUSharedFenceType_MTLSharedEvent == 5,
               "Update shared_texture's fence type constants");

#define SILK_SHARED_MAX_PLANES 4

static WGPUSharedTextureMemory silkImport(WGPUDevice device, WGPUStringView label, WGPUChainedStruct *chain) {
    WGPUSharedTextureMemoryDescriptor descriptor = WGPU_SHARED_TEXTURE_MEMORY_DESCRIPTOR_INIT;
    descriptor.nextInChain = chain;
    descriptor.label = label;
    return wgpuDeviceImportSharedTextureMemory(device, &descriptor);
}

WGPUSharedTextureMemory silkWebgpuImportDmaBuf(WGPUDevice device, WGPUStringView label, uint32_t width,
                                               uint32_t height, uint32_t drmFormat, uint64_t drmModifier,
                                               size_t planeCount, const int32_t *fds,
                                               const uint64_t *offsets, const uint32_t *strides) {
    if (planeCount == 0 || planeCount > SILK_SHARED_MAX_PLANES) {
        return NULL;
    }
    WGPUSharedTextureMemoryDmaBufPlane planes[SILK_SHARED_MAX_PLANES];
    for (size_t i = 0; i < planeCount; i++) {
        planes[i].fd = fds[i];
        planes[i].offset = offsets[i];
        planes[i].stride = strides[i];
    }
    WGPUSharedTextureMemoryDmaBufDescriptor dmaBuf = WGPU_SHARED_TEXTURE_MEMORY_DMA_BUF_DESCRIPTOR_INIT;
    dmaBuf.size.width = width;
    dmaBuf.size.height = height;
    dmaBuf.size.depthOrArrayLayers = 1;
    dmaBuf.drmFormat = drmFormat;
    dmaBuf.drmModifier = drmModifier;
    dmaBuf.planeCount = planeCount;
    dmaBuf.planes = planes;
    return silkImport(device, label, &dmaBuf.chain);
}

WGPUSharedTextureMemory silkWebgpuImportOpaqueFd(WGPUDevice device, WGPUStringView label,
                                                 const void *vkImageCreateInfo, int32_t memoryFd,
                                                 uint32_t memoryTypeIndex, uint64_t allocationSize,
                                                 uint32_t dedicatedAllocation) {
    WGPUSharedTextureMemoryOpaqueFDDescriptor opaque = WGPU_SHARED_TEXTURE_MEMORY_OPAQUE_FD_DESCRIPTOR_INIT;
    opaque.vkImageCreateInfo = vkImageCreateInfo;
    opaque.memoryFD = memoryFd;
    opaque.memoryTypeIndex = memoryTypeIndex;
    opaque.allocationSize = allocationSize;
    opaque.dedicatedAllocation = dedicatedAllocation ? WGPU_TRUE : WGPU_FALSE;
    return silkImport(device, label, &opaque.chain);
}

WGPUSharedTextureMemory silkWebgpuImportIOSurface(WGPUDevice device, WGPUStringView label, void *ioSurface) {
    WGPUSharedTextureMemoryIOSurfaceDescriptor surface = WGPU_SHARED_TEXTURE_MEMORY_IO_SURFACE_DESCRIPTOR_INIT;
    surface.ioSurface = ioSurface;
    return silkImport(device, label, &surface.chain);
}

WGPUSharedTextureMemory silkWebgpuImportDxgiSharedHandle(WGPUDevice device, WGPUStringView label,
                                                         void *handle, uint32_t useKeyedMutex) {
    WGPUSharedTextureMemoryDXGISharedHandleDescriptor shared = WGPU_SHARED_TEXTURE_MEMORY_DXGI_SHARED_HANDLE_DESCRIPTOR_INIT;
    shared.handle = handle;
    shared.useKeyedMutex = useKeyedMutex ? WGPU_TRUE : WGPU_FALSE;
    return silkImport(device, label, &shared.chain);
}

/* out: width, height, format, usage low word, usage high word. */
WGPUStatus silkWebgpuSharedTextureMemoryProperties(WGPUSharedTextureMemory memory, uint32_t *out) {
    WGPUSharedTextureMemoryProperties properties = WGPU_SHARED_TEXTURE_MEMORY_PROPERTIES_INIT;
    WGPUStatus status = wgpuSharedTextureMemoryGetProperties(memory, &properties);
    out[0] = properties.size.width;
    out[1] = properties.size.height;
    out[2] = (uint32_t)properties.format;
    out[3] = (uint32_t)(properties.usage & 0xffffffffu);
    out[4] = (uint32_t)(properties.usage >> 32);
    return status;
}

WGPUTexture silkWebgpuSharedTextureMemoryCreateTexture(WGPUSharedTextureMemory memory, WGPUStringView label) {
    WGPUSharedTextureMemoryProperties properties = WGPU_SHARED_TEXTURE_MEMORY_PROPERTIES_INIT;
    if (wgpuSharedTextureMemoryGetProperties(memory, &properties) != WGPUStatus_Success) {
        return wgpuSharedTextureMemoryCreateTexture(memory, NULL);
    }
    WGPUTextureDescriptor descriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
    descriptor.label = label;
    descriptor.usage = properties.usage;
    descriptor.size = properties.size;
    descriptor.format = properties.format;
    return wgpuSharedTextureMemoryCreateTexture(memory, &descriptor);
}

/* A Vulkan layout pair applies only when useVkLayout is nonzero (images a Vulkan producer handed
 * over in a known layout). */
WGPUStatus silkWebgpuSharedTextureMemoryBeginAccess(WGPUSharedTextureMemory memory, WGPUTexture texture,
                                                    uint32_t initialized, uint32_t concurrentRead,
                                                    size_t fenceCount, const WGPUSharedFence *fences,
                                                    const uint64_t *signaledValues, uint32_t useVkLayout,
                                                    int32_t vkOldLayout, int32_t vkNewLayout) {
    WGPUSharedTextureMemoryVkImageLayoutBeginState layout = WGPU_SHARED_TEXTURE_MEMORY_VK_IMAGE_LAYOUT_BEGIN_STATE_INIT;
    layout.oldLayout = vkOldLayout;
    layout.newLayout = vkNewLayout;
    WGPUSharedTextureMemoryBeginAccessDescriptor descriptor = WGPU_SHARED_TEXTURE_MEMORY_BEGIN_ACCESS_DESCRIPTOR_INIT;
    descriptor.nextInChain = useVkLayout ? &layout.chain : NULL;
    descriptor.initialized = initialized ? WGPU_TRUE : WGPU_FALSE;
    descriptor.concurrentRead = concurrentRead ? WGPU_TRUE : WGPU_FALSE;
    descriptor.fenceCount = fenceCount;
    descriptor.fences = fences;
    descriptor.signaledValueCount = fenceCount;
    descriptor.signaledValues = signaledValues;
    return wgpuSharedTextureMemoryBeginAccess(memory, texture, &descriptor);
}

/* The end state the caller reads back: whether the contents are initialized, the fences to wait
 * on before the producer reuses the memory (each with one reference the caller releases), and on
 * Vulkan the layouts the image was left in. `fences` and `values` hold `capacity` entries;
 * `out` receives initialized, fence count (which may exceed capacity: the extra fences are not
 * returned), Vulkan old layout, Vulkan new layout. */
WGPUStatus silkWebgpuSharedTextureMemoryEndAccess(WGPUSharedTextureMemory memory, WGPUTexture texture,
                                                  uint32_t useVkLayout, size_t capacity,
                                                  WGPUSharedFence *fences, uint64_t *values,
                                                  int32_t *out) {
    WGPUSharedTextureMemoryVkImageLayoutEndState layout = WGPU_SHARED_TEXTURE_MEMORY_VK_IMAGE_LAYOUT_END_STATE_INIT;
    WGPUSharedTextureMemoryEndAccessState state = WGPU_SHARED_TEXTURE_MEMORY_END_ACCESS_STATE_INIT;
    state.nextInChain = useVkLayout ? &layout.chain : NULL;
    WGPUStatus status = wgpuSharedTextureMemoryEndAccess(memory, texture, &state);
    out[0] = state.initialized ? 1 : 0;
    out[1] = (int32_t)state.fenceCount;
    out[2] = layout.oldLayout;
    out[3] = layout.newLayout;
    for (size_t i = 0; i < state.fenceCount && i < capacity; i++) {
        wgpuSharedFenceAddRef(state.fences[i]);
        fences[i] = state.fences[i];
        values[i] = state.signaledValues != NULL ? state.signaledValues[i] : 0;
    }
    wgpuSharedTextureMemoryEndAccessStateFreeMembers(state);
    return status;
}

uint32_t silkWebgpuSharedTextureMemoryIsDeviceLost(WGPUSharedTextureMemory memory) {
    return wgpuSharedTextureMemoryIsDeviceLost(memory) ? 1 : 0;
}

void silkWebgpuSharedTextureMemoryAddRef(WGPUSharedTextureMemory memory) {
    wgpuSharedTextureMemoryAddRef(memory);
}

void silkWebgpuSharedTextureMemoryRelease(WGPUSharedTextureMemory memory) {
    wgpuSharedTextureMemoryRelease(memory);
}

static WGPUSharedFence silkImportFence(WGPUDevice device, WGPUChainedStruct *chain) {
    WGPUSharedFenceDescriptor descriptor = WGPU_SHARED_FENCE_DESCRIPTOR_INIT;
    descriptor.nextInChain = chain;
    return wgpuDeviceImportSharedFence(device, &descriptor);
}

WGPUSharedFence silkWebgpuImportSyncFdFence(WGPUDevice device, int32_t fd) {
    WGPUSharedFenceSyncFDDescriptor sync = WGPU_SHARED_FENCE_SYNC_FD_DESCRIPTOR_INIT;
    sync.handle = fd;
    return silkImportFence(device, &sync.chain);
}

WGPUSharedFence silkWebgpuImportVkSemaphoreOpaqueFdFence(WGPUDevice device, int32_t fd) {
    WGPUSharedFenceVkSemaphoreOpaqueFDDescriptor semaphore = WGPU_SHARED_FENCE_VK_SEMAPHORE_OPAQUE_FD_DESCRIPTOR_INIT;
    semaphore.handle = fd;
    return silkImportFence(device, &semaphore.chain);
}

WGPUSharedFence silkWebgpuImportMtlSharedEventFence(WGPUDevice device, void *event) {
    WGPUSharedFenceMTLSharedEventDescriptor shared = WGPU_SHARED_FENCE_MTL_SHARED_EVENT_DESCRIPTOR_INIT;
    shared.sharedEvent = event;
    return silkImportFence(device, &shared.chain);
}

WGPUSharedFence silkWebgpuImportDxgiSharedHandleFence(WGPUDevice device, void *handle) {
    WGPUSharedFenceDXGISharedHandleDescriptor shared = WGPU_SHARED_FENCE_DXGI_SHARED_HANDLE_DESCRIPTOR_INIT;
    shared.handle = handle;
    return silkImportFence(device, &shared.chain);
}

/* The fence's type, and its handle: a file descriptor in *fd for the FD types, a pointer in
 * *handle for the others. Handles stay owned by the fence. */
uint32_t silkWebgpuSharedFenceExport(WGPUSharedFence fence, int32_t *fd, void **handle) {
    /* The first call asks for the type alone: a fence takes only its own type's chain. */
    WGPUSharedFenceExportInfo info = WGPU_SHARED_FENCE_EXPORT_INFO_INIT;
    wgpuSharedFenceExportInfo(fence, &info);
    *fd = -1;
    *handle = NULL;
    switch (info.type) {
    case WGPUSharedFenceType_SyncFD: {
        WGPUSharedFenceSyncFDExportInfo sync = WGPU_SHARED_FENCE_SYNC_FD_EXPORT_INFO_INIT;
        info.nextInChain = &sync.chain;
        wgpuSharedFenceExportInfo(fence, &info);
        *fd = sync.handle;
        return (uint32_t)info.type;
    }
    case WGPUSharedFenceType_VkSemaphoreOpaqueFD: {
        WGPUSharedFenceVkSemaphoreOpaqueFDExportInfo opaque = WGPU_SHARED_FENCE_VK_SEMAPHORE_OPAQUE_FD_EXPORT_INFO_INIT;
        info.nextInChain = &opaque.chain;
        wgpuSharedFenceExportInfo(fence, &info);
        *fd = opaque.handle;
        return (uint32_t)info.type;
    }
    case WGPUSharedFenceType_MTLSharedEvent: {
        WGPUSharedFenceMTLSharedEventExportInfo event = WGPU_SHARED_FENCE_MTL_SHARED_EVENT_EXPORT_INFO_INIT;
        info.nextInChain = &event.chain;
        wgpuSharedFenceExportInfo(fence, &info);
        *handle = event.sharedEvent;
        return (uint32_t)info.type;
    }
    case WGPUSharedFenceType_DXGISharedHandle: {
        WGPUSharedFenceDXGISharedHandleExportInfo shared = WGPU_SHARED_FENCE_DXGI_SHARED_HANDLE_EXPORT_INFO_INIT;
        info.nextInChain = &shared.chain;
        wgpuSharedFenceExportInfo(fence, &info);
        *handle = shared.handle;
        return (uint32_t)info.type;
    }
    default:
        return (uint32_t)info.type;
    }
}

void silkWebgpuSharedFenceAddRef(WGPUSharedFence fence) {
    wgpuSharedFenceAddRef(fence);
}

void silkWebgpuSharedFenceRelease(WGPUSharedFence fence) {
    wgpuSharedFenceRelease(fence);
}

/* Memory two devices of one process share: the producer side of a shared texture. macOS only
 * here (an IOSurface); other systems return NULL and callers copy through the CPU instead. */
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <IOSurface/IOSurfaceRef.h>
#include <objc/message.h>
#include <objc/runtime.h>

static CFNumberRef silkNumber(int32_t value) {
    return CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
}

/* A BGRA8 IOSurface with one reference the caller releases, or NULL. */
void *silkWebgpuCreateIOSurface(uint32_t width, uint32_t height) {
    const void *keys[] = {kIOSurfaceWidth, kIOSurfaceHeight, kIOSurfaceBytesPerElement, kIOSurfacePixelFormat};
    const void *values[] = {silkNumber((int32_t)width), silkNumber((int32_t)height), silkNumber(4),
                            silkNumber(0x42475241 /* 'BGRA' */)};
    CFDictionaryRef properties = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 4,
                                                    &kCFTypeDictionaryKeyCallBacks,
                                                    &kCFTypeDictionaryValueCallBacks);
    for (int i = 0; i < 4; i++) {
        CFRelease(values[i]);
    }
    IOSurfaceRef surface = IOSurfaceCreate(properties);
    CFRelease(properties);
    return (void *)surface;
}

void silkWebgpuReleaseIOSurface(void *surface) {
    if (surface != NULL) {
        CFRelease((CFTypeRef)surface);
    }
}

/* Whether a process (WindowServer, Core Animation) still uses the surface: 1, 0, or -1 for none. */
int32_t silkWebgpuIOSurfaceInUse(void *surface) {
    if (surface == NULL) {
        return -1;
    }
    return IOSurfaceIsInUse((IOSurfaceRef)surface) ? 1 : 0;
}
#else
void *silkWebgpuCreateIOSurface(uint32_t width, uint32_t height) {
    (void)width;
    (void)height;
    return NULL;
}

void silkWebgpuReleaseIOSurface(void *surface) {
    (void)surface;
}

int32_t silkWebgpuIOSurfaceInUse(void *surface) {
    (void)surface;
    return -1;
}
#endif

#ifndef _WIN32
#include <poll.h>
#endif

/* Whether the fence reached `value`, without waiting: 1 yes, 0 not yet, -1 when this fence type
 * cannot be asked here (the caller then passes the fence to BeginAccess, which waits on the GPU). */
int32_t silkWebgpuSharedFenceIsSignaled(WGPUSharedFence fence, uint64_t value) {
    int32_t fd = -1;
    void *handle = NULL;
    uint32_t type = silkWebgpuSharedFenceExport(fence, &fd, &handle);
    (void)value;
#ifndef _WIN32
    if (type == WGPUSharedFenceType_SyncFD) {
        if (fd < 0) {
            return 1;
        }
        struct pollfd entry = {fd, POLLIN, 0};
        int ready = poll(&entry, 1, 0);
        return ready < 0 ? -1 : (ready > 0 ? 1 : 0);
    }
#endif
#ifdef __APPLE__
    if (type == WGPUSharedFenceType_MTLSharedEvent && handle != NULL) {
        uint64_t signaled = ((uint64_t (*)(id, SEL))objc_msgSend)((id)handle, sel_registerName("signaledValue"));
        return signaled >= value ? 1 : 0;
    }
#endif
    return -1;
}
