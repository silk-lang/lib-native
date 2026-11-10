#include <dlfcn.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

/* Xlib's public XImage prefix. The task needs no development headers: like the
   original ctypes harness it uses the installed libX11.so.6 runtime ABI. */
typedef struct {
    int width, height, xoffset, format;
    char *data;
    int byte_order, bitmap_unit, bitmap_bit_order, bitmap_pad, depth;
    int bytes_per_line, bits_per_pixel;
    unsigned long red_mask, green_mask, blue_mask;
} Image;
typedef struct {
    void *library;
    void *display;
    Image *(*get_image)(void *, unsigned long, int, int, unsigned int, unsigned int, unsigned long, int);
    int (*destroy_image)(Image *);
    int (*close_display)(void *);
} Capture;

void *silk_task_x11_open(const char *name, const unsigned char *cookie, size_t cookie_length) {
    if (!cookie || cookie_length != 16) return NULL;
    void *library = dlopen("libX11.so.6", RTLD_NOW | RTLD_LOCAL);
    if (!library) return NULL;
    void *(*open_display)(const char *) = (void *(*)(const char *))dlsym(library, "XOpenDisplay");
    void (*set_authorization)(char *, int, char *, int) = (void (*)(char *, int, char *, int))dlsym(library, "XSetAuthorization");
    Capture *capture = calloc(1, sizeof(*capture));
    if (!capture) { dlclose(library); return NULL; }
    capture->library = library;
    capture->get_image = (Image *(*)(void *, unsigned long, int, int, unsigned int, unsigned int, unsigned long, int))dlsym(library, "XGetImage");
    capture->destroy_image = (int (*)(Image *))dlsym(library, "XDestroyImage");
    capture->close_display = (int (*)(void *))dlsym(library, "XCloseDisplay");
    if (!open_display || !set_authorization || !capture->get_image || !capture->destroy_image || !capture->close_display) {
        free(capture); dlclose(library); return NULL;
    }
    /* This connection runs in the Silk process, whose environment is deliberately
       unchanged. Authorize it explicitly rather than relying on XAUTHORITY. */
    set_authorization("MIT-MAGIC-COOKIE-1", 18, (char *)cookie, (int)cookie_length);
    capture->display = open_display(name);
    set_authorization(NULL, 0, NULL, 0);
    if (!capture->display) { free(capture); dlclose(library); return NULL; }
    return capture;
}

int silk_task_x11_capture(void *handle, unsigned long window, uint32_t width, uint32_t height, uint8_t *rgba, size_t length) {
    Capture *capture = handle;
    if (!width || !height || (size_t)width > SIZE_MAX / height / 4 || length != (size_t)width * height * 4) return 1;
    Image *image = capture->get_image(capture->display, window, 0, 0, width, height, ~0UL, 2);
    if (!image) return 2;
    int result = 3;
    if (image->depth != 24 || (image->bits_per_pixel != 24 && image->bits_per_pixel != 32)) goto done;
    unsigned long masks[] = {image->red_mask, image->green_mask, image->blue_mask};
    unsigned shifts[3];
    for (unsigned channel = 0; channel < 3; ++channel) {
        unsigned long mask = masks[channel];
        if (!mask) goto done;
        shifts[channel] = 0;
        while (!(mask & 1)) { ++shifts[channel]; mask >>= 1; }
        if (mask != 255) goto done;
    }
    size_t step = (size_t)image->bits_per_pixel / 8;
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            const uint8_t *source = (const uint8_t *)image->data + y * (size_t)image->bytes_per_line + x * step;
            unsigned long pixel = 0;
            for (size_t byte = 0; byte < step; ++byte) {
                size_t shift = image->byte_order == 0 ? byte : step - byte - 1;
                pixel |= (unsigned long)source[byte] << (shift * 8);
            }
            size_t destination = (y * width + x) * 4;
            for (unsigned channel = 0; channel < 3; ++channel) rgba[destination + channel] = (uint8_t)((pixel & masks[channel]) >> shifts[channel]);
            rgba[destination + 3] = 255;
        }
    }
    result = 0;
done:
    capture->destroy_image(image);
    return result;
}

void silk_task_x11_close(void *handle) {
    if (!handle) return;
    Capture *capture = handle;
    capture->close_display(capture->display);
    dlclose(capture->library);
    free(capture);
}
