// SPDX-License-Identifier: MIT OR Apache-2.0
// Port of metal-rs; Copyright 2016 GFX developers and metal-rs contributors.

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <stdint.h>
#import <objc/message.h>
#include <math.h>

// Private selectors exposed upstream behind its private feature. Availability is the caller contract.
@interface MTLSamplerDescriptor (SilkPrivateResourceSelectors)
- (void)setLodBias:(float)bias;
@end
@interface MTLVertexDescriptor (SilkPrivateResourceSelectors)
- (id)newSerializedDescriptor;
@end

void * metal_Resource_device(void * object) {
    id<MTLResource> receiver = (id<MTLResource>)object;
    return (void *)[receiver device];
}

const uint8_t * metal_Resource_label(void * object) {
    id<MTLResource> receiver = (id<MTLResource>)object;
    return (const uint8_t *)[[receiver label] UTF8String];
}

void metal_Resource_set_label(void * object, const uint8_t * label) {
    id<MTLResource> receiver = (id<MTLResource>)object;
    [receiver setLabel:[NSString stringWithUTF8String:(const char *)label]];
}

uint64_t metal_Resource_cpu_cache_mode(void * object) {
    id<MTLResource> receiver = (id<MTLResource>)object;
    return (uint64_t)[receiver cpuCacheMode];
}

uint64_t metal_Resource_storage_mode(void * object) {
    id<MTLResource> receiver = (id<MTLResource>)object;
    return (uint64_t)[receiver storageMode];
}

uint64_t metal_Resource_allocated_size(void * object) {
    id<MTLResource> receiver = (id<MTLResource>)object;
    return (uint64_t)[receiver allocatedSize];
}

uint64_t metal_Resource_hazard_tracking_mode(void * object) {
    id<MTLResource> receiver = (id<MTLResource>)object;
    return (uint64_t)[receiver hazardTrackingMode];
}

uint64_t metal_Resource_resource_options(void * object) {
    id<MTLResource> receiver = (id<MTLResource>)object;
    return (uint64_t)[receiver resourceOptions];
}

uint64_t metal_Resource_heap_offset(void * object) {
    id<MTLResource> receiver = (id<MTLResource>)object;
    return (uint64_t)[receiver heapOffset];
}

uint64_t metal_Resource_set_purgeable_state(void * object, uint64_t state) {
    id<MTLResource> receiver = (id<MTLResource>)object;
    return (uint64_t)[receiver setPurgeableState:state];
}

void * metal_Resource_heap(void * object) {
    id<MTLResource> receiver = (id<MTLResource>)object;
    return (void *)[receiver heap];
}

void metal_Resource_make_aliasable(void * object) {
    id<MTLResource> receiver = (id<MTLResource>)object;
    [receiver makeAliasable];
}

uint8_t metal_Resource_is_aliasable(void * object) {
    id<MTLResource> receiver = (id<MTLResource>)object;
    return [receiver isAliasable] ? 1 : 0;
}

uint64_t metal_Buffer_length(void * object) {
    id<MTLBuffer> receiver = (id<MTLBuffer>)object;
    return (uint64_t)[receiver length];
}

uint8_t * metal_Buffer_contents(void * object) {
    id<MTLBuffer> receiver = (id<MTLBuffer>)object;
    return (uint8_t *)[receiver contents];
}

void metal_Buffer_did_modify_range(void * object, NSRange range) {
    id<MTLBuffer> receiver = (id<MTLBuffer>)object;
    [receiver didModifyRange:range];
}

void * metal_Buffer_new_texture_with_descriptor(void * object, void * descriptor, uint64_t offset, uint64_t bytes_per_row) {
    id<MTLBuffer> receiver = (id<MTLBuffer>)object;
    return (void *)[receiver newTextureWithDescriptor:(id)descriptor offset:offset bytesPerRow:bytes_per_row];
}

void * metal_Buffer_remote_storage_buffer(void * object) {
    id<MTLBuffer> receiver = (id<MTLBuffer>)object;
    return (void *)[receiver remoteStorageBuffer];
}

void * metal_Buffer_new_remote_buffer_view_for_device(void * object, void * device) {
    id<MTLBuffer> receiver = (id<MTLBuffer>)object;
    return (void *)[receiver newRemoteBufferViewForDevice:(id)device];
}

void metal_Buffer_add_debug_marker(void * object, const uint8_t * name, NSRange range) {
    id<MTLBuffer> receiver = (id<MTLBuffer>)object;
    [receiver addDebugMarker:[NSString stringWithUTF8String:(const char *)name] range:range];
}

void metal_Buffer_remove_all_debug_markers(void * object) {
    id<MTLBuffer> receiver = (id<MTLBuffer>)object;
    [receiver removeAllDebugMarkers];
}

uint64_t metal_Buffer_gpu_address(void * object) {
    id<MTLBuffer> receiver = (id<MTLBuffer>)object;
    return (uint64_t)[receiver gpuAddress];
}

MTLResourceID metal_Buffer_gpu_resource_id(void * object) {
    // This upstream selector is not public SDK API. The caller checks availability.
    return ((MTLResourceID (*)(id, SEL))objc_msgSend)((id)object, sel_registerName("gpuResourceID"));
}

void * metal_TextureDescriptor_new(void) {
    return (void *)[MTLTextureDescriptor new];
}

uint64_t metal_TextureDescriptor_texture_type(void * object) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    return (uint64_t)[receiver textureType];
}

void metal_TextureDescriptor_set_texture_type(void * object, uint64_t value) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    [receiver setTextureType:value];
}

uint64_t metal_TextureDescriptor_pixel_format(void * object) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    return (uint64_t)[receiver pixelFormat];
}

void metal_TextureDescriptor_set_pixel_format(void * object, uint64_t value) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    [receiver setPixelFormat:value];
}

uint64_t metal_TextureDescriptor_width(void * object) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    return (uint64_t)[receiver width];
}

void metal_TextureDescriptor_set_width(void * object, uint64_t value) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    [receiver setWidth:value];
}

uint64_t metal_TextureDescriptor_height(void * object) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    return (uint64_t)[receiver height];
}

void metal_TextureDescriptor_set_height(void * object, uint64_t value) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    [receiver setHeight:value];
}

uint64_t metal_TextureDescriptor_depth(void * object) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    return (uint64_t)[receiver depth];
}

void metal_TextureDescriptor_set_depth(void * object, uint64_t value) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    [receiver setDepth:value];
}

uint64_t metal_TextureDescriptor_mipmap_level_count(void * object) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    return (uint64_t)[receiver mipmapLevelCount];
}

void metal_TextureDescriptor_set_mipmap_level_count(void * object, uint64_t value) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    [receiver setMipmapLevelCount:value];
}

uint64_t metal_TextureDescriptor_sample_count(void * object) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    return (uint64_t)[receiver sampleCount];
}

void metal_TextureDescriptor_set_sample_count(void * object, uint64_t value) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    [receiver setSampleCount:value];
}

uint64_t metal_TextureDescriptor_array_length(void * object) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    return (uint64_t)[receiver arrayLength];
}

void metal_TextureDescriptor_set_array_length(void * object, uint64_t value) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    [receiver setArrayLength:value];
}

uint64_t metal_TextureDescriptor_resource_options(void * object) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    return (uint64_t)[receiver resourceOptions];
}

void metal_TextureDescriptor_set_resource_options(void * object, uint64_t value) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    [receiver setResourceOptions:value];
}

uint64_t metal_TextureDescriptor_cpu_cache_mode(void * object) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    return (uint64_t)[receiver cpuCacheMode];
}

void metal_TextureDescriptor_set_cpu_cache_mode(void * object, uint64_t value) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    [receiver setCpuCacheMode:value];
}

uint64_t metal_TextureDescriptor_storage_mode(void * object) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    return (uint64_t)[receiver storageMode];
}

void metal_TextureDescriptor_set_storage_mode(void * object, uint64_t value) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    [receiver setStorageMode:value];
}

uint64_t metal_TextureDescriptor_usage(void * object) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    return (uint64_t)[receiver usage];
}

void metal_TextureDescriptor_set_usage(void * object, uint64_t value) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    [receiver setUsage:value];
}

uint64_t metal_TextureDescriptor_compression_type(void * object) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    return (uint64_t)[receiver compressionType];
}

void metal_TextureDescriptor_set_compression_type(void * object, uint64_t value) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    [receiver setCompressionType:value];
}

void metal_TextureDescriptor_set_mipmap_level_count_for_size(void * object, MTLSize size) {
    MTLTextureDescriptor * receiver = (MTLTextureDescriptor *)object;
    NSUInteger maximum = MAX(size.width, MAX(size.height, size.depth));
    double count = ceil(log2((double)maximum));
    [receiver setMipmapLevelCount:count > 0.0 ? (NSUInteger)count : 0];
}

void * metal_Texture_root_resource(void * object) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    return (void *)[receiver rootResource];
}

void * metal_Texture_parent_texture(void * object) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    return (void *)[receiver parentTexture];
}

uint64_t metal_Texture_parent_relative_level(void * object) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    return (uint64_t)[receiver parentRelativeLevel];
}

uint64_t metal_Texture_parent_relative_slice(void * object) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    return (uint64_t)[receiver parentRelativeSlice];
}

void * metal_Texture_buffer(void * object) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    return (void *)[receiver buffer];
}

uint64_t metal_Texture_buffer_offset(void * object) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    return (uint64_t)[receiver bufferOffset];
}

uint64_t metal_Texture_buffer_stride(void * object) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    return (uint64_t)[receiver bufferBytesPerRow];
}

uint64_t metal_Texture_texture_type(void * object) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    return (uint64_t)[receiver textureType];
}

uint64_t metal_Texture_pixel_format(void * object) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    return (uint64_t)[receiver pixelFormat];
}

uint64_t metal_Texture_width(void * object) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    return (uint64_t)[receiver width];
}

uint64_t metal_Texture_height(void * object) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    return (uint64_t)[receiver height];
}

uint64_t metal_Texture_depth(void * object) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    return (uint64_t)[receiver depth];
}

uint64_t metal_Texture_mipmap_level_count(void * object) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    return (uint64_t)[receiver mipmapLevelCount];
}

uint64_t metal_Texture_sample_count(void * object) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    return (uint64_t)[receiver sampleCount];
}

uint64_t metal_Texture_array_length(void * object) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    return (uint64_t)[receiver arrayLength];
}

uint64_t metal_Texture_usage(void * object) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    return (uint64_t)[receiver usage];
}

uint8_t metal_Texture_framebuffer_only(void * object) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    return [receiver isFramebufferOnly] ? 1 : 0;
}

void metal_Texture_get_bytes(void * object, uint8_t * bytes, uint64_t stride, MTLRegion region, uint64_t mipmap_level) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    [receiver getBytes:bytes bytesPerRow:stride fromRegion:region mipmapLevel:mipmap_level];
}

void metal_Texture_get_bytes_in_slice(void * object, uint8_t * bytes, uint64_t stride, uint64_t image_stride, MTLRegion region, uint64_t mipmap_level, uint64_t slice) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    [receiver getBytes:bytes bytesPerRow:stride bytesPerImage:image_stride fromRegion:region mipmapLevel:mipmap_level slice:slice];
}

void metal_Texture_replace_region(void * object, MTLRegion region, uint64_t mipmap_level, const uint8_t * bytes, uint64_t stride) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    [receiver replaceRegion:region mipmapLevel:mipmap_level withBytes:bytes bytesPerRow:stride];
}

void metal_Texture_replace_region_in_slice(void * object, MTLRegion region, uint64_t mipmap_level, uint64_t slice, const uint8_t * bytes, uint64_t stride, uint64_t image_stride) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    [receiver replaceRegion:region mipmapLevel:mipmap_level slice:slice withBytes:bytes bytesPerRow:stride bytesPerImage:image_stride];
}

void * metal_Texture_new_texture_view(void * object, uint64_t pixel_format) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    return (void *)[receiver newTextureViewWithPixelFormat:pixel_format];
}

void * metal_Texture_new_texture_view_from_slice(void * object, uint64_t pixel_format, uint64_t texture_type, NSRange mipmap_levels, NSRange slices) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    return (void *)[receiver newTextureViewWithPixelFormat:pixel_format textureType:texture_type levels:mipmap_levels slices:slices];
}

MTLResourceID metal_Texture_gpu_resource_id(void * object) {
    id<MTLTexture> receiver = (id<MTLTexture>)object;
    return [receiver gpuResourceID];
}

void * metal_Heap_device(void * object) {
    id<MTLHeap> receiver = (id<MTLHeap>)object;
    return (void *)[receiver device];
}

const uint8_t * metal_Heap_label(void * object) {
    id<MTLHeap> receiver = (id<MTLHeap>)object;
    return (const uint8_t *)[[receiver label] UTF8String];
}

void metal_Heap_set_label(void * object, const uint8_t * label) {
    id<MTLHeap> receiver = (id<MTLHeap>)object;
    [receiver setLabel:[NSString stringWithUTF8String:(const char *)label]];
}

uint64_t metal_Heap_cpu_cache_mode(void * object) {
    id<MTLHeap> receiver = (id<MTLHeap>)object;
    return (uint64_t)[receiver cpuCacheMode];
}

uint64_t metal_Heap_storage_mode(void * object) {
    id<MTLHeap> receiver = (id<MTLHeap>)object;
    return (uint64_t)[receiver storageMode];
}

uint64_t metal_Heap_hazard_tracking_mode(void * object) {
    id<MTLHeap> receiver = (id<MTLHeap>)object;
    return (uint64_t)[receiver hazardTrackingMode];
}

uint64_t metal_Heap_resource_options(void * object) {
    id<MTLHeap> receiver = (id<MTLHeap>)object;
    return (uint64_t)[receiver resourceOptions];
}

uint64_t metal_Heap_size(void * object) {
    id<MTLHeap> receiver = (id<MTLHeap>)object;
    return (uint64_t)[receiver size];
}

uint64_t metal_Heap_used_size(void * object) {
    id<MTLHeap> receiver = (id<MTLHeap>)object;
    return (uint64_t)[receiver usedSize];
}

uint64_t metal_Heap_heap_type(void * object) {
    id<MTLHeap> receiver = (id<MTLHeap>)object;
    return (uint64_t)[receiver type];
}

uint64_t metal_Heap_current_allocated_size(void * object) {
    id<MTLHeap> receiver = (id<MTLHeap>)object;
    return (uint64_t)[receiver currentAllocatedSize];
}

uint64_t metal_Heap_set_purgeable_state(void * object, uint64_t state) {
    id<MTLHeap> receiver = (id<MTLHeap>)object;
    return (uint64_t)[receiver setPurgeableState:state];
}

uint64_t metal_Heap_max_available_size_with_alignment(void * object, uint64_t alignment) {
    id<MTLHeap> receiver = (id<MTLHeap>)object;
    return (uint64_t)[receiver maxAvailableSizeWithAlignment:alignment];
}

void * metal_Heap_new_buffer(void * object, uint64_t length, uint64_t options) {
    id<MTLHeap> receiver = (id<MTLHeap>)object;
    return (void *)[receiver newBufferWithLength:length options:options];
}

void * metal_Heap_new_texture(void * object, void * descriptor) {
    id<MTLHeap> receiver = (id<MTLHeap>)object;
    return (void *)[receiver newTextureWithDescriptor:(id)descriptor];
}

void * metal_Heap_new_buffer_with_offset(void * object, uint64_t length, uint64_t options, uint64_t offset) {
    id<MTLHeap> receiver = (id<MTLHeap>)object;
    return (void *)[receiver newBufferWithLength:length options:options offset:offset];
}

void * metal_Heap_new_texture_with_offset(void * object, void * descriptor, uint64_t offset) {
    id<MTLHeap> receiver = (id<MTLHeap>)object;
    return (void *)[receiver newTextureWithDescriptor:(id)descriptor offset:offset];
}

void * metal_Heap_new_acceleration_structure_with_descriptor(void * object, void * descriptor) {
    id<MTLHeap> receiver = (id<MTLHeap>)object;
    return (void *)[receiver newAccelerationStructureWithDescriptor:(id)descriptor];
}

void * metal_Heap_new_acceleration_structure_with_descriptor_offset(void * object, void * descriptor, uint64_t offset) {
    id<MTLHeap> receiver = (id<MTLHeap>)object;
    return (void *)[receiver newAccelerationStructureWithDescriptor:(id)descriptor offset:offset];
}

void * metal_Heap_new_acceleration_structure_with_size(void * object, uint64_t size) {
    id<MTLHeap> receiver = (id<MTLHeap>)object;
    return (void *)[receiver newAccelerationStructureWithSize:size];
}

void * metal_Heap_new_acceleration_structure_with_size_offset(void * object, uint64_t size, uint64_t offset) {
    id<MTLHeap> receiver = (id<MTLHeap>)object;
    return (void *)[receiver newAccelerationStructureWithSize:size offset:offset];
}

void * metal_HeapDescriptor_new(void) {
    return (void *)[MTLHeapDescriptor new];
}

uint64_t metal_HeapDescriptor_cpu_cache_mode(void * object) {
    MTLHeapDescriptor * receiver = (MTLHeapDescriptor *)object;
    return (uint64_t)[receiver cpuCacheMode];
}

void metal_HeapDescriptor_set_cpu_cache_mode(void * object, uint64_t value) {
    MTLHeapDescriptor * receiver = (MTLHeapDescriptor *)object;
    [receiver setCpuCacheMode:value];
}

uint64_t metal_HeapDescriptor_storage_mode(void * object) {
    MTLHeapDescriptor * receiver = (MTLHeapDescriptor *)object;
    return (uint64_t)[receiver storageMode];
}

void metal_HeapDescriptor_set_storage_mode(void * object, uint64_t value) {
    MTLHeapDescriptor * receiver = (MTLHeapDescriptor *)object;
    [receiver setStorageMode:value];
}

uint64_t metal_HeapDescriptor_size(void * object) {
    MTLHeapDescriptor * receiver = (MTLHeapDescriptor *)object;
    return (uint64_t)[receiver size];
}

void metal_HeapDescriptor_set_size(void * object, uint64_t value) {
    MTLHeapDescriptor * receiver = (MTLHeapDescriptor *)object;
    [receiver setSize:value];
}

uint64_t metal_HeapDescriptor_hazard_tracking_mode(void * object) {
    MTLHeapDescriptor * receiver = (MTLHeapDescriptor *)object;
    return (uint64_t)[receiver hazardTrackingMode];
}

void metal_HeapDescriptor_set_hazard_tracking_mode(void * object, uint64_t value) {
    MTLHeapDescriptor * receiver = (MTLHeapDescriptor *)object;
    [receiver setHazardTrackingMode:value];
}

uint64_t metal_HeapDescriptor_heap_type(void * object) {
    MTLHeapDescriptor * receiver = (MTLHeapDescriptor *)object;
    return (uint64_t)[receiver type];
}

void metal_HeapDescriptor_set_heap_type(void * object, uint64_t value) {
    MTLHeapDescriptor * receiver = (MTLHeapDescriptor *)object;
    [receiver setType:value];
}

uint64_t metal_HeapDescriptor_resource_options(void * object) {
    MTLHeapDescriptor * receiver = (MTLHeapDescriptor *)object;
    return (uint64_t)[receiver resourceOptions];
}

void * metal_SamplerDescriptor_new(void) {
    return (void *)[MTLSamplerDescriptor new];
}

void metal_SamplerDescriptor_set_min_filter(void * object, uint64_t value) {
    MTLSamplerDescriptor * receiver = (MTLSamplerDescriptor *)object;
    [receiver setMinFilter:value];
}

void metal_SamplerDescriptor_set_mag_filter(void * object, uint64_t value) {
    MTLSamplerDescriptor * receiver = (MTLSamplerDescriptor *)object;
    [receiver setMagFilter:value];
}

void metal_SamplerDescriptor_set_mip_filter(void * object, uint64_t value) {
    MTLSamplerDescriptor * receiver = (MTLSamplerDescriptor *)object;
    [receiver setMipFilter:value];
}

void metal_SamplerDescriptor_set_address_mode_s(void * object, uint64_t value) {
    MTLSamplerDescriptor * receiver = (MTLSamplerDescriptor *)object;
    [receiver setSAddressMode:value];
}

void metal_SamplerDescriptor_set_address_mode_t(void * object, uint64_t value) {
    MTLSamplerDescriptor * receiver = (MTLSamplerDescriptor *)object;
    [receiver setTAddressMode:value];
}

void metal_SamplerDescriptor_set_address_mode_r(void * object, uint64_t value) {
    MTLSamplerDescriptor * receiver = (MTLSamplerDescriptor *)object;
    [receiver setRAddressMode:value];
}

void metal_SamplerDescriptor_set_max_anisotropy(void * object, uint64_t value) {
    MTLSamplerDescriptor * receiver = (MTLSamplerDescriptor *)object;
    [receiver setMaxAnisotropy:value];
}

void metal_SamplerDescriptor_set_compare_function(void * object, uint64_t value) {
    MTLSamplerDescriptor * receiver = (MTLSamplerDescriptor *)object;
    [receiver setCompareFunction:value];
}

void metal_SamplerDescriptor_set_border_color(void * object, uint64_t value) {
    MTLSamplerDescriptor * receiver = (MTLSamplerDescriptor *)object;
    [receiver setBorderColor:value];
}

void metal_SamplerDescriptor_set_lod_bias(void * object, float value) {
    MTLSamplerDescriptor * receiver = (MTLSamplerDescriptor *)object;
    [receiver setLodBias:value];
}

void metal_SamplerDescriptor_set_lod_min_clamp(void * object, float value) {
    MTLSamplerDescriptor * receiver = (MTLSamplerDescriptor *)object;
    [receiver setLodMinClamp:value];
}

void metal_SamplerDescriptor_set_lod_max_clamp(void * object, float value) {
    MTLSamplerDescriptor * receiver = (MTLSamplerDescriptor *)object;
    [receiver setLodMaxClamp:value];
}

void metal_SamplerDescriptor_set_lod_average(void * object, uint8_t value) {
    MTLSamplerDescriptor * receiver = (MTLSamplerDescriptor *)object;
    [receiver setLodAverage:(value != 0)];
}

void metal_SamplerDescriptor_set_normalized_coordinates(void * object, uint8_t value) {
    MTLSamplerDescriptor * receiver = (MTLSamplerDescriptor *)object;
    [receiver setNormalizedCoordinates:(value != 0)];
}

void metal_SamplerDescriptor_set_support_argument_buffers(void * object, uint8_t value) {
    MTLSamplerDescriptor * receiver = (MTLSamplerDescriptor *)object;
    [receiver setSupportArgumentBuffers:(value != 0)];
}

const uint8_t * metal_SamplerDescriptor_label(void * object) {
    MTLSamplerDescriptor * receiver = (MTLSamplerDescriptor *)object;
    return (const uint8_t *)[[receiver label] UTF8String];
}

void metal_SamplerDescriptor_set_label(void * object, const uint8_t * label) {
    MTLSamplerDescriptor * receiver = (MTLSamplerDescriptor *)object;
    [receiver setLabel:[NSString stringWithUTF8String:(const char *)label]];
}

void * metal_SamplerState_device(void * object) {
    id<MTLSamplerState> receiver = (id<MTLSamplerState>)object;
    return (void *)[receiver device];
}

const uint8_t * metal_SamplerState_label(void * object) {
    id<MTLSamplerState> receiver = (id<MTLSamplerState>)object;
    return (const uint8_t *)[[receiver label] UTF8String];
}

MTLResourceID metal_SamplerState_gpu_resource_id(void * object) {
    id<MTLSamplerState> receiver = (id<MTLSamplerState>)object;
    return [receiver gpuResourceID];
}

void * metal_StencilDescriptor_new(void) {
    return (void *)[MTLStencilDescriptor new];
}

uint64_t metal_StencilDescriptor_stencil_compare_function(void * object) {
    MTLStencilDescriptor * receiver = (MTLStencilDescriptor *)object;
    return (uint64_t)[receiver stencilCompareFunction];
}

void metal_StencilDescriptor_set_stencil_compare_function(void * object, uint64_t value) {
    MTLStencilDescriptor * receiver = (MTLStencilDescriptor *)object;
    [receiver setStencilCompareFunction:value];
}

uint64_t metal_StencilDescriptor_stencil_failure_operation(void * object) {
    MTLStencilDescriptor * receiver = (MTLStencilDescriptor *)object;
    return (uint64_t)[receiver stencilFailureOperation];
}

void metal_StencilDescriptor_set_stencil_failure_operation(void * object, uint64_t value) {
    MTLStencilDescriptor * receiver = (MTLStencilDescriptor *)object;
    [receiver setStencilFailureOperation:value];
}

uint64_t metal_StencilDescriptor_depth_failure_operation(void * object) {
    MTLStencilDescriptor * receiver = (MTLStencilDescriptor *)object;
    return (uint64_t)[receiver depthFailureOperation];
}

void metal_StencilDescriptor_set_depth_failure_operation(void * object, uint64_t value) {
    MTLStencilDescriptor * receiver = (MTLStencilDescriptor *)object;
    [receiver setDepthFailureOperation:value];
}

uint64_t metal_StencilDescriptor_depth_stencil_pass_operation(void * object) {
    MTLStencilDescriptor * receiver = (MTLStencilDescriptor *)object;
    return (uint64_t)[receiver depthStencilPassOperation];
}

void metal_StencilDescriptor_set_depth_stencil_pass_operation(void * object, uint64_t value) {
    MTLStencilDescriptor * receiver = (MTLStencilDescriptor *)object;
    [receiver setDepthStencilPassOperation:value];
}

uint32_t metal_StencilDescriptor_read_mask(void * object) {
    MTLStencilDescriptor * receiver = (MTLStencilDescriptor *)object;
    return (uint32_t)[receiver readMask];
}

void metal_StencilDescriptor_set_read_mask(void * object, uint32_t value) {
    MTLStencilDescriptor * receiver = (MTLStencilDescriptor *)object;
    [receiver setReadMask:value];
}

uint32_t metal_StencilDescriptor_write_mask(void * object) {
    MTLStencilDescriptor * receiver = (MTLStencilDescriptor *)object;
    return (uint32_t)[receiver writeMask];
}

void metal_StencilDescriptor_set_write_mask(void * object, uint32_t value) {
    MTLStencilDescriptor * receiver = (MTLStencilDescriptor *)object;
    [receiver setWriteMask:value];
}

void * metal_DepthStencilDescriptor_new(void) {
    return (void *)[MTLDepthStencilDescriptor new];
}

uint64_t metal_DepthStencilDescriptor_depth_compare_function(void * object) {
    MTLDepthStencilDescriptor * receiver = (MTLDepthStencilDescriptor *)object;
    return (uint64_t)[receiver depthCompareFunction];
}

void metal_DepthStencilDescriptor_set_depth_compare_function(void * object, uint64_t value) {
    MTLDepthStencilDescriptor * receiver = (MTLDepthStencilDescriptor *)object;
    [receiver setDepthCompareFunction:value];
}

uint8_t metal_DepthStencilDescriptor_depth_write_enabled(void * object) {
    MTLDepthStencilDescriptor * receiver = (MTLDepthStencilDescriptor *)object;
    return [receiver isDepthWriteEnabled] ? 1 : 0;
}

void metal_DepthStencilDescriptor_set_depth_write_enabled(void * object, uint8_t value) {
    MTLDepthStencilDescriptor * receiver = (MTLDepthStencilDescriptor *)object;
    [receiver setDepthWriteEnabled:(value != 0)];
}

void * metal_DepthStencilDescriptor_front_face_stencil(void * object) {
    MTLDepthStencilDescriptor * receiver = (MTLDepthStencilDescriptor *)object;
    return (void *)[receiver frontFaceStencil];
}

void metal_DepthStencilDescriptor_set_front_face_stencil(void * object, void * value) {
    MTLDepthStencilDescriptor * receiver = (MTLDepthStencilDescriptor *)object;
    [receiver setFrontFaceStencil:(id)value];
}

void * metal_DepthStencilDescriptor_back_face_stencil(void * object) {
    MTLDepthStencilDescriptor * receiver = (MTLDepthStencilDescriptor *)object;
    return (void *)[receiver backFaceStencil];
}

void metal_DepthStencilDescriptor_set_back_face_stencil(void * object, void * value) {
    MTLDepthStencilDescriptor * receiver = (MTLDepthStencilDescriptor *)object;
    [receiver setBackFaceStencil:(id)value];
}

const uint8_t * metal_DepthStencilDescriptor_label(void * object) {
    MTLDepthStencilDescriptor * receiver = (MTLDepthStencilDescriptor *)object;
    return (const uint8_t *)[[receiver label] UTF8String];
}

void metal_DepthStencilDescriptor_set_label(void * object, const uint8_t * label) {
    MTLDepthStencilDescriptor * receiver = (MTLDepthStencilDescriptor *)object;
    [receiver setLabel:[NSString stringWithUTF8String:(const char *)label]];
}

void * metal_DepthStencilState_device(void * object) {
    id<MTLDepthStencilState> receiver = (id<MTLDepthStencilState>)object;
    return (void *)[receiver device];
}

const uint8_t * metal_DepthStencilState_label(void * object) {
    id<MTLDepthStencilState> receiver = (id<MTLDepthStencilState>)object;
    return (const uint8_t *)[[receiver label] UTF8String];
}

void * metal_VertexBufferLayoutDescriptor_new(void) {
    return (void *)[MTLVertexBufferLayoutDescriptor new];
}

uint64_t metal_VertexBufferLayoutDescriptor_stride(void * object) {
    MTLVertexBufferLayoutDescriptor * receiver = (MTLVertexBufferLayoutDescriptor *)object;
    return (uint64_t)[receiver stride];
}

void metal_VertexBufferLayoutDescriptor_set_stride(void * object, uint64_t value) {
    MTLVertexBufferLayoutDescriptor * receiver = (MTLVertexBufferLayoutDescriptor *)object;
    [receiver setStride:value];
}

uint64_t metal_VertexBufferLayoutDescriptor_step_function(void * object) {
    MTLVertexBufferLayoutDescriptor * receiver = (MTLVertexBufferLayoutDescriptor *)object;
    return (uint64_t)[receiver stepFunction];
}

void metal_VertexBufferLayoutDescriptor_set_step_function(void * object, uint64_t value) {
    MTLVertexBufferLayoutDescriptor * receiver = (MTLVertexBufferLayoutDescriptor *)object;
    [receiver setStepFunction:value];
}

uint64_t metal_VertexBufferLayoutDescriptor_step_rate(void * object) {
    MTLVertexBufferLayoutDescriptor * receiver = (MTLVertexBufferLayoutDescriptor *)object;
    return (uint64_t)[receiver stepRate];
}

void metal_VertexBufferLayoutDescriptor_set_step_rate(void * object, uint64_t value) {
    MTLVertexBufferLayoutDescriptor * receiver = (MTLVertexBufferLayoutDescriptor *)object;
    [receiver setStepRate:value];
}

void * metal_VertexAttributeDescriptor_new(void) {
    return (void *)[MTLVertexAttributeDescriptor new];
}

uint64_t metal_VertexAttributeDescriptor_format(void * object) {
    MTLVertexAttributeDescriptor * receiver = (MTLVertexAttributeDescriptor *)object;
    return (uint64_t)[receiver format];
}

void metal_VertexAttributeDescriptor_set_format(void * object, uint64_t value) {
    MTLVertexAttributeDescriptor * receiver = (MTLVertexAttributeDescriptor *)object;
    [receiver setFormat:value];
}

uint64_t metal_VertexAttributeDescriptor_offset(void * object) {
    MTLVertexAttributeDescriptor * receiver = (MTLVertexAttributeDescriptor *)object;
    return (uint64_t)[receiver offset];
}

void metal_VertexAttributeDescriptor_set_offset(void * object, uint64_t value) {
    MTLVertexAttributeDescriptor * receiver = (MTLVertexAttributeDescriptor *)object;
    [receiver setOffset:value];
}

uint64_t metal_VertexAttributeDescriptor_buffer_index(void * object) {
    MTLVertexAttributeDescriptor * receiver = (MTLVertexAttributeDescriptor *)object;
    return (uint64_t)[receiver bufferIndex];
}

void metal_VertexAttributeDescriptor_set_buffer_index(void * object, uint64_t value) {
    MTLVertexAttributeDescriptor * receiver = (MTLVertexAttributeDescriptor *)object;
    [receiver setBufferIndex:value];
}

void * metal_VertexBufferLayoutDescriptorArray_object_at(void * object, uint64_t index) {
    MTLVertexBufferLayoutDescriptorArray * receiver = (MTLVertexBufferLayoutDescriptorArray *)object;
    return (void *)[receiver objectAtIndexedSubscript:index];
}

void metal_VertexBufferLayoutDescriptorArray_set_object_at(void * object, uint64_t index, void * layout) {
    MTLVertexBufferLayoutDescriptorArray * receiver = (MTLVertexBufferLayoutDescriptorArray *)object;
    [receiver setObject:(id)layout atIndexedSubscript:index];
}

void * metal_VertexAttributeDescriptorArray_object_at(void * object, uint64_t index) {
    MTLVertexAttributeDescriptorArray * receiver = (MTLVertexAttributeDescriptorArray *)object;
    return (void *)[receiver objectAtIndexedSubscript:index];
}

void metal_VertexAttributeDescriptorArray_set_object_at(void * object, uint64_t index, void * attribute) {
    MTLVertexAttributeDescriptorArray * receiver = (MTLVertexAttributeDescriptorArray *)object;
    [receiver setObject:(id)attribute atIndexedSubscript:index];
}

void * metal_VertexDescriptor_new(void) {
    return (void *)[[MTLVertexDescriptor vertexDescriptor] retain];
}

void * metal_VertexDescriptor_layouts(void * object) {
    MTLVertexDescriptor * receiver = (MTLVertexDescriptor *)object;
    return (void *)[receiver layouts];
}

void * metal_VertexDescriptor_attributes(void * object) {
    MTLVertexDescriptor * receiver = (MTLVertexDescriptor *)object;
    return (void *)[receiver attributes];
}

void * metal_VertexDescriptor_serialize_descriptor(void * object) {
    MTLVertexDescriptor * receiver = (MTLVertexDescriptor *)object;
    return (void *)[receiver newSerializedDescriptor];
}

void metal_VertexDescriptor_reset(void * object) {
    MTLVertexDescriptor * receiver = (MTLVertexDescriptor *)object;
    [receiver reset];
}
