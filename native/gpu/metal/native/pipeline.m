// SPDX-License-Identifier: MIT OR Apache-2.0
// Port of metal-rs; Copyright 2016-2026 GFX developers.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <objc/message.h>
#include <stdint.h>
#include "types.h"

void * metal_PipelineBufferDescriptorArray_object_at(void * object, uint64_t index) {
    return (void *)[((MTLPipelineBufferDescriptorArray *)object) objectAtIndexedSubscript:index];
}

void metal_PipelineBufferDescriptorArray_set_object_at(void * object, uint64_t index, void * buffer_desc) {
    [((MTLPipelineBufferDescriptorArray *)object) setObject:(id)buffer_desc atIndexedSubscript:index];
}

uint64_t metal_PipelineBufferDescriptor_mutability(void * object) {
    return (uint64_t)[((MTLPipelineBufferDescriptor *)object) mutability];
}

void metal_PipelineBufferDescriptor_set_mutability(void * object, uint64_t new_mutability) {
    [((MTLPipelineBufferDescriptor *)object) setMutability:new_mutability];
}

uint64_t metal_RenderPipelineColorAttachmentDescriptor_pixel_format(void * object) {
    return (uint64_t)[((MTLRenderPipelineColorAttachmentDescriptor *)object) pixelFormat];
}

void metal_RenderPipelineColorAttachmentDescriptor_set_pixel_format(void * object, uint64_t pixel_format) {
    [((MTLRenderPipelineColorAttachmentDescriptor *)object) setPixelFormat:pixel_format];
}

uint8_t metal_RenderPipelineColorAttachmentDescriptor_is_blending_enabled(void * object) {
    return [((MTLRenderPipelineColorAttachmentDescriptor *)object) isBlendingEnabled] ? 1 : 0;
}

void metal_RenderPipelineColorAttachmentDescriptor_set_blending_enabled(void * object, uint8_t enabled) {
    [((MTLRenderPipelineColorAttachmentDescriptor *)object) setBlendingEnabled:(enabled != 0)];
}

uint64_t metal_RenderPipelineColorAttachmentDescriptor_source_rgb_blend_factor(void * object) {
    return (uint64_t)[((MTLRenderPipelineColorAttachmentDescriptor *)object) sourceRGBBlendFactor];
}

void metal_RenderPipelineColorAttachmentDescriptor_set_source_rgb_blend_factor(void * object, uint64_t blend_factor) {
    [((MTLRenderPipelineColorAttachmentDescriptor *)object) setSourceRGBBlendFactor:blend_factor];
}

uint64_t metal_RenderPipelineColorAttachmentDescriptor_destination_rgb_blend_factor(void * object) {
    return (uint64_t)[((MTLRenderPipelineColorAttachmentDescriptor *)object) destinationRGBBlendFactor];
}

void metal_RenderPipelineColorAttachmentDescriptor_set_destination_rgb_blend_factor(void * object, uint64_t blend_factor) {
    [((MTLRenderPipelineColorAttachmentDescriptor *)object) setDestinationRGBBlendFactor:blend_factor];
}

uint64_t metal_RenderPipelineColorAttachmentDescriptor_rgb_blend_operation(void * object) {
    return (uint64_t)[((MTLRenderPipelineColorAttachmentDescriptor *)object) rgbBlendOperation];
}

void metal_RenderPipelineColorAttachmentDescriptor_set_rgb_blend_operation(void * object, uint64_t blend_operation) {
    [((MTLRenderPipelineColorAttachmentDescriptor *)object) setRgbBlendOperation:blend_operation];
}

uint64_t metal_RenderPipelineColorAttachmentDescriptor_source_alpha_blend_factor(void * object) {
    return (uint64_t)[((MTLRenderPipelineColorAttachmentDescriptor *)object) sourceAlphaBlendFactor];
}

void metal_RenderPipelineColorAttachmentDescriptor_set_source_alpha_blend_factor(void * object, uint64_t blend_factor) {
    [((MTLRenderPipelineColorAttachmentDescriptor *)object) setSourceAlphaBlendFactor:blend_factor];
}

uint64_t metal_RenderPipelineColorAttachmentDescriptor_destination_alpha_blend_factor(void * object) {
    return (uint64_t)[((MTLRenderPipelineColorAttachmentDescriptor *)object) destinationAlphaBlendFactor];
}

void metal_RenderPipelineColorAttachmentDescriptor_set_destination_alpha_blend_factor(void * object, uint64_t blend_factor) {
    [((MTLRenderPipelineColorAttachmentDescriptor *)object) setDestinationAlphaBlendFactor:blend_factor];
}

uint64_t metal_RenderPipelineColorAttachmentDescriptor_alpha_blend_operation(void * object) {
    return (uint64_t)[((MTLRenderPipelineColorAttachmentDescriptor *)object) alphaBlendOperation];
}

void metal_RenderPipelineColorAttachmentDescriptor_set_alpha_blend_operation(void * object, uint64_t blend_operation) {
    [((MTLRenderPipelineColorAttachmentDescriptor *)object) setAlphaBlendOperation:blend_operation];
}

uint64_t metal_RenderPipelineColorAttachmentDescriptor_write_mask(void * object) {
    return (uint64_t)[((MTLRenderPipelineColorAttachmentDescriptor *)object) writeMask];
}

void metal_RenderPipelineColorAttachmentDescriptor_set_write_mask(void * object, uint64_t mask) {
    [((MTLRenderPipelineColorAttachmentDescriptor *)object) setWriteMask:mask];
}

void * metal_RenderPipelineReflection_new(void * vertex_data, void * fragment_data, void * vertex_desc, void * device, uint64_t options, uint64_t flags) {
    return (void *)((id (*)(id, SEL, void *, void *, void *, id, uint64_t, uint64_t))objc_msgSend)([MTLRenderPipelineReflection alloc], sel_registerName("initWithVertexData:fragmentData:serializedVertexDescriptor:device:options:flags:"), vertex_data, fragment_data, vertex_desc, (id)device, options, flags);
}

void * metal_RenderPipelineReflection_fragment_arguments(void * object) {
    return (void *)[((MTLRenderPipelineReflection *)object) fragmentArguments];
}

void * metal_RenderPipelineReflection_vertex_arguments(void * object) {
    return (void *)[((MTLRenderPipelineReflection *)object) vertexArguments];
}

void * metal_RenderPipelineReflection_tile_arguments(void * object) {
    return (void *)[((MTLRenderPipelineReflection *)object) tileArguments];
}

void * metal_ArgumentArray_object_at(void * object, uint64_t index) {
    return (void *)[((NSArray *)object) objectAtIndexedSubscript:index];
}

uint64_t metal_ArgumentArray_count(void * object) {
    return (uint64_t)[((NSArray *)object) count];
}

void * metal_ComputePipelineReflection_arguments(void * object) {
    return (void *)[((MTLComputePipelineReflection *)object) arguments];
}

void * metal_MeshRenderPipelineDescriptor_new(void) {
    return (void *)[MTLMeshRenderPipelineDescriptor new];
}

void * metal_MeshRenderPipelineDescriptor_color_attachments(void * object) {
    return (void *)[((MTLMeshRenderPipelineDescriptor *)object) colorAttachments];
}

uint64_t metal_MeshRenderPipelineDescriptor_depth_attachment_pixel_format(void * object) {
    return (uint64_t)[((MTLMeshRenderPipelineDescriptor *)object) depthAttachmentPixelFormat];
}

void metal_MeshRenderPipelineDescriptor_set_depth_attachment_pixel_format(void * object, uint64_t pixel_format) {
    [((MTLMeshRenderPipelineDescriptor *)object) setDepthAttachmentPixelFormat:pixel_format];
}

void * metal_MeshRenderPipelineDescriptor_fragment_buffers(void * object) {
    return (void *)[((MTLMeshRenderPipelineDescriptor *)object) fragmentBuffers];
}

void * metal_MeshRenderPipelineDescriptor_fragment_function(void * object) {
    return (void *)[((MTLMeshRenderPipelineDescriptor *)object) fragmentFunction];
}

void metal_MeshRenderPipelineDescriptor_set_fragment_function(void * object, void * function) {
    [((MTLMeshRenderPipelineDescriptor *)object) setFragmentFunction:(id)function];
}

uint8_t metal_MeshRenderPipelineDescriptor_is_alpha_to_coverage_enabled(void * object) {
    return [((MTLMeshRenderPipelineDescriptor *)object) isAlphaToCoverageEnabled] ? 1 : 0;
}

void metal_MeshRenderPipelineDescriptor_set_alpha_to_coverage_enabled(void * object, uint8_t enabled) {
    [((MTLMeshRenderPipelineDescriptor *)object) setAlphaToCoverageEnabled:(enabled != 0)];
}

uint8_t metal_MeshRenderPipelineDescriptor_is_alpha_to_one_enabled(void * object) {
    return [((MTLMeshRenderPipelineDescriptor *)object) isAlphaToOneEnabled] ? 1 : 0;
}

void metal_MeshRenderPipelineDescriptor_set_alpha_to_one_enabled(void * object, uint8_t enabled) {
    [((MTLMeshRenderPipelineDescriptor *)object) setAlphaToOneEnabled:(enabled != 0)];
}

uint8_t metal_MeshRenderPipelineDescriptor_is_rasterization_enabled(void * object) {
    return [((MTLMeshRenderPipelineDescriptor *)object) isRasterizationEnabled] ? 1 : 0;
}

void metal_MeshRenderPipelineDescriptor_set_rasterization_enabled(void * object, uint8_t enabled) {
    [((MTLMeshRenderPipelineDescriptor *)object) setRasterizationEnabled:(enabled != 0)];
}

const char * metal_MeshRenderPipelineDescriptor_label(void * object) {
    return (const char *)[[((MTLMeshRenderPipelineDescriptor *)object) label] UTF8String];
}

void metal_MeshRenderPipelineDescriptor_set_label(void * object, const char * label) {
    NSString *text = label ? [[NSString alloc] initWithUTF8String:label] : nil;
    [((MTLMeshRenderPipelineDescriptor *)object) setLabel:text];
    [text release];
}

uint64_t metal_MeshRenderPipelineDescriptor_max_total_threadgroups_per_mesh_grid(void * object) {
    return (uint64_t)[((MTLMeshRenderPipelineDescriptor *)object) maxTotalThreadgroupsPerMeshGrid];
}

void metal_MeshRenderPipelineDescriptor_set_max_total_threadgroups_per_mesh_grid(void * object, uint64_t max_total_threadgroups_per_mesh_grid) {
    [((MTLMeshRenderPipelineDescriptor *)object) setMaxTotalThreadgroupsPerMeshGrid:max_total_threadgroups_per_mesh_grid];
}

uint64_t metal_MeshRenderPipelineDescriptor_max_total_threads_per_mesh_threadgroup(void * object) {
    return (uint64_t)[((MTLMeshRenderPipelineDescriptor *)object) maxTotalThreadsPerMeshThreadgroup];
}

void metal_MeshRenderPipelineDescriptor_set_max_total_threads_per_mesh_threadgroup(void * object, uint64_t max_total_threads_per_mesh_threadgroup) {
    [((MTLMeshRenderPipelineDescriptor *)object) setMaxTotalThreadsPerMeshThreadgroup:max_total_threads_per_mesh_threadgroup];
}

uint64_t metal_MeshRenderPipelineDescriptor_max_total_threads_per_object_threadgroup(void * object) {
    return (uint64_t)[((MTLMeshRenderPipelineDescriptor *)object) maxTotalThreadsPerObjectThreadgroup];
}

void metal_MeshRenderPipelineDescriptor_set_max_total_threads_per_object_threadgroup(void * object, uint64_t max_total_threads_per_object_threadgroup) {
    [((MTLMeshRenderPipelineDescriptor *)object) setMaxTotalThreadsPerObjectThreadgroup:max_total_threads_per_object_threadgroup];
}

uint64_t metal_MeshRenderPipelineDescriptor_max_vertex_amplification_count(void * object) {
    return (uint64_t)[((MTLMeshRenderPipelineDescriptor *)object) maxVertexAmplificationCount];
}

void metal_MeshRenderPipelineDescriptor_set_max_vertex_amplification_count(void * object, uint64_t max_vertex_amplification_count) {
    [((MTLMeshRenderPipelineDescriptor *)object) setMaxVertexAmplificationCount:max_vertex_amplification_count];
}

void * metal_MeshRenderPipelineDescriptor_mesh_buffers(void * object) {
    return (void *)[((MTLMeshRenderPipelineDescriptor *)object) meshBuffers];
}

void * metal_MeshRenderPipelineDescriptor_mesh_function(void * object) {
    return (void *)[((MTLMeshRenderPipelineDescriptor *)object) meshFunction];
}

void metal_MeshRenderPipelineDescriptor_set_mesh_function(void * object, void * function) {
    [((MTLMeshRenderPipelineDescriptor *)object) setMeshFunction:(id)function];
}

uint8_t metal_MeshRenderPipelineDescriptor_mesh_threadgroup_size_is_multiple_of_thread_execution_width(void * object) {
    return [((MTLMeshRenderPipelineDescriptor *)object) meshThreadgroupSizeIsMultipleOfThreadExecutionWidth] ? 1 : 0;
}

void metal_MeshRenderPipelineDescriptor_set_mesh_threadgroup_size_is_multiple_of_thread_execution_width(void * object, uint8_t mesh_threadgroup_size_is_multiple_of_thread_execution_width) {
    [((MTLMeshRenderPipelineDescriptor *)object) setMeshThreadgroupSizeIsMultipleOfThreadExecutionWidth:(mesh_threadgroup_size_is_multiple_of_thread_execution_width != 0)];
}

void * metal_MeshRenderPipelineDescriptor_object_buffers(void * object) {
    return (void *)[((MTLMeshRenderPipelineDescriptor *)object) objectBuffers];
}

void * metal_MeshRenderPipelineDescriptor_object_function(void * object) {
    return (void *)[((MTLMeshRenderPipelineDescriptor *)object) objectFunction];
}

void metal_MeshRenderPipelineDescriptor_set_object_function(void * object, void * function) {
    [((MTLMeshRenderPipelineDescriptor *)object) setObjectFunction:(id)function];
}

uint8_t metal_MeshRenderPipelineDescriptor_object_threadgroup_size_is_multiple_of_thread_execution_width(void * object) {
    return [((MTLMeshRenderPipelineDescriptor *)object) objectThreadgroupSizeIsMultipleOfThreadExecutionWidth] ? 1 : 0;
}

void metal_MeshRenderPipelineDescriptor_set_object_threadgroup_size_is_multiple_of_thread_execution_width(void * object, uint8_t object_threadgroup_size_is_multiple_of_thread_execution_width) {
    [((MTLMeshRenderPipelineDescriptor *)object) setObjectThreadgroupSizeIsMultipleOfThreadExecutionWidth:(object_threadgroup_size_is_multiple_of_thread_execution_width != 0)];
}

uint64_t metal_MeshRenderPipelineDescriptor_payload_memory_length(void * object) {
    return (uint64_t)[((MTLMeshRenderPipelineDescriptor *)object) payloadMemoryLength];
}

void metal_MeshRenderPipelineDescriptor_set_payload_memory_length(void * object, uint64_t payload_memory_length) {
    [((MTLMeshRenderPipelineDescriptor *)object) setPayloadMemoryLength:payload_memory_length];
}

uint64_t metal_MeshRenderPipelineDescriptor_raster_sample_count(void * object) {
    return (uint64_t)[((MTLMeshRenderPipelineDescriptor *)object) rasterSampleCount];
}

void metal_MeshRenderPipelineDescriptor_set_raster_sample_count(void * object, uint64_t raster_sample_count) {
    [((MTLMeshRenderPipelineDescriptor *)object) setRasterSampleCount:raster_sample_count];
}

uint64_t metal_MeshRenderPipelineDescriptor_stencil_attachment_pixel_format(void * object) {
    return (uint64_t)[((MTLMeshRenderPipelineDescriptor *)object) stencilAttachmentPixelFormat];
}

void metal_MeshRenderPipelineDescriptor_set_stencil_attachment_pixel_format(void * object, uint64_t pixel_format) {
    [((MTLMeshRenderPipelineDescriptor *)object) setStencilAttachmentPixelFormat:pixel_format];
}

void metal_MeshRenderPipelineDescriptor_reset(void * object) {
    [((MTLMeshRenderPipelineDescriptor *)object) reset];
}

void * metal_RenderPipelineDescriptor_new(void) {
    return (void *)[MTLRenderPipelineDescriptor new];
}

const char * metal_RenderPipelineDescriptor_label(void * object) {
    return (const char *)[[((MTLRenderPipelineDescriptor *)object) label] UTF8String];
}

void metal_RenderPipelineDescriptor_set_label(void * object, const char * label) {
    NSString *text = label ? [[NSString alloc] initWithUTF8String:label] : nil;
    [((MTLRenderPipelineDescriptor *)object) setLabel:text];
    [text release];
}

void * metal_RenderPipelineDescriptor_vertex_function(void * object) {
    return (void *)[((MTLRenderPipelineDescriptor *)object) vertexFunction];
}

void metal_RenderPipelineDescriptor_set_vertex_function(void * object, void * function) {
    [((MTLRenderPipelineDescriptor *)object) setVertexFunction:(id)function];
}

void * metal_RenderPipelineDescriptor_fragment_function(void * object) {
    return (void *)[((MTLRenderPipelineDescriptor *)object) fragmentFunction];
}

void metal_RenderPipelineDescriptor_set_fragment_function(void * object, void * function) {
    [((MTLRenderPipelineDescriptor *)object) setFragmentFunction:(id)function];
}

void * metal_RenderPipelineDescriptor_vertex_descriptor(void * object) {
    return (void *)[((MTLRenderPipelineDescriptor *)object) vertexDescriptor];
}

void metal_RenderPipelineDescriptor_set_vertex_descriptor(void * object, void * descriptor) {
    [((MTLRenderPipelineDescriptor *)object) setVertexDescriptor:(id)descriptor];
}

uint64_t metal_RenderPipelineDescriptor_sample_count(void * object) {
    return (uint64_t)[((MTLRenderPipelineDescriptor *)object) sampleCount];
}

void metal_RenderPipelineDescriptor_set_sample_count(void * object, uint64_t count) {
    [((MTLRenderPipelineDescriptor *)object) setSampleCount:count];
}

uint64_t metal_RenderPipelineDescriptor_raster_sample_count(void * object) {
    return (uint64_t)[((MTLRenderPipelineDescriptor *)object) rasterSampleCount];
}

void metal_RenderPipelineDescriptor_set_raster_sample_count(void * object, uint64_t count) {
    [((MTLRenderPipelineDescriptor *)object) setRasterSampleCount:count];
}

uint64_t metal_RenderPipelineDescriptor_max_vertex_amplification_count(void * object) {
    return (uint64_t)[((MTLRenderPipelineDescriptor *)object) maxVertexAmplificationCount];
}

void metal_RenderPipelineDescriptor_set_max_vertex_amplification_count(void * object, uint64_t count) {
    [((MTLRenderPipelineDescriptor *)object) setMaxVertexAmplificationCount:count];
}

uint8_t metal_RenderPipelineDescriptor_is_alpha_to_coverage_enabled(void * object) {
    return [((MTLRenderPipelineDescriptor *)object) isAlphaToCoverageEnabled] ? 1 : 0;
}

void metal_RenderPipelineDescriptor_set_alpha_to_coverage_enabled(void * object, uint8_t enabled) {
    [((MTLRenderPipelineDescriptor *)object) setAlphaToCoverageEnabled:(enabled != 0)];
}

uint8_t metal_RenderPipelineDescriptor_is_alpha_to_one_enabled(void * object) {
    return [((MTLRenderPipelineDescriptor *)object) isAlphaToOneEnabled] ? 1 : 0;
}

void metal_RenderPipelineDescriptor_set_alpha_to_one_enabled(void * object, uint8_t enabled) {
    [((MTLRenderPipelineDescriptor *)object) setAlphaToOneEnabled:(enabled != 0)];
}

uint8_t metal_RenderPipelineDescriptor_is_rasterization_enabled(void * object) {
    return [((MTLRenderPipelineDescriptor *)object) isRasterizationEnabled] ? 1 : 0;
}

void metal_RenderPipelineDescriptor_set_rasterization_enabled(void * object, uint8_t enabled) {
    [((MTLRenderPipelineDescriptor *)object) setRasterizationEnabled:(enabled != 0)];
}

void * metal_RenderPipelineDescriptor_color_attachments(void * object) {
    return (void *)[((MTLRenderPipelineDescriptor *)object) colorAttachments];
}

uint64_t metal_RenderPipelineDescriptor_depth_attachment_pixel_format(void * object) {
    return (uint64_t)[((MTLRenderPipelineDescriptor *)object) depthAttachmentPixelFormat];
}

void metal_RenderPipelineDescriptor_set_depth_attachment_pixel_format(void * object, uint64_t pixel_format) {
    [((MTLRenderPipelineDescriptor *)object) setDepthAttachmentPixelFormat:pixel_format];
}

uint64_t metal_RenderPipelineDescriptor_stencil_attachment_pixel_format(void * object) {
    return (uint64_t)[((MTLRenderPipelineDescriptor *)object) stencilAttachmentPixelFormat];
}

void metal_RenderPipelineDescriptor_set_stencil_attachment_pixel_format(void * object, uint64_t pixel_format) {
    [((MTLRenderPipelineDescriptor *)object) setStencilAttachmentPixelFormat:pixel_format];
}

uint64_t metal_RenderPipelineDescriptor_input_primitive_topology(void * object) {
    return (uint64_t)[((MTLRenderPipelineDescriptor *)object) inputPrimitiveTopology];
}

void metal_RenderPipelineDescriptor_set_input_primitive_topology(void * object, uint64_t topology) {
    [((MTLRenderPipelineDescriptor *)object) setInputPrimitiveTopology:topology];
}

void * metal_RenderPipelineDescriptor_serialize_vertex_data(void * object, void ** error) {
    return (void *)((id (*)(id, SEL, uint64_t, NSError **))objc_msgSend)((id)object, sel_registerName("newSerializedVertexDataWithFlags:error:"), 0, (NSError **)error);
}

void * metal_RenderPipelineDescriptor_serialize_fragment_data(void * object) {
    return (void *)((id (*)(id, SEL))objc_msgSend)((id)object, sel_registerName("serializeFragmentData"));
}

uint8_t metal_RenderPipelineDescriptor_support_indirect_command_buffers(void * object) {
    return [((MTLRenderPipelineDescriptor *)object) supportIndirectCommandBuffers] ? 1 : 0;
}

void metal_RenderPipelineDescriptor_set_support_indirect_command_buffers(void * object, uint8_t support) {
    [((MTLRenderPipelineDescriptor *)object) setSupportIndirectCommandBuffers:(support != 0)];
}

void * metal_RenderPipelineDescriptor_vertex_buffers(void * object) {
    return (void *)[((MTLRenderPipelineDescriptor *)object) vertexBuffers];
}

void * metal_RenderPipelineDescriptor_fragment_buffers(void * object) {
    return (void *)[((MTLRenderPipelineDescriptor *)object) fragmentBuffers];
}

void * metal_RenderPipelineDescriptor_binary_archives(void * object) {
    return (void *)[((MTLRenderPipelineDescriptor *)object) binaryArchives];
}

void metal_RenderPipelineDescriptor_set_binary_archives(void * object, void * const * archives, uint64_t archives_count) {
    NSArray *items = [[NSArray alloc] initWithObjects:(id const *)archives count:(NSUInteger)archives_count];
    [((MTLRenderPipelineDescriptor *)object) setBinaryArchives:items];
    [items release];
}

void * metal_RenderPipelineDescriptor_fragment_linked_functions(void * object) {
    return (void *)[((MTLRenderPipelineDescriptor *)object) fragmentLinkedFunctions];
}

void metal_RenderPipelineDescriptor_set_fragment_linked_functions(void * object, void * functions) {
    [((MTLRenderPipelineDescriptor *)object) setFragmentLinkedFunctions:(id)functions];
}

void metal_RenderPipelineDescriptor_reset(void * object) {
    [((MTLRenderPipelineDescriptor *)object) reset];
}

void * metal_RenderPipelineState_device(void * object) {
    return (void *)[((id<MTLRenderPipelineState>)object) device];
}

const char * metal_RenderPipelineState_label(void * object) {
    return (const char *)[[((id<MTLRenderPipelineState>)object) label] UTF8String];
}

void * metal_RenderPipelineState_new_intersection_function_table_with_descriptor(void * object, void * descriptor, uint64_t stage) {
    return (void *)[((id<MTLRenderPipelineState>)object) newIntersectionFunctionTableWithDescriptor:(id)descriptor stage:stage];
}

void * metal_RenderPipelineState_function_handle_with_function(void * object, void * function, uint64_t stage) {
    return (void *)[((id<MTLRenderPipelineState>)object) functionHandleWithFunction:(id)function stage:stage];
}

void * metal_RenderPipelineState_new_visible_function_table_with_descriptor(void * object, void * descriptor, uint64_t stage) {
    return (void *)[((id<MTLRenderPipelineState>)object) newVisibleFunctionTableWithDescriptor:(id)descriptor stage:stage];
}

void * metal_RenderPipelineColorAttachmentDescriptorArray_object_at(void * object, uint64_t index) {
    return (void *)[((MTLRenderPipelineColorAttachmentDescriptorArray *)object) objectAtIndexedSubscript:index];
}

void metal_RenderPipelineColorAttachmentDescriptorArray_set_object_at(void * object, uint64_t index, void * attachment) {
    [((MTLRenderPipelineColorAttachmentDescriptorArray *)object) setObject:(id)attachment atIndexedSubscript:index];
}

void * metal_ComputePipelineDescriptor_new(void) {
    return (void *)[MTLComputePipelineDescriptor new];
}

const char * metal_ComputePipelineDescriptor_label(void * object) {
    return (const char *)[[((MTLComputePipelineDescriptor *)object) label] UTF8String];
}

void metal_ComputePipelineDescriptor_set_label(void * object, const char * label) {
    NSString *text = label ? [[NSString alloc] initWithUTF8String:label] : nil;
    [((MTLComputePipelineDescriptor *)object) setLabel:text];
    [text release];
}

void * metal_ComputePipelineDescriptor_compute_function(void * object) {
    return (void *)[((MTLComputePipelineDescriptor *)object) computeFunction];
}

void metal_ComputePipelineDescriptor_set_compute_function(void * object, void * function) {
    [((MTLComputePipelineDescriptor *)object) setComputeFunction:(id)function];
}

uint8_t metal_ComputePipelineDescriptor_thread_group_size_is_multiple_of_thread_execution_width(void * object) {
    return [((MTLComputePipelineDescriptor *)object) threadGroupSizeIsMultipleOfThreadExecutionWidth] ? 1 : 0;
}

void metal_ComputePipelineDescriptor_set_thread_group_size_is_multiple_of_thread_execution_width(void * object, uint8_t size_is_multiple_of_width) {
    [((MTLComputePipelineDescriptor *)object) setThreadGroupSizeIsMultipleOfThreadExecutionWidth:(size_is_multiple_of_width != 0)];
}

uint64_t metal_ComputePipelineDescriptor_max_total_threads_per_threadgroup(void * object) {
    return (uint64_t)[((MTLComputePipelineDescriptor *)object) maxTotalThreadsPerThreadgroup];
}

void metal_ComputePipelineDescriptor_set_max_total_threads_per_threadgroup(void * object, uint64_t max_total_threads) {
    [((MTLComputePipelineDescriptor *)object) setMaxTotalThreadsPerThreadgroup:max_total_threads];
}

uint8_t metal_ComputePipelineDescriptor_support_indirect_command_buffers(void * object) {
    return [((MTLComputePipelineDescriptor *)object) supportIndirectCommandBuffers] ? 1 : 0;
}

void metal_ComputePipelineDescriptor_set_support_indirect_command_buffers(void * object, uint8_t support) {
    [((MTLComputePipelineDescriptor *)object) setSupportIndirectCommandBuffers:(support != 0)];
}

uint8_t metal_ComputePipelineDescriptor_support_adding_binary_functions(void * object) {
    return [((MTLComputePipelineDescriptor *)object) supportAddingBinaryFunctions] ? 1 : 0;
}

void metal_ComputePipelineDescriptor_set_support_adding_binary_functions(void * object, uint8_t support) {
    [((MTLComputePipelineDescriptor *)object) setSupportAddingBinaryFunctions:(support != 0)];
}

uint64_t metal_ComputePipelineDescriptor_max_call_stack_depth(void * object) {
    return (uint64_t)[((MTLComputePipelineDescriptor *)object) maxCallStackDepth];
}

void metal_ComputePipelineDescriptor_set_max_call_stack_depth(void * object, uint64_t depth) {
    [((MTLComputePipelineDescriptor *)object) setMaxCallStackDepth:depth];
}

void * metal_ComputePipelineDescriptor_insert_libraries(void * object) {
    return (void *)[((MTLComputePipelineDescriptor *)object) insertLibraries];
}

void metal_ComputePipelineDescriptor_set_insert_libraries(void * object, void * const * libraries, uint64_t libraries_count) {
    NSArray *items = [[NSArray alloc] initWithObjects:(id const *)libraries count:(NSUInteger)libraries_count];
    [((MTLComputePipelineDescriptor *)object) setInsertLibraries:items];
    [items release];
}

void * metal_ComputePipelineDescriptor_binary_archives(void * object) {
    return (void *)[((MTLComputePipelineDescriptor *)object) binaryArchives];
}

void metal_ComputePipelineDescriptor_set_binary_archives(void * object, void * const * archives, uint64_t archives_count) {
    NSArray *items = [[NSArray alloc] initWithObjects:(id const *)archives count:(NSUInteger)archives_count];
    [((MTLComputePipelineDescriptor *)object) setBinaryArchives:items];
    [items release];
}

void * metal_ComputePipelineDescriptor_linked_functions(void * object) {
    return (void *)[((MTLComputePipelineDescriptor *)object) linkedFunctions];
}

void metal_ComputePipelineDescriptor_set_linked_functions(void * object, void * functions) {
    [((MTLComputePipelineDescriptor *)object) setLinkedFunctions:(id)functions];
}

void * metal_ComputePipelineDescriptor_stage_input_descriptor(void * object) {
    return (void *)[((MTLComputePipelineDescriptor *)object) stageInputDescriptor];
}

void metal_ComputePipelineDescriptor_set_stage_input_descriptor(void * object, void * descriptor) {
    [((MTLComputePipelineDescriptor *)object) setStageInputDescriptor:(id)descriptor];
}

void * metal_ComputePipelineDescriptor_buffers(void * object) {
    return (void *)[((MTLComputePipelineDescriptor *)object) buffers];
}

void metal_ComputePipelineDescriptor_reset(void * object) {
    [((MTLComputePipelineDescriptor *)object) reset];
}

const char * metal_ComputePipelineState_label(void * object) {
    return (const char *)[[((id<MTLComputePipelineState>)object) label] UTF8String];
}

uint64_t metal_ComputePipelineState_max_total_threads_per_threadgroup(void * object) {
    return (uint64_t)[((id<MTLComputePipelineState>)object) maxTotalThreadsPerThreadgroup];
}

uint64_t metal_ComputePipelineState_thread_execution_width(void * object) {
    return (uint64_t)[((id<MTLComputePipelineState>)object) threadExecutionWidth];
}

uint64_t metal_ComputePipelineState_static_threadgroup_memory_length(void * object) {
    return (uint64_t)[((id<MTLComputePipelineState>)object) staticThreadgroupMemoryLength];
}

uint64_t metal_ComputePipelineState_imageblock_memory_length_for_dimensions(void * object, MTLSize dimensions) {
    return (uint64_t)[((id<MTLComputePipelineState>)object) imageblockMemoryLengthForDimensions:dimensions];
}

uint8_t metal_ComputePipelineState_support_indirect_command_buffers(void * object) {
    return [((id<MTLComputePipelineState>)object) supportIndirectCommandBuffers] ? 1 : 0;
}

void * metal_ComputePipelineState_function_handle_with_function(void * object, void * function) {
    return (void *)[((id<MTLComputePipelineState>)object) functionHandleWithFunction:(id)function];
}

void * metal_ComputePipelineState_new_visible_function_table_with_descriptor(void * object, void * descriptor) {
    return (void *)[((id<MTLComputePipelineState>)object) newVisibleFunctionTableWithDescriptor:(id)descriptor];
}

void * metal_ComputePipelineState_new_intersection_function_table_with_descriptor(void * object, void * descriptor) {
    return (void *)[((id<MTLComputePipelineState>)object) newIntersectionFunctionTableWithDescriptor:(id)descriptor];
}

void * metal_StageInputOutputDescriptor_new(void) {
    return (void *)[[MTLStageInputOutputDescriptor stageInputOutputDescriptor] retain];
}

void * metal_StageInputOutputDescriptor_attributes(void * object) {
    return (void *)[((MTLStageInputOutputDescriptor *)object) attributes];
}

uint64_t metal_StageInputOutputDescriptor_index_buffer_index(void * object) {
    return (uint64_t)[((MTLStageInputOutputDescriptor *)object) indexBufferIndex];
}

void metal_StageInputOutputDescriptor_set_index_buffer_index(void * object, uint64_t idx_buffer_idx) {
    [((MTLStageInputOutputDescriptor *)object) setIndexBufferIndex:idx_buffer_idx];
}

uint64_t metal_StageInputOutputDescriptor_index_type(void * object) {
    return (uint64_t)[((MTLStageInputOutputDescriptor *)object) indexType];
}

void metal_StageInputOutputDescriptor_set_index_type(void * object, uint64_t index_ty) {
    [((MTLStageInputOutputDescriptor *)object) setIndexType:index_ty];
}

void * metal_StageInputOutputDescriptor_layouts(void * object) {
    return (void *)[((MTLStageInputOutputDescriptor *)object) layouts];
}

void metal_StageInputOutputDescriptor_reset(void * object) {
    [((MTLStageInputOutputDescriptor *)object) reset];
}

void * metal_AttributeDescriptorArray_object_at(void * object, uint64_t index) {
    return (void *)[((MTLAttributeDescriptorArray *)object) objectAtIndexedSubscript:index];
}

void metal_AttributeDescriptorArray_set_object_at(void * object, uint64_t index, void * buffer_desc) {
    [((MTLAttributeDescriptorArray *)object) setObject:(id)buffer_desc atIndexedSubscript:index];
}

uint64_t metal_AttributeDescriptor_buffer_index(void * object) {
    return (uint64_t)[((MTLAttributeDescriptor *)object) bufferIndex];
}

void metal_AttributeDescriptor_set_buffer_index(void * object, uint64_t buffer_index) {
    [((MTLAttributeDescriptor *)object) setBufferIndex:buffer_index];
}

uint64_t metal_AttributeDescriptor_format(void * object) {
    return (uint64_t)[((MTLAttributeDescriptor *)object) format];
}

void metal_AttributeDescriptor_set_format(void * object, uint64_t format) {
    [((MTLAttributeDescriptor *)object) setFormat:format];
}

uint64_t metal_AttributeDescriptor_offset(void * object) {
    return (uint64_t)[((MTLAttributeDescriptor *)object) offset];
}

void metal_AttributeDescriptor_set_offset(void * object, uint64_t offset) {
    [((MTLAttributeDescriptor *)object) setOffset:offset];
}

void * metal_BufferLayoutDescriptorArray_object_at(void * object, uint64_t index) {
    return (void *)[((MTLBufferLayoutDescriptorArray *)object) objectAtIndexedSubscript:index];
}

void metal_BufferLayoutDescriptorArray_set_object_at(void * object, uint64_t index, void * buffer_desc) {
    [((MTLBufferLayoutDescriptorArray *)object) setObject:(id)buffer_desc atIndexedSubscript:index];
}

uint64_t metal_BufferLayoutDescriptor_step_function(void * object) {
    return (uint64_t)[((MTLBufferLayoutDescriptor *)object) stepFunction];
}

void metal_BufferLayoutDescriptor_set_step_function(void * object, uint64_t step_function) {
    [((MTLBufferLayoutDescriptor *)object) setStepFunction:step_function];
}

uint64_t metal_BufferLayoutDescriptor_step_rate(void * object) {
    return (uint64_t)[((MTLBufferLayoutDescriptor *)object) stepRate];
}

void metal_BufferLayoutDescriptor_set_step_rate(void * object, uint64_t step_rate) {
    [((MTLBufferLayoutDescriptor *)object) setStepRate:step_rate];
}

uint64_t metal_BufferLayoutDescriptor_stride(void * object) {
    return (uint64_t)[((MTLBufferLayoutDescriptor *)object) stride];
}

void metal_BufferLayoutDescriptor_set_stride(void * object, uint64_t stride) {
    [((MTLBufferLayoutDescriptor *)object) setStride:stride];
}

void * metal_RenderPassAttachmentDescriptor_texture(void * object) {
    return (void *)[((MTLRenderPassAttachmentDescriptor *)object) texture];
}

void metal_RenderPassAttachmentDescriptor_set_texture(void * object, void * texture) {
    [((MTLRenderPassAttachmentDescriptor *)object) setTexture:(id)texture];
}

uint64_t metal_RenderPassAttachmentDescriptor_level(void * object) {
    return (uint64_t)[((MTLRenderPassAttachmentDescriptor *)object) level];
}

void metal_RenderPassAttachmentDescriptor_set_level(void * object, uint64_t level) {
    [((MTLRenderPassAttachmentDescriptor *)object) setLevel:level];
}

uint64_t metal_RenderPassAttachmentDescriptor_slice(void * object) {
    return (uint64_t)[((MTLRenderPassAttachmentDescriptor *)object) slice];
}

void metal_RenderPassAttachmentDescriptor_set_slice(void * object, uint64_t slice) {
    [((MTLRenderPassAttachmentDescriptor *)object) setSlice:slice];
}

uint64_t metal_RenderPassAttachmentDescriptor_depth_plane(void * object) {
    return (uint64_t)[((MTLRenderPassAttachmentDescriptor *)object) depthPlane];
}

void metal_RenderPassAttachmentDescriptor_set_depth_plane(void * object, uint64_t depth_plane) {
    [((MTLRenderPassAttachmentDescriptor *)object) setDepthPlane:depth_plane];
}

void * metal_RenderPassAttachmentDescriptor_resolve_texture(void * object) {
    return (void *)[((MTLRenderPassAttachmentDescriptor *)object) resolveTexture];
}

void metal_RenderPassAttachmentDescriptor_set_resolve_texture(void * object, void * resolve_texture) {
    [((MTLRenderPassAttachmentDescriptor *)object) setResolveTexture:(id)resolve_texture];
}

uint64_t metal_RenderPassAttachmentDescriptor_resolve_level(void * object) {
    return (uint64_t)[((MTLRenderPassAttachmentDescriptor *)object) resolveLevel];
}

void metal_RenderPassAttachmentDescriptor_set_resolve_level(void * object, uint64_t resolve_level) {
    [((MTLRenderPassAttachmentDescriptor *)object) setResolveLevel:resolve_level];
}

uint64_t metal_RenderPassAttachmentDescriptor_resolve_slice(void * object) {
    return (uint64_t)[((MTLRenderPassAttachmentDescriptor *)object) resolveSlice];
}

void metal_RenderPassAttachmentDescriptor_set_resolve_slice(void * object, uint64_t resolve_slice) {
    [((MTLRenderPassAttachmentDescriptor *)object) setResolveSlice:resolve_slice];
}

uint64_t metal_RenderPassAttachmentDescriptor_resolve_depth_plane(void * object) {
    return (uint64_t)[((MTLRenderPassAttachmentDescriptor *)object) resolveDepthPlane];
}

void metal_RenderPassAttachmentDescriptor_set_resolve_depth_plane(void * object, uint64_t resolve_depth_plane) {
    [((MTLRenderPassAttachmentDescriptor *)object) setResolveDepthPlane:resolve_depth_plane];
}

uint64_t metal_RenderPassAttachmentDescriptor_load_action(void * object) {
    return (uint64_t)[((MTLRenderPassAttachmentDescriptor *)object) loadAction];
}

void metal_RenderPassAttachmentDescriptor_set_load_action(void * object, uint64_t load_action) {
    [((MTLRenderPassAttachmentDescriptor *)object) setLoadAction:load_action];
}

uint64_t metal_RenderPassAttachmentDescriptor_store_action(void * object) {
    return (uint64_t)[((MTLRenderPassAttachmentDescriptor *)object) storeAction];
}

void metal_RenderPassAttachmentDescriptor_set_store_action(void * object, uint64_t store_action) {
    [((MTLRenderPassAttachmentDescriptor *)object) setStoreAction:store_action];
}

void * metal_RenderPassColorAttachmentDescriptor_new(void) {
    return (void *)[MTLRenderPassColorAttachmentDescriptor new];
}

MTLClearColor metal_RenderPassColorAttachmentDescriptor_clear_color(void * object) {
    return [((MTLRenderPassColorAttachmentDescriptor *)object) clearColor];
}

void metal_RenderPassColorAttachmentDescriptor_set_clear_color(void * object, MTLClearColor clear_color) {
    [((MTLRenderPassColorAttachmentDescriptor *)object) setClearColor:clear_color];
}

double metal_RenderPassDepthAttachmentDescriptor_clear_depth(void * object) {
    return (double)[((MTLRenderPassDepthAttachmentDescriptor *)object) clearDepth];
}

void metal_RenderPassDepthAttachmentDescriptor_set_clear_depth(void * object, double clear_depth) {
    [((MTLRenderPassDepthAttachmentDescriptor *)object) setClearDepth:clear_depth];
}

uint32_t metal_RenderPassStencilAttachmentDescriptor_clear_stencil(void * object) {
    return (uint32_t)[((MTLRenderPassStencilAttachmentDescriptor *)object) clearStencil];
}

void metal_RenderPassStencilAttachmentDescriptor_set_clear_stencil(void * object, uint32_t clear_stencil) {
    [((MTLRenderPassStencilAttachmentDescriptor *)object) setClearStencil:clear_stencil];
}

uint32_t metal_RenderPassStencilAttachmentDescriptor_stencil_resolve_filter(void * object) {
    return (uint32_t)[((MTLRenderPassStencilAttachmentDescriptor *)object) stencilResolveFilter];
}

void metal_RenderPassStencilAttachmentDescriptor_set_stencil_resolve_filter(void * object, uint32_t stencil_resolve_filter) {
    [((MTLRenderPassStencilAttachmentDescriptor *)object) setStencilResolveFilter:stencil_resolve_filter];
}

void * metal_RenderPassColorAttachmentDescriptorArray_object_at(void * object, uint64_t index) {
    return (void *)[((MTLRenderPassColorAttachmentDescriptorArray *)object) objectAtIndexedSubscript:index];
}

void metal_RenderPassColorAttachmentDescriptorArray_set_object_at(void * object, uint64_t index, void * attachment) {
    [((MTLRenderPassColorAttachmentDescriptorArray *)object) setObject:(id)attachment atIndexedSubscript:index];
}

void * metal_RenderPassSampleBufferAttachmentDescriptor_new(void) {
    return (void *)[MTLRenderPassSampleBufferAttachmentDescriptor new];
}

void * metal_RenderPassSampleBufferAttachmentDescriptor_sample_buffer(void * object) {
    return (void *)[((MTLRenderPassSampleBufferAttachmentDescriptor *)object) sampleBuffer];
}

void metal_RenderPassSampleBufferAttachmentDescriptor_set_sample_buffer(void * object, void * sample_buffer) {
    [((MTLRenderPassSampleBufferAttachmentDescriptor *)object) setSampleBuffer:(id)sample_buffer];
}

uint64_t metal_RenderPassSampleBufferAttachmentDescriptor_start_of_vertex_sample_index(void * object) {
    return (uint64_t)[((MTLRenderPassSampleBufferAttachmentDescriptor *)object) startOfVertexSampleIndex];
}

void metal_RenderPassSampleBufferAttachmentDescriptor_set_start_of_vertex_sample_index(void * object, uint64_t start_of_vertex_sample_index) {
    [((MTLRenderPassSampleBufferAttachmentDescriptor *)object) setStartOfVertexSampleIndex:start_of_vertex_sample_index];
}

uint64_t metal_RenderPassSampleBufferAttachmentDescriptor_end_of_vertex_sample_index(void * object) {
    return (uint64_t)[((MTLRenderPassSampleBufferAttachmentDescriptor *)object) endOfVertexSampleIndex];
}

void metal_RenderPassSampleBufferAttachmentDescriptor_set_end_of_vertex_sample_index(void * object, uint64_t end_of_vertex_sample_index) {
    [((MTLRenderPassSampleBufferAttachmentDescriptor *)object) setEndOfVertexSampleIndex:end_of_vertex_sample_index];
}

uint64_t metal_RenderPassSampleBufferAttachmentDescriptor_start_of_fragment_sample_index(void * object) {
    return (uint64_t)[((MTLRenderPassSampleBufferAttachmentDescriptor *)object) startOfFragmentSampleIndex];
}

void metal_RenderPassSampleBufferAttachmentDescriptor_set_start_of_fragment_sample_index(void * object, uint64_t start_of_fragment_sample_index) {
    [((MTLRenderPassSampleBufferAttachmentDescriptor *)object) setStartOfFragmentSampleIndex:start_of_fragment_sample_index];
}

uint64_t metal_RenderPassSampleBufferAttachmentDescriptor_end_of_fragment_sample_index(void * object) {
    return (uint64_t)[((MTLRenderPassSampleBufferAttachmentDescriptor *)object) endOfFragmentSampleIndex];
}

void metal_RenderPassSampleBufferAttachmentDescriptor_set_end_of_fragment_sample_index(void * object, uint64_t end_of_fragment_sample_index) {
    [((MTLRenderPassSampleBufferAttachmentDescriptor *)object) setEndOfFragmentSampleIndex:end_of_fragment_sample_index];
}

void * metal_RenderPassSampleBufferAttachmentDescriptorArray_object_at(void * object, uint64_t index) {
    return (void *)[((MTLRenderPassSampleBufferAttachmentDescriptorArray *)object) objectAtIndexedSubscript:index];
}

void metal_RenderPassSampleBufferAttachmentDescriptorArray_set_object_at(void * object, uint64_t index, void * attachment) {
    [((MTLRenderPassSampleBufferAttachmentDescriptorArray *)object) setObject:(id)attachment atIndexedSubscript:index];
}

void * metal_RenderPassDescriptor_new(void) {
    return (void *)[[MTLRenderPassDescriptor renderPassDescriptor] retain];
}

void * metal_RenderPassDescriptor_color_attachments(void * object) {
    return (void *)[((MTLRenderPassDescriptor *)object) colorAttachments];
}

void * metal_RenderPassDescriptor_depth_attachment(void * object) {
    return (void *)[((MTLRenderPassDescriptor *)object) depthAttachment];
}

void metal_RenderPassDescriptor_set_depth_attachment(void * object, void * depth_attachment) {
    [((MTLRenderPassDescriptor *)object) setDepthAttachment:(id)depth_attachment];
}

void * metal_RenderPassDescriptor_stencil_attachment(void * object) {
    return (void *)[((MTLRenderPassDescriptor *)object) stencilAttachment];
}

void metal_RenderPassDescriptor_set_stencil_attachment(void * object, void * stencil_attachment) {
    [((MTLRenderPassDescriptor *)object) setStencilAttachment:(id)stencil_attachment];
}

void * metal_RenderPassDescriptor_visibility_result_buffer(void * object) {
    return (void *)[((MTLRenderPassDescriptor *)object) visibilityResultBuffer];
}

void metal_RenderPassDescriptor_set_visibility_result_buffer(void * object, void * buffer) {
    [((MTLRenderPassDescriptor *)object) setVisibilityResultBuffer:(id)buffer];
}

uint64_t metal_RenderPassDescriptor_render_target_array_length(void * object) {
    return (uint64_t)[((MTLRenderPassDescriptor *)object) renderTargetArrayLength];
}

void metal_RenderPassDescriptor_set_render_target_array_length(void * object, uint64_t length) {
    [((MTLRenderPassDescriptor *)object) setRenderTargetArrayLength:length];
}

uint64_t metal_RenderPassDescriptor_render_target_width(void * object) {
    return (uint64_t)[((MTLRenderPassDescriptor *)object) renderTargetWidth];
}

void metal_RenderPassDescriptor_set_render_target_width(void * object, uint64_t size) {
    [((MTLRenderPassDescriptor *)object) setRenderTargetWidth:size];
}

uint64_t metal_RenderPassDescriptor_render_target_height(void * object) {
    return (uint64_t)[((MTLRenderPassDescriptor *)object) renderTargetHeight];
}

void metal_RenderPassDescriptor_set_render_target_height(void * object, uint64_t size) {
    [((MTLRenderPassDescriptor *)object) setRenderTargetHeight:size];
}

uint64_t metal_RenderPassDescriptor_default_raster_sample_count(void * object) {
    return (uint64_t)[((MTLRenderPassDescriptor *)object) defaultRasterSampleCount];
}

void metal_RenderPassDescriptor_set_default_raster_sample_count(void * object, uint64_t count) {
    [((MTLRenderPassDescriptor *)object) setDefaultRasterSampleCount:count];
}

void * metal_RenderPassDescriptor_sample_buffer_attachments(void * object) {
    return (void *)[((MTLRenderPassDescriptor *)object) sampleBufferAttachments];
}

void * metal_ComputePassDescriptor_new(void) {
    return (void *)[[MTLComputePassDescriptor computePassDescriptor] retain];
}

void * metal_ComputePassDescriptor_sample_buffer_attachments(void * object) {
    return (void *)[((MTLComputePassDescriptor *)object) sampleBufferAttachments];
}

void metal_ComputePassDescriptor_set_dispatch_type(void * object, uint32_t ty) {
    [((MTLComputePassDescriptor *)object) setDispatchType:ty];
}

void * metal_ComputePassSampleBufferAttachmentDescriptorArray_object_at(void * object, uint64_t index) {
    return (void *)[((MTLComputePassSampleBufferAttachmentDescriptorArray *)object) objectAtIndexedSubscript:index];
}

void metal_ComputePassSampleBufferAttachmentDescriptorArray_set_object_at(void * object, uint64_t index, void * attachment) {
    [((MTLComputePassSampleBufferAttachmentDescriptorArray *)object) setObject:(id)attachment atIndexedSubscript:index];
}

void * metal_ComputePassSampleBufferAttachmentDescriptor_new(void) {
    return (void *)[MTLComputePassSampleBufferAttachmentDescriptor new];
}

void * metal_ComputePassSampleBufferAttachmentDescriptor_sample_buffer(void * object) {
    return (void *)[((MTLComputePassSampleBufferAttachmentDescriptor *)object) sampleBuffer];
}

void metal_ComputePassSampleBufferAttachmentDescriptor_set_sample_buffer(void * object, void * sample_buffer) {
    [((MTLComputePassSampleBufferAttachmentDescriptor *)object) setSampleBuffer:(id)sample_buffer];
}

uint64_t metal_ComputePassSampleBufferAttachmentDescriptor_start_of_encoder_sample_index(void * object) {
    return (uint64_t)[((MTLComputePassSampleBufferAttachmentDescriptor *)object) startOfEncoderSampleIndex];
}

void metal_ComputePassSampleBufferAttachmentDescriptor_set_start_of_encoder_sample_index(void * object, uint64_t start_of_encoder_sample_index) {
    [((MTLComputePassSampleBufferAttachmentDescriptor *)object) setStartOfEncoderSampleIndex:start_of_encoder_sample_index];
}

uint64_t metal_ComputePassSampleBufferAttachmentDescriptor_end_of_encoder_sample_index(void * object) {
    return (uint64_t)[((MTLComputePassSampleBufferAttachmentDescriptor *)object) endOfEncoderSampleIndex];
}

void metal_ComputePassSampleBufferAttachmentDescriptor_set_end_of_encoder_sample_index(void * object, uint64_t end_of_encoder_sample_index) {
    [((MTLComputePassSampleBufferAttachmentDescriptor *)object) setEndOfEncoderSampleIndex:end_of_encoder_sample_index];
}

void * metal_BlitPassDescriptor_new(void) {
    return (void *)[[MTLBlitPassDescriptor blitPassDescriptor] retain];
}

void * metal_BlitPassDescriptor_sample_buffer_attachments(void * object) {
    return (void *)[((MTLBlitPassDescriptor *)object) sampleBufferAttachments];
}

void * metal_BlitPassSampleBufferAttachmentDescriptorArray_object_at(void * object, uint64_t index) {
    return (void *)[((MTLBlitPassSampleBufferAttachmentDescriptorArray *)object) objectAtIndexedSubscript:index];
}

void metal_BlitPassSampleBufferAttachmentDescriptorArray_set_object_at(void * object, uint64_t index, void * attachment) {
    [((MTLBlitPassSampleBufferAttachmentDescriptorArray *)object) setObject:(id)attachment atIndexedSubscript:index];
}

void * metal_BlitPassSampleBufferAttachmentDescriptor_new(void) {
    return (void *)[MTLBlitPassSampleBufferAttachmentDescriptor new];
}

void * metal_BlitPassSampleBufferAttachmentDescriptor_sample_buffer(void * object) {
    return (void *)[((MTLBlitPassSampleBufferAttachmentDescriptor *)object) sampleBuffer];
}

void metal_BlitPassSampleBufferAttachmentDescriptor_set_sample_buffer(void * object, void * sample_buffer) {
    [((MTLBlitPassSampleBufferAttachmentDescriptor *)object) setSampleBuffer:(id)sample_buffer];
}

uint64_t metal_BlitPassSampleBufferAttachmentDescriptor_start_of_encoder_sample_index(void * object) {
    return (uint64_t)[((MTLBlitPassSampleBufferAttachmentDescriptor *)object) startOfEncoderSampleIndex];
}

void metal_BlitPassSampleBufferAttachmentDescriptor_set_start_of_encoder_sample_index(void * object, uint64_t start_of_encoder_sample_index) {
    [((MTLBlitPassSampleBufferAttachmentDescriptor *)object) setStartOfEncoderSampleIndex:start_of_encoder_sample_index];
}

uint64_t metal_BlitPassSampleBufferAttachmentDescriptor_end_of_encoder_sample_index(void * object) {
    return (uint64_t)[((MTLBlitPassSampleBufferAttachmentDescriptor *)object) endOfEncoderSampleIndex];
}

void metal_BlitPassSampleBufferAttachmentDescriptor_set_end_of_encoder_sample_index(void * object, uint64_t end_of_encoder_sample_index) {
    [((MTLBlitPassSampleBufferAttachmentDescriptor *)object) setEndOfEncoderSampleIndex:end_of_encoder_sample_index];
}

void * metal_AccelerationStructurePassDescriptor_new(void) {
    return (void *)[[MTLAccelerationStructurePassDescriptor accelerationStructurePassDescriptor] retain];
}

void * metal_AccelerationStructurePassDescriptor_sample_buffer_attachments(void * object) {
    return (void *)[((MTLAccelerationStructurePassDescriptor *)object) sampleBufferAttachments];
}

void * metal_AccelerationStructurePassSampleBufferAttachmentDescriptorArray_object_at(void * object, uint64_t index) {
    return (void *)[((MTLAccelerationStructurePassSampleBufferAttachmentDescriptorArray *)object) objectAtIndexedSubscript:index];
}

void metal_AccelerationStructurePassSampleBufferAttachmentDescriptorArray_set_object_at(void * object, uint64_t index, void * attachment) {
    [((MTLAccelerationStructurePassSampleBufferAttachmentDescriptorArray *)object) setObject:(id)attachment atIndexedSubscript:index];
}

void * metal_AccelerationStructurePassSampleBufferAttachmentDescriptor_new(void) {
    return (void *)[MTLAccelerationStructurePassSampleBufferAttachmentDescriptor new];
}

void * metal_AccelerationStructurePassSampleBufferAttachmentDescriptor_sample_buffer(void * object) {
    return (void *)[((MTLAccelerationStructurePassSampleBufferAttachmentDescriptor *)object) sampleBuffer];
}

void metal_AccelerationStructurePassSampleBufferAttachmentDescriptor_set_sample_buffer(void * object, void * sample_buffer) {
    [((MTLAccelerationStructurePassSampleBufferAttachmentDescriptor *)object) setSampleBuffer:(id)sample_buffer];
}

uint64_t metal_AccelerationStructurePassSampleBufferAttachmentDescriptor_start_of_encoder_sample_index(void * object) {
    return (uint64_t)[((MTLAccelerationStructurePassSampleBufferAttachmentDescriptor *)object) startOfEncoderSampleIndex];
}

void metal_AccelerationStructurePassSampleBufferAttachmentDescriptor_set_start_of_encoder_sample_index(void * object, uint64_t start_of_encoder_sample_index) {
    [((MTLAccelerationStructurePassSampleBufferAttachmentDescriptor *)object) setStartOfEncoderSampleIndex:start_of_encoder_sample_index];
}

uint64_t metal_AccelerationStructurePassSampleBufferAttachmentDescriptor_end_of_encoder_sample_index(void * object) {
    return (uint64_t)[((MTLAccelerationStructurePassSampleBufferAttachmentDescriptor *)object) endOfEncoderSampleIndex];
}

void metal_AccelerationStructurePassSampleBufferAttachmentDescriptor_set_end_of_encoder_sample_index(void * object, uint64_t end_of_encoder_sample_index) {
    [((MTLAccelerationStructurePassSampleBufferAttachmentDescriptor *)object) setEndOfEncoderSampleIndex:end_of_encoder_sample_index];
}

