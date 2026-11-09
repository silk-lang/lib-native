// SPDX-License-Identifier: MIT OR Apache-2.0
// Derived from metal-rs, copyright GFX developers.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <stdint.h>

// Objects and array entries are borrowed unless explicitly documented +1.
// The caller guarantees object validity, platform availability, and array extents.

const char * metal_CommandEncoder_label(void * object) {
    return [[(id<MTLCommandEncoder>)object label] UTF8String];
}

void metal_CommandEncoder_set_label(void * object, const char * label) {
    [(id<MTLCommandEncoder>)object setLabel: [NSString stringWithUTF8String:label]];
}

void metal_CommandEncoder_end_encoding(void * object) {
    [(id<MTLCommandEncoder>)object endEncoding];
}

void metal_CommandEncoder_insert_debug_signpost(void * object, const char * name) {
    [(id<MTLCommandEncoder>)object insertDebugSignpost: [NSString stringWithUTF8String:name]];
}

void metal_CommandEncoder_push_debug_group(void * object, const char * name) {
    [(id<MTLCommandEncoder>)object pushDebugGroup: [NSString stringWithUTF8String:name]];
}

void metal_CommandEncoder_pop_debug_group(void * object) {
    [(id<MTLCommandEncoder>)object popDebugGroup];
}

void * metal_ParallelRenderCommandEncoder_render_command_encoder(void * object) {
    return [(id<MTLParallelRenderCommandEncoder>)object renderCommandEncoder];
}

void metal_RenderCommandEncoder_set_render_pipeline_state(void * object, void * pipeline_state) {
    [(id<MTLRenderCommandEncoder>)object setRenderPipelineState:(id)pipeline_state];
}

void metal_RenderCommandEncoder_set_viewport(void * object, MTLViewport viewport) {
    [(id<MTLRenderCommandEncoder>)object setViewport:viewport];
}

void metal_RenderCommandEncoder_set_viewports(void * object, const MTLViewport * viewports, NSUInteger viewports_count) {
    [(id<MTLRenderCommandEncoder>)object setViewports:viewports count:viewports_count];
}

void metal_RenderCommandEncoder_set_front_facing_winding(void * object, NSUInteger winding) {
    [(id<MTLRenderCommandEncoder>)object setFrontFacingWinding:(MTLWinding)winding];
}

void metal_RenderCommandEncoder_set_cull_mode(void * object, NSUInteger mode) {
    [(id<MTLRenderCommandEncoder>)object setCullMode:(MTLCullMode)mode];
}

void metal_RenderCommandEncoder_set_depth_clip_mode(void * object, NSUInteger mode) {
    [(id<MTLRenderCommandEncoder>)object setDepthClipMode:(MTLDepthClipMode)mode];
}

void metal_RenderCommandEncoder_set_depth_bias(void * object, float bias, float scale, float clamp) {
    [(id<MTLRenderCommandEncoder>)object setDepthBias:bias slopeScale:scale clamp:clamp];
}

void metal_RenderCommandEncoder_set_scissor_rect(void * object, MTLScissorRect rect) {
    [(id<MTLRenderCommandEncoder>)object setScissorRect:rect];
}

void metal_RenderCommandEncoder_set_scissor_rects(void * object, const MTLScissorRect * rects, NSUInteger rects_count) {
    [(id<MTLRenderCommandEncoder>)object setScissorRects:rects count:rects_count];
}

void metal_RenderCommandEncoder_set_triangle_fill_mode(void * object, NSUInteger mode) {
    [(id<MTLRenderCommandEncoder>)object setTriangleFillMode:(MTLTriangleFillMode)mode];
}

void metal_RenderCommandEncoder_set_blend_color(void * object, float red, float green, float blue, float alpha) {
    [(id<MTLRenderCommandEncoder>)object setBlendColorRed:red green:green blue:blue alpha:alpha];
}

void metal_RenderCommandEncoder_set_depth_stencil_state(void * object, void * depth_stencil_state) {
    [(id<MTLRenderCommandEncoder>)object setDepthStencilState:(id)depth_stencil_state];
}

void metal_RenderCommandEncoder_set_stencil_reference_value(void * object, uint32_t value) {
    [(id<MTLRenderCommandEncoder>)object setStencilReferenceValue:value];
}

void metal_RenderCommandEncoder_set_stencil_front_back_reference_value(void * object, uint32_t front, uint32_t back) {
    [(id<MTLRenderCommandEncoder>)object setStencilFrontReferenceValue:front backReferenceValue:back];
}

void metal_RenderCommandEncoder_set_visibility_result_mode(void * object, NSUInteger mode, NSUInteger offset) {
    [(id<MTLRenderCommandEncoder>)object setVisibilityResultMode:(MTLVisibilityResultMode)mode offset:offset];
}

void metal_RenderCommandEncoder_set_vertex_amplification_count(void * object, NSUInteger count, const MTLVertexAmplificationViewMapping * view_mappings) {
    [(id<MTLRenderCommandEncoder>)object setVertexAmplificationCount:count viewMappings:view_mappings];
}

void metal_RenderCommandEncoder_set_vertex_bytes(void * object, NSUInteger index, NSUInteger length, const void * bytes) {
    [(id<MTLRenderCommandEncoder>)object setVertexBytes:bytes length:length atIndex:index];
}

void metal_RenderCommandEncoder_set_vertex_buffer(void * object, NSUInteger index, void * buffer, NSUInteger offset) {
    [(id<MTLRenderCommandEncoder>)object setVertexBuffer:(id)buffer offset:offset atIndex:index];
}

void metal_RenderCommandEncoder_set_vertex_buffer_offset(void * object, NSUInteger index, NSUInteger offset) {
    [(id<MTLRenderCommandEncoder>)object setVertexBufferOffset:offset atIndex:index];
}

void metal_RenderCommandEncoder_set_vertex_buffers(void * object, NSUInteger start_index, const id * data, const NSUInteger * offsets, NSUInteger data_count) {
    [(id<MTLRenderCommandEncoder>)object setVertexBuffers:data offsets:offsets withRange:NSMakeRange(start_index, data_count)];
}

void metal_RenderCommandEncoder_set_vertex_texture(void * object, NSUInteger index, void * texture) {
    [(id<MTLRenderCommandEncoder>)object setVertexTexture:(id)texture atIndex:index];
}

void metal_RenderCommandEncoder_set_vertex_textures(void * object, NSUInteger start_index, const id * data, NSUInteger data_count) {
    [(id<MTLRenderCommandEncoder>)object setVertexTextures:data withRange:NSMakeRange(start_index, data_count)];
}

void metal_RenderCommandEncoder_set_vertex_sampler_state(void * object, NSUInteger index, void * sampler) {
    [(id<MTLRenderCommandEncoder>)object setVertexSamplerState:(id)sampler atIndex:index];
}

void metal_RenderCommandEncoder_set_vertex_sampler_states(void * object, NSUInteger start_index, const id * data, NSUInteger data_count) {
    [(id<MTLRenderCommandEncoder>)object setVertexSamplerStates:data withRange:NSMakeRange(start_index, data_count)];
}

void metal_RenderCommandEncoder_set_vertex_sampler_state_with_lod(void * object, NSUInteger index, void * sampler, float lod_clamp_start, float lod_clamp_end) {
    [(id<MTLRenderCommandEncoder>)object setVertexSamplerState:(id)sampler lodMinClamp:lod_clamp_start lodMaxClamp:lod_clamp_end atIndex:index];
}

void metal_RenderCommandEncoder_set_vertex_acceleration_structure(void * object, NSUInteger index, void * accel) {
    [(id<MTLRenderCommandEncoder>)object setVertexAccelerationStructure:(id)accel atBufferIndex:index];
}

void metal_RenderCommandEncoder_set_vertex_intersection_function_table(void * object, NSUInteger index, void * table) {
    [(id<MTLRenderCommandEncoder>)object setVertexIntersectionFunctionTable:(id)table atBufferIndex:index];
}

void metal_RenderCommandEncoder_set_vertex_visible_function_table(void * object, NSUInteger buffer_index, void * visible_function_table) {
    [(id<MTLRenderCommandEncoder>)object setVertexVisibleFunctionTable:(id)visible_function_table atBufferIndex:buffer_index];
}

void metal_RenderCommandEncoder_set_vertex_visible_function_tables(void * object, NSUInteger buffer_start_index, const id * visible_function_tables, NSUInteger visible_function_tables_count) {
    [(id<MTLRenderCommandEncoder>)object setVertexVisibleFunctionTables:visible_function_tables withBufferRange:NSMakeRange(buffer_start_index, visible_function_tables_count)];
}

void metal_RenderCommandEncoder_set_object_buffer(void * object, NSUInteger index, void * buffer, NSUInteger offset) {
    [(id<MTLRenderCommandEncoder>)object setObjectBuffer:(id)buffer offset:offset atIndex:index];
}

void metal_RenderCommandEncoder_set_object_buffer_offset(void * object, NSUInteger index, NSUInteger offset) {
    [(id<MTLRenderCommandEncoder>)object setObjectBufferOffset:offset atIndex:index];
}

void metal_RenderCommandEncoder_set_object_bytes(void * object, NSUInteger index, NSUInteger length, const void * bytes) {
    [(id<MTLRenderCommandEncoder>)object setObjectBytes:bytes length:length atIndex:index];
}

void metal_RenderCommandEncoder_set_object_sampler_state(void * object, NSUInteger index, void * sampler) {
    [(id<MTLRenderCommandEncoder>)object setObjectSamplerState:(id)sampler atIndex:index];
}

void metal_RenderCommandEncoder_set_object_sampler_state_with_lod(void * object, NSUInteger index, void * sampler, float lod_clamp_start, float lod_clamp_end) {
    [(id<MTLRenderCommandEncoder>)object setObjectSamplerState:(id)sampler lodMinClamp:lod_clamp_start lodMaxClamp:lod_clamp_end atIndex:index];
}

void metal_RenderCommandEncoder_set_object_texture(void * object, NSUInteger index, void * texture) {
    [(id<MTLRenderCommandEncoder>)object setObjectTexture:(id)texture atIndex:index];
}

void metal_RenderCommandEncoder_set_object_threadgroup_memory_length(void * object, NSUInteger index, NSUInteger length) {
    [(id<MTLRenderCommandEncoder>)object setObjectThreadgroupMemoryLength:length atIndex:index];
}

void metal_RenderCommandEncoder_set_object_buffers(void * object, NSUInteger start_index, const id * data, const NSUInteger * offsets, NSUInteger data_count) {
    [(id<MTLRenderCommandEncoder>)object setObjectBuffers:data offsets:offsets withRange:NSMakeRange(start_index, data_count)];
}

void metal_RenderCommandEncoder_set_object_sampler_states(void * object, NSUInteger start_index, const id * data, NSUInteger data_count) {
    [(id<MTLRenderCommandEncoder>)object setObjectSamplerStates:data withRange:NSMakeRange(start_index, data_count)];
}

void metal_RenderCommandEncoder_set_object_textures(void * object, NSUInteger start_index, const id * data, NSUInteger data_count) {
    [(id<MTLRenderCommandEncoder>)object setObjectTextures:data withRange:NSMakeRange(start_index, data_count)];
}

void metal_RenderCommandEncoder_set_mesh_buffer(void * object, NSUInteger index, void * buffer, NSUInteger offset) {
    [(id<MTLRenderCommandEncoder>)object setMeshBuffer:(id)buffer offset:offset atIndex:index];
}

void metal_RenderCommandEncoder_set_mesh_buffer_offset(void * object, NSUInteger index, NSUInteger offset) {
    [(id<MTLRenderCommandEncoder>)object setMeshBufferOffset:offset atIndex:index];
}

void metal_RenderCommandEncoder_set_mesh_bytes(void * object, NSUInteger index, NSUInteger length, const void * bytes) {
    [(id<MTLRenderCommandEncoder>)object setMeshBytes:bytes length:length atIndex:index];
}

void metal_RenderCommandEncoder_set_mesh_sampler_state(void * object, NSUInteger index, void * sampler) {
    [(id<MTLRenderCommandEncoder>)object setMeshSamplerState:(id)sampler atIndex:index];
}

void metal_RenderCommandEncoder_set_mesh_sampler_state_with_lod(void * object, NSUInteger index, void * sampler, float lod_clamp_start, float lod_clamp_end) {
    [(id<MTLRenderCommandEncoder>)object setMeshSamplerState:(id)sampler lodMinClamp:lod_clamp_start lodMaxClamp:lod_clamp_end atIndex:index];
}

void metal_RenderCommandEncoder_set_mesh_texture(void * object, NSUInteger index, void * texture) {
    [(id<MTLRenderCommandEncoder>)object setMeshTexture:(id)texture atIndex:index];
}

void metal_RenderCommandEncoder_set_mesh_buffers(void * object, NSUInteger start_index, const id * data, const NSUInteger * offsets, NSUInteger data_count) {
    [(id<MTLRenderCommandEncoder>)object setMeshBuffers:data offsets:offsets withRange:NSMakeRange(start_index, data_count)];
}

void metal_RenderCommandEncoder_set_mesh_sampler_states(void * object, NSUInteger start_index, const id * data, NSUInteger data_count) {
    [(id<MTLRenderCommandEncoder>)object setMeshSamplerStates:data withRange:NSMakeRange(start_index, data_count)];
}

void metal_RenderCommandEncoder_set_mesh_textures(void * object, NSUInteger start_index, const id * data, NSUInteger data_count) {
    [(id<MTLRenderCommandEncoder>)object setMeshTextures:data withRange:NSMakeRange(start_index, data_count)];
}

void metal_RenderCommandEncoder_set_fragment_bytes(void * object, NSUInteger index, NSUInteger length, const void * bytes) {
    [(id<MTLRenderCommandEncoder>)object setFragmentBytes:bytes length:length atIndex:index];
}

void metal_RenderCommandEncoder_set_fragment_buffer(void * object, NSUInteger index, void * buffer, NSUInteger offset) {
    [(id<MTLRenderCommandEncoder>)object setFragmentBuffer:(id)buffer offset:offset atIndex:index];
}

void metal_RenderCommandEncoder_set_fragment_buffer_offset(void * object, NSUInteger index, NSUInteger offset) {
    [(id<MTLRenderCommandEncoder>)object setFragmentBufferOffset:offset atIndex:index];
}

void metal_RenderCommandEncoder_set_fragment_buffers(void * object, NSUInteger start_index, const id * data, const NSUInteger * offsets, NSUInteger data_count) {
    [(id<MTLRenderCommandEncoder>)object setFragmentBuffers:data offsets:offsets withRange:NSMakeRange(start_index, data_count)];
}

void metal_RenderCommandEncoder_set_fragment_texture(void * object, NSUInteger index, void * texture) {
    [(id<MTLRenderCommandEncoder>)object setFragmentTexture:(id)texture atIndex:index];
}

void metal_RenderCommandEncoder_set_fragment_textures(void * object, NSUInteger start_index, const id * data, NSUInteger data_count) {
    [(id<MTLRenderCommandEncoder>)object setFragmentTextures:data withRange:NSMakeRange(start_index, data_count)];
}

void metal_RenderCommandEncoder_set_fragment_sampler_state(void * object, NSUInteger index, void * sampler) {
    [(id<MTLRenderCommandEncoder>)object setFragmentSamplerState:(id)sampler atIndex:index];
}

void metal_RenderCommandEncoder_set_fragment_sampler_states(void * object, NSUInteger start_index, const id * data, NSUInteger data_count) {
    [(id<MTLRenderCommandEncoder>)object setFragmentSamplerStates:data withRange:NSMakeRange(start_index, data_count)];
}

void metal_RenderCommandEncoder_set_fragment_sampler_state_with_lod(void * object, NSUInteger index, void * sampler, float lod_clamp_start, float lod_clamp_end) {
    [(id<MTLRenderCommandEncoder>)object setFragmentSamplerState:(id)sampler lodMinClamp:lod_clamp_start lodMaxClamp:lod_clamp_end atIndex:index];
}

void metal_RenderCommandEncoder_set_fragment_acceleration_structure(void * object, NSUInteger index, void * accel) {
    [(id<MTLRenderCommandEncoder>)object setFragmentAccelerationStructure:(id)accel atBufferIndex:index];
}

void metal_RenderCommandEncoder_set_fragment_intersection_function_table(void * object, NSUInteger index, void * table) {
    [(id<MTLRenderCommandEncoder>)object setFragmentIntersectionFunctionTable:(id)table atBufferIndex:index];
}

void metal_RenderCommandEncoder_set_fragment_visible_function_table(void * object, NSUInteger buffer_index, void * visible_function_table) {
    [(id<MTLRenderCommandEncoder>)object setFragmentVisibleFunctionTable:(id)visible_function_table atBufferIndex:buffer_index];
}

void metal_RenderCommandEncoder_set_fragment_visible_function_tables(void * object, NSUInteger buffer_start_index, const id * visible_function_tables, NSUInteger visible_function_tables_count) {
    [(id<MTLRenderCommandEncoder>)object setFragmentVisibleFunctionTables:visible_function_tables withBufferRange:NSMakeRange(buffer_start_index, visible_function_tables_count)];
}

void metal_RenderCommandEncoder_draw_primitives(void * object, NSUInteger primitive_type, NSUInteger vertex_start, NSUInteger vertex_count) {
    [(id<MTLRenderCommandEncoder>)object drawPrimitives:(MTLPrimitiveType)primitive_type vertexStart:vertex_start vertexCount:vertex_count];
}

void metal_RenderCommandEncoder_draw_primitives_instanced(void * object, NSUInteger primitive_type, NSUInteger vertex_start, NSUInteger vertex_count, NSUInteger instance_count) {
    [(id<MTLRenderCommandEncoder>)object drawPrimitives:(MTLPrimitiveType)primitive_type vertexStart:vertex_start vertexCount:vertex_count instanceCount:instance_count];
}

void metal_RenderCommandEncoder_draw_primitives_instanced_base_instance(void * object, NSUInteger primitive_type, NSUInteger vertex_start, NSUInteger vertex_count, NSUInteger instance_count, NSUInteger base_instance) {
    [(id<MTLRenderCommandEncoder>)object drawPrimitives:(MTLPrimitiveType)primitive_type vertexStart:vertex_start vertexCount:vertex_count instanceCount:instance_count baseInstance:base_instance];
}

void metal_RenderCommandEncoder_draw_primitives_indirect(void * object, NSUInteger primitive_type, void * indirect_buffer, NSUInteger indirect_buffer_offset) {
    [(id<MTLRenderCommandEncoder>)object drawPrimitives:(MTLPrimitiveType)primitive_type indirectBuffer:(id)indirect_buffer indirectBufferOffset:indirect_buffer_offset];
}

void metal_RenderCommandEncoder_draw_indexed_primitives(void * object, NSUInteger primitive_type, NSUInteger index_count, NSUInteger index_type, void * index_buffer, NSUInteger index_buffer_offset) {
    [(id<MTLRenderCommandEncoder>)object drawIndexedPrimitives:(MTLPrimitiveType)primitive_type indexCount:index_count indexType:(MTLIndexType)index_type indexBuffer:(id)index_buffer indexBufferOffset:index_buffer_offset];
}

void metal_RenderCommandEncoder_draw_indexed_primitives_instanced(void * object, NSUInteger primitive_type, NSUInteger index_count, NSUInteger index_type, void * index_buffer, NSUInteger index_buffer_offset, NSUInteger instance_count) {
    [(id<MTLRenderCommandEncoder>)object drawIndexedPrimitives:(MTLPrimitiveType)primitive_type indexCount:index_count indexType:(MTLIndexType)index_type indexBuffer:(id)index_buffer indexBufferOffset:index_buffer_offset instanceCount:instance_count];
}

void metal_RenderCommandEncoder_draw_indexed_primitives_instanced_base_instance(void * object, NSUInteger primitive_type, NSUInteger index_count, NSUInteger index_type, void * index_buffer, NSUInteger index_buffer_offset, NSUInteger instance_count, NSInteger base_vertex, NSUInteger base_instance) {
    [(id<MTLRenderCommandEncoder>)object drawIndexedPrimitives:(MTLPrimitiveType)primitive_type indexCount:index_count indexType:(MTLIndexType)index_type indexBuffer:(id)index_buffer indexBufferOffset:index_buffer_offset instanceCount:instance_count baseVertex:base_vertex baseInstance:base_instance];
}

void metal_RenderCommandEncoder_draw_indexed_primitives_indirect(void * object, NSUInteger primitive_type, NSUInteger index_type, void * index_buffer, NSUInteger index_buffer_offset, void * indirect_buffer, NSUInteger indirect_buffer_offset) {
    [(id<MTLRenderCommandEncoder>)object drawIndexedPrimitives:(MTLPrimitiveType)primitive_type indexType:(MTLIndexType)index_type indexBuffer:(id)index_buffer indexBufferOffset:index_buffer_offset indirectBuffer:(id)indirect_buffer indirectBufferOffset:indirect_buffer_offset];
}

void metal_RenderCommandEncoder_draw_mesh_threadgroups(void * object, MTLSize threadgroups_per_grid, MTLSize threads_per_object_threadgroup, MTLSize threads_per_mesh_threadgroup) {
    [(id<MTLRenderCommandEncoder>)object drawMeshThreadgroups:threadgroups_per_grid threadsPerObjectThreadgroup:threads_per_object_threadgroup threadsPerMeshThreadgroup:threads_per_mesh_threadgroup];
}

void metal_RenderCommandEncoder_draw_mesh_threadgroups_with_indirect_buffer(void * object, void * indirect_buffer, NSUInteger indirect_buffer_offset, MTLSize threads_per_object_threadgroup, MTLSize threads_per_mesh_threadgroup) {
    [(id<MTLRenderCommandEncoder>)object drawMeshThreadgroupsWithIndirectBuffer:(id)indirect_buffer indirectBufferOffset:indirect_buffer_offset threadsPerObjectThreadgroup:threads_per_object_threadgroup threadsPerMeshThreadgroup:threads_per_mesh_threadgroup];
}

void metal_RenderCommandEncoder_draw_mesh_threads(void * object, MTLSize threads_per_grid, MTLSize threads_per_object_threadgroup, MTLSize threads_per_mesh_threadgroup) {
    [(id<MTLRenderCommandEncoder>)object drawMeshThreads:threads_per_grid threadsPerObjectThreadgroup:threads_per_object_threadgroup threadsPerMeshThreadgroup:threads_per_mesh_threadgroup];
}

void metal_RenderCommandEncoder_use_resource(void * object, void * resource, NSUInteger usage) {
    [(id<MTLRenderCommandEncoder>)object useResource:(id)resource usage:(MTLResourceUsage)usage];
}

void metal_RenderCommandEncoder_use_resource_at(void * object, void * resource, NSUInteger usage, NSUInteger stages) {
    [(id<MTLRenderCommandEncoder>)object useResource:(id)resource usage:(MTLResourceUsage)usage stages:(MTLRenderStages)stages];
}

void metal_RenderCommandEncoder_use_resources(void * object, const id * resources, NSUInteger usage, NSUInteger stages, NSUInteger resources_count) {
    [(id<MTLRenderCommandEncoder>)object useResources:resources count:resources_count usage:(MTLResourceUsage)usage stages:(MTLRenderStages)stages];
}

void metal_RenderCommandEncoder_use_heap(void * object, void * heap) {
    [(id<MTLRenderCommandEncoder>)object useHeap:(id)heap];
}

void metal_RenderCommandEncoder_use_heap_at(void * object, void * heap, NSUInteger stages) {
    [(id<MTLRenderCommandEncoder>)object useHeap:(id)heap stages:(MTLRenderStages)stages];
}

void metal_RenderCommandEncoder_use_heaps(void * object, const id * heaps, NSUInteger stages, NSUInteger heaps_count) {
    [(id<MTLRenderCommandEncoder>)object useHeaps:heaps count:heaps_count stages:(MTLRenderStages)stages];
}

void metal_RenderCommandEncoder_execute_commands_in_buffer(void * object, void * buffer, NSRange with_range) {
    [(id<MTLRenderCommandEncoder>)object executeCommandsInBuffer:(id)buffer withRange:with_range];
}

void metal_RenderCommandEncoder_update_fence(void * object, void * fence, NSUInteger after_stages) {
    [(id<MTLRenderCommandEncoder>)object updateFence:(id)fence afterStages:(MTLRenderStages)after_stages];
}

void metal_RenderCommandEncoder_wait_for_fence(void * object, void * fence, NSUInteger before_stages) {
    [(id<MTLRenderCommandEncoder>)object waitForFence:(id)fence beforeStages:(MTLRenderStages)before_stages];
}

void metal_RenderCommandEncoder_sample_counters_in_buffer(void * object, void * sample_buffer, NSUInteger sample_index, uint8_t with_barrier) {
    [(id<MTLRenderCommandEncoder>)object sampleCountersInBuffer:(id)sample_buffer atSampleIndex:sample_index withBarrier:with_barrier];
}

void metal_BlitCommandEncoder_synchronize_resource(void * object, void * resource) {
    [(id<MTLBlitCommandEncoder>)object synchronizeResource:(id)resource];
}

void metal_BlitCommandEncoder_fill_buffer(void * object, void * destination_buffer, NSRange range, uint8_t value) {
    [(id<MTLBlitCommandEncoder>)object fillBuffer:(id)destination_buffer range:range value:value];
}

void metal_BlitCommandEncoder_generate_mipmaps(void * object, void * texture) {
    [(id<MTLBlitCommandEncoder>)object generateMipmapsForTexture:(id)texture];
}

void metal_BlitCommandEncoder_copy_from_buffer(void * object, void * source_buffer, NSUInteger source_offset, void * destination_buffer, NSUInteger destination_offset, NSUInteger size) {
    [(id<MTLBlitCommandEncoder>)object copyFromBuffer:(id)source_buffer sourceOffset:source_offset toBuffer:(id)destination_buffer destinationOffset:destination_offset size:size];
}

void metal_BlitCommandEncoder_copy_from_texture(void * object, void * source_texture, NSUInteger source_slice, NSUInteger source_level, MTLOrigin source_origin, MTLSize source_size, void * destination_texture, NSUInteger destination_slice, NSUInteger destination_level, MTLOrigin destination_origin) {
    [(id<MTLBlitCommandEncoder>)object copyFromTexture:(id)source_texture sourceSlice:source_slice sourceLevel:source_level sourceOrigin:source_origin sourceSize:source_size toTexture:(id)destination_texture destinationSlice:destination_slice destinationLevel:destination_level destinationOrigin:destination_origin];
}

void metal_BlitCommandEncoder_copy_from_buffer_to_texture(void * object, void * source_buffer, NSUInteger source_offset, NSUInteger source_bytes_per_row, NSUInteger source_bytes_per_image, MTLSize source_size, void * destination_texture, NSUInteger destination_slice, NSUInteger destination_level, MTLOrigin destination_origin, NSUInteger options) {
    [(id<MTLBlitCommandEncoder>)object copyFromBuffer:(id)source_buffer sourceOffset:source_offset sourceBytesPerRow:source_bytes_per_row sourceBytesPerImage:source_bytes_per_image sourceSize:source_size toTexture:(id)destination_texture destinationSlice:destination_slice destinationLevel:destination_level destinationOrigin:destination_origin options:(MTLBlitOption)options];
}

void metal_BlitCommandEncoder_copy_from_texture_to_buffer(void * object, void * source_texture, NSUInteger source_slice, NSUInteger source_level, MTLOrigin source_origin, MTLSize source_size, void * destination_buffer, NSUInteger destination_offset, NSUInteger destination_bytes_per_row, NSUInteger destination_bytes_per_image, NSUInteger options) {
    [(id<MTLBlitCommandEncoder>)object copyFromTexture:(id)source_texture sourceSlice:source_slice sourceLevel:source_level sourceOrigin:source_origin sourceSize:source_size toBuffer:(id)destination_buffer destinationOffset:destination_offset destinationBytesPerRow:destination_bytes_per_row destinationBytesPerImage:destination_bytes_per_image options:(MTLBlitOption)options];
}

void metal_BlitCommandEncoder_optimize_contents_for_gpu_access(void * object, void * texture) {
    [(id<MTLBlitCommandEncoder>)object optimizeContentsForGPUAccess:(id)texture];
}

void metal_BlitCommandEncoder_optimize_contents_for_gpu_access_slice_level(void * object, void * texture, NSUInteger slice, NSUInteger level) {
    [(id<MTLBlitCommandEncoder>)object optimizeContentsForGPUAccess:(id)texture slice:slice level:level];
}

void metal_BlitCommandEncoder_optimize_contents_for_cpu_access(void * object, void * texture) {
    [(id<MTLBlitCommandEncoder>)object optimizeContentsForCPUAccess:(id)texture];
}

void metal_BlitCommandEncoder_optimize_contents_for_cpu_access_slice_level(void * object, void * texture, NSUInteger slice, NSUInteger level) {
    [(id<MTLBlitCommandEncoder>)object optimizeContentsForCPUAccess:(id)texture slice:slice level:level];
}

void metal_BlitCommandEncoder_update_fence(void * object, void * fence) {
    [(id<MTLBlitCommandEncoder>)object updateFence:(id)fence];
}

void metal_BlitCommandEncoder_wait_for_fence(void * object, void * fence) {
    [(id<MTLBlitCommandEncoder>)object waitForFence:(id)fence];
}

void metal_BlitCommandEncoder_copy_indirect_command_buffer(void * object, void * source, NSRange source_range, void * destination, NSUInteger destination_index) {
    [(id<MTLBlitCommandEncoder>)object copyIndirectCommandBuffer:(id)source sourceRange:source_range destination:(id)destination destinationIndex:destination_index];
}

void metal_BlitCommandEncoder_reset_commands_in_buffer(void * object, void * buffer, NSRange range) {
    [(id<MTLBlitCommandEncoder>)object resetCommandsInBuffer:(id)buffer withRange:range];
}

void metal_BlitCommandEncoder_optimize_indirect_command_buffer(void * object, void * buffer, NSRange range) {
    [(id<MTLBlitCommandEncoder>)object optimizeIndirectCommandBuffer:(id)buffer withRange:range];
}

void metal_BlitCommandEncoder_sample_counters_in_buffer(void * object, void * sample_buffer, NSUInteger sample_index, uint8_t with_barrier) {
    [(id<MTLBlitCommandEncoder>)object sampleCountersInBuffer:(id)sample_buffer atSampleIndex:sample_index withBarrier:with_barrier];
}

void metal_BlitCommandEncoder_resolve_counters(void * object, void * sample_buffer, NSRange range, void * destination_buffer, NSUInteger destination_offset) {
    [(id<MTLBlitCommandEncoder>)object resolveCounters:(id)sample_buffer inRange:range destinationBuffer:(id)destination_buffer destinationOffset:destination_offset];
}

void metal_ComputeCommandEncoder_set_compute_pipeline_state(void * object, void * state) {
    [(id<MTLComputeCommandEncoder>)object setComputePipelineState:(id)state];
}

void metal_ComputeCommandEncoder_set_buffer(void * object, NSUInteger index, void * buffer, NSUInteger offset) {
    [(id<MTLComputeCommandEncoder>)object setBuffer:(id)buffer offset:offset atIndex:index];
}

void metal_ComputeCommandEncoder_set_buffers(void * object, NSUInteger start_index, const id * data, const NSUInteger * offsets, NSUInteger data_count) {
    [(id<MTLComputeCommandEncoder>)object setBuffers:data offsets:offsets withRange:NSMakeRange(start_index, data_count)];
}

void metal_ComputeCommandEncoder_set_texture(void * object, NSUInteger index, void * texture) {
    [(id<MTLComputeCommandEncoder>)object setTexture:(id)texture atIndex:index];
}

void metal_ComputeCommandEncoder_set_textures(void * object, NSUInteger start_index, const id * data, NSUInteger data_count) {
    [(id<MTLComputeCommandEncoder>)object setTextures:data withRange:NSMakeRange(start_index, data_count)];
}

void metal_ComputeCommandEncoder_set_sampler_state(void * object, NSUInteger index, void * sampler) {
    [(id<MTLComputeCommandEncoder>)object setSamplerState:(id)sampler atIndex:index];
}

void metal_ComputeCommandEncoder_set_sampler_states(void * object, NSUInteger start_index, const id * data, NSUInteger data_count) {
    [(id<MTLComputeCommandEncoder>)object setSamplerStates:data withRange:NSMakeRange(start_index, data_count)];
}

void metal_ComputeCommandEncoder_set_sampler_state_with_lod(void * object, NSUInteger index, void * sampler, float lod_clamp_start, float lod_clamp_end) {
    [(id<MTLComputeCommandEncoder>)object setSamplerState:(id)sampler lodMinClamp:lod_clamp_start lodMaxClamp:lod_clamp_end atIndex:index];
}

void metal_ComputeCommandEncoder_set_bytes(void * object, NSUInteger index, NSUInteger length, const void * bytes) {
    [(id<MTLComputeCommandEncoder>)object setBytes:bytes length:length atIndex:index];
}

void metal_ComputeCommandEncoder_set_visible_function_table(void * object, NSUInteger buffer_index, void * visible_function_table) {
    [(id<MTLComputeCommandEncoder>)object setVisibleFunctionTable:(id)visible_function_table atBufferIndex:buffer_index];
}

void metal_ComputeCommandEncoder_set_visible_function_tables(void * object, NSUInteger buffer_start_index, const id * visible_function_tables, NSUInteger visible_function_tables_count) {
    [(id<MTLComputeCommandEncoder>)object setVisibleFunctionTables:visible_function_tables withBufferRange:NSMakeRange(buffer_start_index, visible_function_tables_count)];
}

void metal_ComputeCommandEncoder_dispatch_thread_groups(void * object, MTLSize thread_groups_count, MTLSize threads_per_threadgroup) {
    [(id<MTLComputeCommandEncoder>)object dispatchThreadgroups:thread_groups_count threadsPerThreadgroup:threads_per_threadgroup];
}

void metal_ComputeCommandEncoder_dispatch_threads(void * object, MTLSize threads_per_grid, MTLSize threads_per_thread_group) {
    [(id<MTLComputeCommandEncoder>)object dispatchThreads:threads_per_grid threadsPerThreadgroup:threads_per_thread_group];
}

void metal_ComputeCommandEncoder_dispatch_thread_groups_indirect(void * object, void * buffer, NSUInteger offset, MTLSize threads_per_threadgroup) {
    [(id<MTLComputeCommandEncoder>)object dispatchThreadgroupsWithIndirectBuffer:(id)buffer indirectBufferOffset:offset threadsPerThreadgroup:threads_per_threadgroup];
}

void metal_ComputeCommandEncoder_set_threadgroup_memory_length(void * object, NSUInteger at_index, NSUInteger size) {
    [(id<MTLComputeCommandEncoder>)object setThreadgroupMemoryLength:size atIndex:at_index];
}

void metal_ComputeCommandEncoder_memory_barrier_with_resources(void * object, const id * resources, NSUInteger resources_count) {
    [(id<MTLComputeCommandEncoder>)object memoryBarrierWithResources:resources count:resources_count];
}

void metal_ComputeCommandEncoder_use_resource(void * object, void * resource, NSUInteger usage) {
    [(id<MTLComputeCommandEncoder>)object useResource:(id)resource usage:(MTLResourceUsage)usage];
}

void metal_ComputeCommandEncoder_use_resources(void * object, const id * resources, NSUInteger usage, NSUInteger resources_count) {
    [(id<MTLComputeCommandEncoder>)object useResources:resources count:resources_count usage:(MTLResourceUsage)usage];
}

void metal_ComputeCommandEncoder_use_heap(void * object, void * heap) {
    [(id<MTLComputeCommandEncoder>)object useHeap:(id)heap];
}

void metal_ComputeCommandEncoder_use_heaps(void * object, const id * heaps, NSUInteger heaps_count) {
    [(id<MTLComputeCommandEncoder>)object useHeaps:heaps count:heaps_count];
}

void metal_ComputeCommandEncoder_update_fence(void * object, void * fence) {
    [(id<MTLComputeCommandEncoder>)object updateFence:(id)fence];
}

void metal_ComputeCommandEncoder_wait_for_fence(void * object, void * fence) {
    [(id<MTLComputeCommandEncoder>)object waitForFence:(id)fence];
}

void metal_ComputeCommandEncoder_set_acceleration_structure(void * object, NSUInteger index, void * accel) {
    [(id<MTLComputeCommandEncoder>)object setAccelerationStructure:(id)accel atBufferIndex:index];
}

void metal_ComputeCommandEncoder_set_intersection_function_table(void * object, NSUInteger index, void * table) {
    [(id<MTLComputeCommandEncoder>)object setIntersectionFunctionTable:(id)table atBufferIndex:index];
}

void metal_ComputeCommandEncoder_sample_counters_in_buffer(void * object, void * sample_buffer, NSUInteger sample_index, uint8_t with_barrier) {
    [(id<MTLComputeCommandEncoder>)object sampleCountersInBuffer:(id)sample_buffer atSampleIndex:sample_index withBarrier:with_barrier];
}

NSUInteger metal_ArgumentEncoder_encoded_length(void * object) {
    return [(id<MTLArgumentEncoder>)object encodedLength];
}

NSUInteger metal_ArgumentEncoder_alignment(void * object) {
    return [(id<MTLArgumentEncoder>)object alignment];
}

void metal_ArgumentEncoder_set_argument_buffer(void * object, void * buffer, NSUInteger offset) {
    [(id<MTLArgumentEncoder>)object setArgumentBuffer:(id)buffer offset:offset];
}

void metal_ArgumentEncoder_set_argument_buffer_to_element(void * object, NSUInteger array_element, void * buffer, NSUInteger offset) {
    [(id<MTLArgumentEncoder>)object setArgumentBuffer:(id)buffer startOffset:offset arrayElement:array_element];
}

void metal_ArgumentEncoder_set_buffer(void * object, NSUInteger at_index, void * buffer, NSUInteger offset) {
    [(id<MTLArgumentEncoder>)object setBuffer:(id)buffer offset:offset atIndex:at_index];
}

void metal_ArgumentEncoder_set_buffers(void * object, NSUInteger start_index, const id * data, const NSUInteger * offsets, NSUInteger data_count) {
    [(id<MTLArgumentEncoder>)object setBuffers:data offsets:offsets withRange:NSMakeRange(start_index, data_count)];
}

void metal_ArgumentEncoder_set_texture(void * object, NSUInteger at_index, void * texture) {
    [(id<MTLArgumentEncoder>)object setTexture:(id)texture atIndex:at_index];
}

void metal_ArgumentEncoder_set_textures(void * object, NSUInteger start_index, const id * data, NSUInteger data_count) {
    [(id<MTLArgumentEncoder>)object setTextures:data withRange:NSMakeRange(start_index, data_count)];
}

void metal_ArgumentEncoder_set_sampler_state(void * object, NSUInteger at_index, void * sampler_state) {
    [(id<MTLArgumentEncoder>)object setSamplerState:(id)sampler_state atIndex:at_index];
}

void metal_ArgumentEncoder_set_sampler_states(void * object, NSUInteger start_index, const id * data, NSUInteger data_count) {
    [(id<MTLArgumentEncoder>)object setSamplerStates:data withRange:NSMakeRange(start_index, data_count)];
}

void metal_ArgumentEncoder_set_render_pipeline_state(void * object, NSUInteger at_index, void * pipeline) {
    [(id<MTLArgumentEncoder>)object setRenderPipelineState:(id)pipeline atIndex:at_index];
}

void metal_ArgumentEncoder_set_render_pipeline_states(void * object, NSUInteger start_index, const id * pipelines, NSUInteger pipelines_count) {
    [(id<MTLArgumentEncoder>)object setRenderPipelineStates:pipelines withRange:NSMakeRange(start_index, pipelines_count)];
}

void * metal_ArgumentEncoder_constant_data(void * object, NSUInteger at_index) {
    return [(id<MTLArgumentEncoder>)object constantDataAtIndex:at_index];
}

void metal_ArgumentEncoder_set_indirect_command_buffer(void * object, NSUInteger at_index, void * buffer) {
    [(id<MTLArgumentEncoder>)object setIndirectCommandBuffer:(id)buffer atIndex:at_index];
}

void metal_ArgumentEncoder_set_indirect_command_buffers(void * object, NSUInteger start_index, const id * data, NSUInteger data_count) {
    [(id<MTLArgumentEncoder>)object setIndirectCommandBuffers:data withRange:NSMakeRange(start_index, data_count)];
}

void * metal_ArgumentEncoder_new_argument_encoder_for_buffer(void * object, NSUInteger index) {
    return [(id<MTLArgumentEncoder>)object newArgumentEncoderForBufferAtIndex:index];
}

void * metal_IndirectCommandBufferDescriptor_new(void) {
    return [MTLIndirectCommandBufferDescriptor new];
}

NSUInteger metal_IndirectCommandBufferDescriptor_command_types(void * object) {
    return [(MTLIndirectCommandBufferDescriptor *)object commandTypes];
}

void metal_IndirectCommandBufferDescriptor_set_command_types(void * object, NSUInteger types) {
    [(MTLIndirectCommandBufferDescriptor *)object setCommandTypes:(MTLIndirectCommandType)types];
}

uint8_t metal_IndirectCommandBufferDescriptor_inherit_buffers(void * object) {
    return ([(MTLIndirectCommandBufferDescriptor *)object inheritBuffers] ? 1 : 0);
}

void metal_IndirectCommandBufferDescriptor_set_inherit_buffers(void * object, uint8_t inherit) {
    [(MTLIndirectCommandBufferDescriptor *)object setInheritBuffers:inherit];
}

uint8_t metal_IndirectCommandBufferDescriptor_inherit_pipeline_state(void * object) {
    return ([(MTLIndirectCommandBufferDescriptor *)object inheritPipelineState] ? 1 : 0);
}

void metal_IndirectCommandBufferDescriptor_set_inherit_pipeline_state(void * object, uint8_t inherit) {
    [(MTLIndirectCommandBufferDescriptor *)object setInheritPipelineState:inherit];
}

NSUInteger metal_IndirectCommandBufferDescriptor_max_vertex_buffer_bind_count(void * object) {
    return [(MTLIndirectCommandBufferDescriptor *)object maxVertexBufferBindCount];
}

void metal_IndirectCommandBufferDescriptor_set_max_vertex_buffer_bind_count(void * object, NSUInteger count) {
    [(MTLIndirectCommandBufferDescriptor *)object setMaxVertexBufferBindCount:count];
}

NSUInteger metal_IndirectCommandBufferDescriptor_max_fragment_buffer_bind_count(void * object) {
    return [(MTLIndirectCommandBufferDescriptor *)object maxFragmentBufferBindCount];
}

void metal_IndirectCommandBufferDescriptor_set_max_fragment_buffer_bind_count(void * object, NSUInteger count) {
    [(MTLIndirectCommandBufferDescriptor *)object setMaxFragmentBufferBindCount:count];
}

NSUInteger metal_IndirectCommandBufferDescriptor_max_kernel_buffer_bind_count(void * object) {
    return [(MTLIndirectCommandBufferDescriptor *)object maxKernelBufferBindCount];
}

void metal_IndirectCommandBufferDescriptor_set_max_kernel_buffer_bind_count(void * object, NSUInteger count) {
    [(MTLIndirectCommandBufferDescriptor *)object setMaxKernelBufferBindCount:count];
}

NSUInteger metal_IndirectCommandBuffer_size(void * object) {
    return [(id<MTLIndirectCommandBuffer>)object size];
}

void * metal_IndirectCommandBuffer_indirect_render_command_at_index(void * object, NSUInteger index) {
    return [(id<MTLIndirectCommandBuffer>)object indirectRenderCommandAtIndex:index];
}

void * metal_IndirectCommandBuffer_indirect_compute_command_at_index(void * object, NSUInteger index) {
    return [(id<MTLIndirectCommandBuffer>)object indirectComputeCommandAtIndex:index];
}

void metal_IndirectCommandBuffer_reset_with_range(void * object, NSRange range) {
    [(id<MTLIndirectCommandBuffer>)object resetWithRange:range];
}

void metal_IndirectRenderCommand_set_render_pipeline_state(void * object, void * pipeline_state) {
    [(id<MTLIndirectRenderCommand>)object setRenderPipelineState:(id)pipeline_state];
}

void metal_IndirectRenderCommand_set_vertex_buffer(void * object, NSUInteger index, void * buffer, NSUInteger offset) {
    [(id<MTLIndirectRenderCommand>)object setVertexBuffer:(id)buffer offset:offset atIndex:index];
}

void metal_IndirectRenderCommand_set_fragment_buffer(void * object, NSUInteger index, void * buffer, NSUInteger offset) {
    [(id<MTLIndirectRenderCommand>)object setFragmentBuffer:(id)buffer offset:offset atIndex:index];
}

void metal_IndirectRenderCommand_draw_primitives(void * object, NSUInteger primitive_type, NSUInteger vertex_start, NSUInteger vertex_count, NSUInteger instance_count, NSUInteger base_instance) {
    [(id<MTLIndirectRenderCommand>)object drawPrimitives:(MTLPrimitiveType)primitive_type vertexStart:vertex_start vertexCount:vertex_count instanceCount:instance_count baseInstance:base_instance];
}

void metal_IndirectRenderCommand_draw_indexed_primitives(void * object, NSUInteger primitive_type, NSUInteger index_count, NSUInteger index_type, void * index_buffer, NSUInteger index_buffer_offset, NSUInteger instance_count, NSUInteger base_vertex, NSUInteger base_instance) {
    [(id<MTLIndirectRenderCommand>)object drawIndexedPrimitives:(MTLPrimitiveType)primitive_type indexCount:index_count indexType:(MTLIndexType)index_type indexBuffer:(id)index_buffer indexBufferOffset:index_buffer_offset instanceCount:instance_count baseVertex:base_vertex baseInstance:base_instance];
}

void metal_IndirectRenderCommand_draw_patches(void * object, NSUInteger number_of_patch_control_points, NSUInteger patch_start, NSUInteger patch_count, void * patch_index_buffer, NSUInteger patch_index_buffer_offset, NSUInteger instance_count, NSUInteger base_instance, void * tesselation_factor_buffer, NSUInteger tesselation_factor_buffer_offset, NSUInteger tesselation_factor_buffer_instance_stride) {
    [(id<MTLIndirectRenderCommand>)object drawPatches:number_of_patch_control_points patchStart:patch_start patchCount:patch_count patchIndexBuffer:(id)patch_index_buffer patchIndexBufferOffset:patch_index_buffer_offset instanceCount:instance_count baseInstance:base_instance tessellationFactorBuffer:(id)tesselation_factor_buffer tessellationFactorBufferOffset:tesselation_factor_buffer_offset tessellationFactorBufferInstanceStride:tesselation_factor_buffer_instance_stride];
}

void metal_IndirectRenderCommand_draw_indexed_patches(void * object, NSUInteger number_of_patch_control_points, NSUInteger patch_start, NSUInteger patch_count, void * patch_index_buffer, NSUInteger patch_index_buffer_offset, void * control_point_index_buffer, NSUInteger control_point_index_buffer_offset, NSUInteger instance_count, NSUInteger base_instance, void * tesselation_factor_buffer, NSUInteger tesselation_factor_buffer_offset, NSUInteger tesselation_factor_buffer_instance_stride) {
    [(id<MTLIndirectRenderCommand>)object drawIndexedPatches:number_of_patch_control_points patchStart:patch_start patchCount:patch_count patchIndexBuffer:(id)patch_index_buffer patchIndexBufferOffset:patch_index_buffer_offset controlPointIndexBuffer:(id)control_point_index_buffer controlPointIndexBufferOffset:control_point_index_buffer_offset instanceCount:instance_count baseInstance:base_instance tessellationFactorBuffer:(id)tesselation_factor_buffer tessellationFactorBufferOffset:tesselation_factor_buffer_offset tessellationFactorBufferInstanceStride:tesselation_factor_buffer_instance_stride];
}

void metal_IndirectRenderCommand_reset(void * object) {
    [(id<MTLIndirectRenderCommand>)object reset];
}

void metal_IndirectComputeCommand_set_compute_pipeline_state(void * object, void * state) {
    [(id<MTLIndirectComputeCommand>)object setComputePipelineState:(id)state];
}

void metal_IndirectComputeCommand_set_kernel_buffer(void * object, NSUInteger index, void * buffer, NSUInteger offset) {
    [(id<MTLIndirectComputeCommand>)object setKernelBuffer:(id)buffer offset:offset atIndex:index];
}

void metal_IndirectComputeCommand_set_threadgroup_memory_length(void * object, NSUInteger index, NSUInteger length) {
    [(id<MTLIndirectComputeCommand>)object setThreadgroupMemoryLength:length atIndex:index];
}

void metal_IndirectComputeCommand_set_stage_in_region(void * object, MTLRegion region) {
    [(id<MTLIndirectComputeCommand>)object setStageInRegion:region];
}

void metal_IndirectComputeCommand_set_barrier(void * object) {
    [(id<MTLIndirectComputeCommand>)object setBarrier];
}

void metal_IndirectComputeCommand_clear_barrier(void * object) {
    [(id<MTLIndirectComputeCommand>)object clearBarrier];
}

void metal_IndirectComputeCommand_concurrent_dispatch_threadgroups(void * object, MTLSize thread_groups_per_grid, MTLSize threads_per_threadgroup) {
    [(id<MTLIndirectComputeCommand>)object concurrentDispatchThreadgroups:thread_groups_per_grid threadsPerThreadgroup:threads_per_threadgroup];
}

void metal_IndirectComputeCommand_concurrent_dispatch_threads(void * object, MTLSize thread_groups_per_grid, MTLSize threads_per_threadgroup) {
    [(id<MTLIndirectComputeCommand>)object concurrentDispatchThreads:thread_groups_per_grid threadsPerThreadgroup:threads_per_threadgroup];
}

void metal_IndirectComputeCommand_reset(void * object) {
    [(id<MTLIndirectComputeCommand>)object reset];
}
