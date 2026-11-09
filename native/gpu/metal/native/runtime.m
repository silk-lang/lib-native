// Copyright GFX developers. SPDX-License-Identifier: MIT OR Apache-2.0
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#include <stdint.h>

void *metal_retain(void *object) { return [(id)object retain]; }
void metal_release(void *object) { [(id)object release]; }
void *metal_pool_new(void) { return [NSAutoreleasePool new]; }
void metal_pool_drain(void *pool) { [(NSAutoreleasePool *)pool drain]; }
uint8_t metal_responds_to(void *object, const char *selector) {
    return [(id)object respondsToSelector:sel_registerName(selector)];
}
const char *metal_error_description(void *error) {
    return [[(NSError *)error localizedDescription] UTF8String];
}
int64_t metal_error_code(void *error) { return [(NSError *)error code]; }
const void *metal_data_bytes(void *data) { return [(NSData *)data bytes]; }
uint64_t metal_data_length(void *data) { return [(NSData *)data length]; }
void *metal_string_new(const uint8_t *bytes, size_t length) {
    return [[NSString alloc] initWithBytes:bytes length:length encoding:NSUTF8StringEncoding];
}
const char *metal_string_bytes(void *string) { return [(NSString *)string UTF8String]; }
size_t metal_string_length(void *string) {
    return [(NSString *)string lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
}
