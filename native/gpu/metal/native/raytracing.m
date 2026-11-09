// SPDX-License-Identifier: MIT OR Apache-2.0
// Ported from metal-rs; copyright GFX developers (2020, 2023).

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalPerformanceShaders/MetalPerformanceShaders.h>
#include <stdint.h>

// Manual reference counting: constructors return +1; object getters return +0.
// Raw object arrays are borrowed only for the duration of each bridge call.

void * metal_PrimitiveAccelerationStructureDescriptor_descriptor(void) {
    return [[MTLPrimitiveAccelerationStructureDescriptor descriptor] retain];
}

void metal_PrimitiveAccelerationStructureDescriptor_set_geometry_descriptors(void * object, void * descriptors) {
    [((MTLPrimitiveAccelerationStructureDescriptor *)object) setGeometryDescriptors:(NSArray *)descriptors];
}

MTLResourceID metal_AccelerationStructure_gpu_resource_id(void * object) {
    return [((id<MTLAccelerationStructure>)object) gpuResourceID];
}

void metal_AccelerationStructureGeometryDescriptor_set_opaque(void * object, uint8_t opaque) {
    [((MTLAccelerationStructureGeometryDescriptor *)object) setOpaque:(opaque != 0)];
}

void metal_AccelerationStructureGeometryDescriptor_set_primitive_data_buffer(void * object, void * buffer) {
    [((MTLAccelerationStructureGeometryDescriptor *)object) setPrimitiveDataBuffer:(id<MTLBuffer>)buffer];
}

void metal_AccelerationStructureGeometryDescriptor_set_primitive_data_stride(void * object, uint64_t stride) {
    [((MTLAccelerationStructureGeometryDescriptor *)object) setPrimitiveDataStride:stride];
}

void metal_AccelerationStructureGeometryDescriptor_set_primitive_data_element_size(void * object, uint64_t size) {
    [((MTLAccelerationStructureGeometryDescriptor *)object) setPrimitiveDataElementSize:size];
}

void metal_AccelerationStructureGeometryDescriptor_set_intersection_function_table_offset(void * object, uint64_t offset) {
    [((MTLAccelerationStructureGeometryDescriptor *)object) setIntersectionFunctionTableOffset:offset];
}

void * metal_AccelerationStructureTriangleGeometryDescriptor_descriptor(void) {
    return [[MTLAccelerationStructureTriangleGeometryDescriptor descriptor] retain];
}

void metal_AccelerationStructureTriangleGeometryDescriptor_set_index_buffer(void * object, void * buffer) {
    [((MTLAccelerationStructureTriangleGeometryDescriptor *)object) setIndexBuffer:(id<MTLBuffer>)buffer];
}

void metal_AccelerationStructureTriangleGeometryDescriptor_set_index_buffer_offset(void * object, uint64_t offset) {
    [((MTLAccelerationStructureTriangleGeometryDescriptor *)object) setIndexBufferOffset:offset];
}

void metal_AccelerationStructureTriangleGeometryDescriptor_set_index_type(void * object, uint64_t t) {
    [((MTLAccelerationStructureTriangleGeometryDescriptor *)object) setIndexType:(MTLIndexType)t];
}

void metal_AccelerationStructureTriangleGeometryDescriptor_set_vertex_buffer(void * object, void * buffer) {
    [((MTLAccelerationStructureTriangleGeometryDescriptor *)object) setVertexBuffer:(id<MTLBuffer>)buffer];
}

void metal_AccelerationStructureTriangleGeometryDescriptor_set_vertex_buffer_offset(void * object, uint64_t offset) {
    [((MTLAccelerationStructureTriangleGeometryDescriptor *)object) setVertexBufferOffset:offset];
}

void metal_AccelerationStructureTriangleGeometryDescriptor_set_vertex_stride(void * object, uint64_t stride) {
    [((MTLAccelerationStructureTriangleGeometryDescriptor *)object) setVertexStride:stride];
}

void metal_AccelerationStructureTriangleGeometryDescriptor_set_triangle_count(void * object, uint64_t count) {
    [((MTLAccelerationStructureTriangleGeometryDescriptor *)object) setTriangleCount:count];
}

void metal_AccelerationStructureTriangleGeometryDescriptor_set_vertex_format(void * object, uint64_t format) {
    [((MTLAccelerationStructureTriangleGeometryDescriptor *)object) setVertexFormat:(MTLAttributeFormat)format];
}

void metal_AccelerationStructureTriangleGeometryDescriptor_set_transformation_matrix_buffer(void * object, void * buffer) {
    [((MTLAccelerationStructureTriangleGeometryDescriptor *)object) setTransformationMatrixBuffer:(id<MTLBuffer>)buffer];
}

void metal_AccelerationStructureTriangleGeometryDescriptor_set_transformation_matrix_buffer_offset(void * object, uint64_t offset) {
    [((MTLAccelerationStructureTriangleGeometryDescriptor *)object) setTransformationMatrixBufferOffset:offset];
}

void * metal_AccelerationStructureBoundingBoxGeometryDescriptor_descriptor(void) {
    return [[MTLAccelerationStructureBoundingBoxGeometryDescriptor descriptor] retain];
}

void metal_AccelerationStructureBoundingBoxGeometryDescriptor_set_bounding_box_buffer(void * object, void * buffer) {
    [((MTLAccelerationStructureBoundingBoxGeometryDescriptor *)object) setBoundingBoxBuffer:(id<MTLBuffer>)buffer];
}

void metal_AccelerationStructureBoundingBoxGeometryDescriptor_set_bounding_box_count(void * object, uint64_t count) {
    [((MTLAccelerationStructureBoundingBoxGeometryDescriptor *)object) setBoundingBoxCount:count];
}

void metal_AccelerationStructureBoundingBoxGeometryDescriptor_set_bounding_box_stride(void * object, uint64_t stride) {
    [((MTLAccelerationStructureBoundingBoxGeometryDescriptor *)object) setBoundingBoxStride:stride];
}

void metal_AccelerationStructureBoundingBoxGeometryDescriptor_set_bounding_box_buffer_offset(void * object, uint64_t offset) {
    [((MTLAccelerationStructureBoundingBoxGeometryDescriptor *)object) setBoundingBoxBufferOffset:offset];
}

void * metal_InstanceAccelerationStructureDescriptor_descriptor(void) {
    return [[MTLInstanceAccelerationStructureDescriptor descriptor] retain];
}

void metal_InstanceAccelerationStructureDescriptor_set_instance_descriptor_type(void * object, uint64_t ty) {
    [((MTLInstanceAccelerationStructureDescriptor *)object) setInstanceDescriptorType:(MTLAccelerationStructureInstanceDescriptorType)ty];
}

void metal_InstanceAccelerationStructureDescriptor_set_instanced_acceleration_structures(void * object, void * instances) {
    [((MTLInstanceAccelerationStructureDescriptor *)object) setInstancedAccelerationStructures:(NSArray *)instances];
}

void metal_InstanceAccelerationStructureDescriptor_set_instance_count(void * object, uint64_t count) {
    [((MTLInstanceAccelerationStructureDescriptor *)object) setInstanceCount:count];
}

void metal_InstanceAccelerationStructureDescriptor_set_instance_descriptor_buffer(void * object, void * buffer) {
    [((MTLInstanceAccelerationStructureDescriptor *)object) setInstanceDescriptorBuffer:(id<MTLBuffer>)buffer];
}

void metal_InstanceAccelerationStructureDescriptor_set_instance_descriptor_buffer_offset(void * object, uint64_t offset) {
    [((MTLInstanceAccelerationStructureDescriptor *)object) setInstanceDescriptorBufferOffset:offset];
}

void metal_InstanceAccelerationStructureDescriptor_set_instance_descriptor_stride(void * object, uint64_t stride) {
    [((MTLInstanceAccelerationStructureDescriptor *)object) setInstanceDescriptorStride:stride];
}

void * metal_IndirectInstanceAccelerationStructureDescriptor_descriptor(void) {
    return [[MTLIndirectInstanceAccelerationStructureDescriptor descriptor] retain];
}

void metal_IndirectInstanceAccelerationStructureDescriptor_set_instance_descriptor_buffer(void * object, void * buffer) {
    [((MTLIndirectInstanceAccelerationStructureDescriptor *)object) setInstanceDescriptorBuffer:(id<MTLBuffer>)buffer];
}

void metal_IndirectInstanceAccelerationStructureDescriptor_set_instance_descriptor_buffer_offset(void * object, uint64_t offset) {
    [((MTLIndirectInstanceAccelerationStructureDescriptor *)object) setInstanceDescriptorBufferOffset:offset];
}

void metal_IndirectInstanceAccelerationStructureDescriptor_set_instance_descriptor_stride(void * object, uint64_t stride) {
    [((MTLIndirectInstanceAccelerationStructureDescriptor *)object) setInstanceDescriptorStride:stride];
}

void metal_IndirectInstanceAccelerationStructureDescriptor_set_max_instance_count(void * object, uint64_t count) {
    [((MTLIndirectInstanceAccelerationStructureDescriptor *)object) setMaxInstanceCount:count];
}

void metal_IndirectInstanceAccelerationStructureDescriptor_set_instance_count_buffer(void * object, void * buffer) {
    [((MTLIndirectInstanceAccelerationStructureDescriptor *)object) setInstanceCountBuffer:(id<MTLBuffer>)buffer];
}

void metal_IndirectInstanceAccelerationStructureDescriptor_set_instance_count_buffer_offset(void * object, uint64_t offset) {
    [((MTLIndirectInstanceAccelerationStructureDescriptor *)object) setInstanceCountBufferOffset:offset];
}

void metal_IndirectInstanceAccelerationStructureDescriptor_set_instance_descriptor_type(void * object, uint64_t ty) {
    [((MTLIndirectInstanceAccelerationStructureDescriptor *)object) setInstanceDescriptorType:(MTLAccelerationStructureInstanceDescriptorType)ty];
}

void metal_IndirectInstanceAccelerationStructureDescriptor_set_motion_transform_buffer(void * object, void * buffer) {
    [((MTLIndirectInstanceAccelerationStructureDescriptor *)object) setMotionTransformBuffer:(id<MTLBuffer>)buffer];
}

void metal_IndirectInstanceAccelerationStructureDescriptor_set_motion_transform_buffer_offset(void * object, uint64_t offset) {
    [((MTLIndirectInstanceAccelerationStructureDescriptor *)object) setMotionTransformBufferOffset:offset];
}

void metal_IndirectInstanceAccelerationStructureDescriptor_set_max_motion_transform_count(void * object, uint64_t count) {
    [((MTLIndirectInstanceAccelerationStructureDescriptor *)object) setMaxMotionTransformCount:count];
}

void metal_IndirectInstanceAccelerationStructureDescriptor_set_motion_transform_count_buffer(void * object, void * buffer) {
    [((MTLIndirectInstanceAccelerationStructureDescriptor *)object) setMotionTransformCountBuffer:(id<MTLBuffer>)buffer];
}

void metal_IndirectInstanceAccelerationStructureDescriptor_set_motion_transform_count_buffer_offset(void * object, uint64_t offset) {
    [((MTLIndirectInstanceAccelerationStructureDescriptor *)object) setMotionTransformCountBufferOffset:offset];
}

void metal_AccelerationStructureCommandEncoder_build_acceleration_structure(void * object, void * acceleration_structure, void * descriptor, void * scratch_buffer, uint64_t scratch_buffer_offset) {
    [((id<MTLAccelerationStructureCommandEncoder>)object) buildAccelerationStructure:(id<MTLAccelerationStructure>)acceleration_structure descriptor:(MTLAccelerationStructureDescriptor *)descriptor scratchBuffer:(id<MTLBuffer>)scratch_buffer scratchBufferOffset:scratch_buffer_offset];
}

void metal_AccelerationStructureCommandEncoder_copy_acceleration_structure(void * object, void * source_acceleration_structure, void * destination_acceleration_structure) {
    [((id<MTLAccelerationStructureCommandEncoder>)object) copyAccelerationStructure:(id<MTLAccelerationStructure>)source_acceleration_structure toAccelerationStructure:(id<MTLAccelerationStructure>)destination_acceleration_structure];
}

void metal_AccelerationStructureCommandEncoder_write_compacted_acceleration_structure_size(void * object, void * acceleration_structure, void * to_buffer, uint64_t offset) {
    [((id<MTLAccelerationStructureCommandEncoder>)object) writeCompactedAccelerationStructureSize:(id<MTLAccelerationStructure>)acceleration_structure toBuffer:(id<MTLBuffer>)to_buffer offset:offset];
}

void metal_AccelerationStructureCommandEncoder_write_compacted_acceleration_structure_size_with_type(void * object, void * acceleration_structure, void * to_buffer, uint64_t offset, uint64_t size_data_type) {
    [((id<MTLAccelerationStructureCommandEncoder>)object) writeCompactedAccelerationStructureSize:(id<MTLAccelerationStructure>)acceleration_structure toBuffer:(id<MTLBuffer>)to_buffer offset:offset sizeDataType:(MTLDataType)size_data_type];
}

void metal_AccelerationStructureCommandEncoder_copy_and_compact_acceleration_structure(void * object, void * source, void * destination) {
    [((id<MTLAccelerationStructureCommandEncoder>)object) copyAndCompactAccelerationStructure:(id<MTLAccelerationStructure>)source toAccelerationStructure:(id<MTLAccelerationStructure>)destination];
}

void metal_AccelerationStructureCommandEncoder_refit_acceleration_structure(void * object, void * source_acceleration_structure, void * descriptor, void * destination_acceleration_structure, void * scratch_buffer, uint64_t scratch_buffer_offset) {
    [((id<MTLAccelerationStructureCommandEncoder>)object) refitAccelerationStructure:(id<MTLAccelerationStructure>)source_acceleration_structure descriptor:(MTLAccelerationStructureDescriptor *)descriptor destination:(id<MTLAccelerationStructure>)destination_acceleration_structure scratchBuffer:(id<MTLBuffer>)scratch_buffer scratchBufferOffset:scratch_buffer_offset];
}

void metal_AccelerationStructureCommandEncoder_update_fence(void * object, void * fence) {
    [((id<MTLAccelerationStructureCommandEncoder>)object) updateFence:(id<MTLFence>)fence];
}

void metal_AccelerationStructureCommandEncoder_wait_for_fence(void * object, void * fence) {
    [((id<MTLAccelerationStructureCommandEncoder>)object) waitForFence:(id<MTLFence>)fence];
}

void metal_AccelerationStructureCommandEncoder_use_heap(void * object, void * heap) {
    [((id<MTLAccelerationStructureCommandEncoder>)object) useHeap:(id<MTLHeap>)heap];
}

void metal_AccelerationStructureCommandEncoder_use_heaps(void * object, void *const * heaps, uint64_t heaps_count) {
    [((id<MTLAccelerationStructureCommandEncoder>)object) useHeaps:(const id<MTLHeap> *)heaps count:heaps_count];
}

void metal_AccelerationStructureCommandEncoder_use_resource(void * object, void * resource, uint64_t usage) {
    [((id<MTLAccelerationStructureCommandEncoder>)object) useResource:(id<MTLResource>)resource usage:(MTLResourceUsage)usage];
}

void metal_AccelerationStructureCommandEncoder_use_resources(void * object, void *const * resources, uint64_t resources_count, uint64_t usage) {
    [((id<MTLAccelerationStructureCommandEncoder>)object) useResources:(const id<MTLResource> *)resources count:resources_count usage:(MTLResourceUsage)usage];
}

void metal_AccelerationStructureCommandEncoder_sample_counters_in_buffer(void * object, void * sample_buffer, uint64_t sample_index, uint8_t with_barrier) {
    [((id<MTLAccelerationStructureCommandEncoder>)object) sampleCountersInBuffer:(id<MTLCounterSampleBuffer>)sample_buffer atSampleIndex:sample_index withBarrier:(with_barrier != 0)];
}

void * metal_IntersectionFunctionTableDescriptor_new(void) {
    return [[MTLIntersectionFunctionTableDescriptor alloc] init];
}

void metal_IntersectionFunctionTableDescriptor_set_function_count(void * object, uint64_t count) {
    [((MTLIntersectionFunctionTableDescriptor *)object) setFunctionCount:count];
}

void metal_IntersectionFunctionTable_set_function(void * object, void * function, uint64_t index) {
    [((id<MTLIntersectionFunctionTable>)object) setFunction:(id<MTLFunctionHandle>)function atIndex:index];
}

void metal_IntersectionFunctionTable_set_functions(void * object, void *const * functions, uint64_t functions_count, uint64_t start_index) {
    [((id<MTLIntersectionFunctionTable>)object) setFunctions:(const id<MTLFunctionHandle> *)functions withRange:NSMakeRange(start_index, functions_count)];
}

void metal_IntersectionFunctionTable_set_buffer(void * object, uint64_t index, void * buffer, uint64_t offset) {
    [((id<MTLIntersectionFunctionTable>)object) setBuffer:(id<MTLBuffer>)buffer offset:offset atIndex:index];
}

void metal_IntersectionFunctionTable_set_buffers(void * object, uint64_t start_index, void *const * data, uint64_t data_count, const uint64_t * offsets) {
    [((id<MTLIntersectionFunctionTable>)object) setBuffers:(const id<MTLBuffer> *)data offsets:(const NSUInteger *)offsets withRange:NSMakeRange(start_index, data_count)];
}

void metal_IntersectionFunctionTable_set_visible_function_table(void * object, uint64_t buffer_index, void * visible_function_table) {
    [((id<MTLIntersectionFunctionTable>)object) setVisibleFunctionTable:(id<MTLVisibleFunctionTable>)visible_function_table atBufferIndex:buffer_index];
}

void metal_IntersectionFunctionTable_set_visible_function_tables(void * object, uint64_t buffer_start_index, void *const * visible_function_tables, uint64_t visible_function_tables_count) {
    [((id<MTLIntersectionFunctionTable>)object) setVisibleFunctionTables:(const id<MTLVisibleFunctionTable> *)visible_function_tables withBufferRange:NSMakeRange(buffer_start_index, visible_function_tables_count)];
}

MTLResourceID metal_IntersectionFunctionTable_gpu_resource_id(void * object) {
    return [((id<MTLIntersectionFunctionTable>)object) gpuResourceID];
}

void * metal_VisibleFunctionTableDescriptor_new(void) {
    return [[MTLVisibleFunctionTableDescriptor alloc] init];
}

void metal_VisibleFunctionTableDescriptor_set_function_count(void * object, uint64_t count) {
    [((MTLVisibleFunctionTableDescriptor *)object) setFunctionCount:count];
}

void metal_VisibleFunctionTable_set_functions(void * object, void *const * functions, uint64_t functions_count) {
    [((id<MTLVisibleFunctionTable>)object) setFunctions:(const id<MTLFunctionHandle> *)functions withRange:NSMakeRange(0, functions_count)];
}

void metal_VisibleFunctionTable_set_function(void * object, uint64_t index, void * function) {
    [((id<MTLVisibleFunctionTable>)object) setFunction:(id<MTLFunctionHandle>)function atIndex:index];
}

MTLResourceID metal_VisibleFunctionTable_gpu_resource_id(void * object) {
    return [((id<MTLVisibleFunctionTable>)object) gpuResourceID];
}

void * metal_RayIntersector_from_device(void * device) {
    return [[MPSRayIntersector alloc] initWithDevice:(id<MTLDevice>)device];
}

void metal_RayIntersector_set_cull_mode(void * object, uint64_t mode) {
    [((MPSRayIntersector *)object) setCullMode:(MTLCullMode)mode];
}

void metal_RayIntersector_set_front_facing_winding(void * object, uint64_t winding) {
    [((MPSRayIntersector *)object) setFrontFacingWinding:(MTLWinding)winding];
}

void metal_RayIntersector_set_intersection_data_type(void * object, uint64_t options) {
    [((MPSRayIntersector *)object) setIntersectionDataType:(MPSIntersectionDataType)options];
}

void metal_RayIntersector_set_intersection_stride(void * object, uint64_t stride) {
    [((MPSRayIntersector *)object) setIntersectionStride:stride];
}

void metal_RayIntersector_set_ray_data_type(void * object, uint64_t ty) {
    [((MPSRayIntersector *)object) setRayDataType:(MPSRayDataType)ty];
}

void metal_RayIntersector_set_ray_index_data_type(void * object, uint64_t ty) {
    [((MPSRayIntersector *)object) setRayIndexDataType:(MPSDataType)ty];
}

void metal_RayIntersector_set_ray_mask(void * object, uint32_t ray_mask) {
    [((MPSRayIntersector *)object) setRayMask:ray_mask];
}

void metal_RayIntersector_set_ray_mask_operator(void * object, uint64_t operator) {
    [((MPSRayIntersector *)object) setRayMaskOperator:(MPSRayMaskOperator)operator];
}

void metal_RayIntersector_set_ray_mask_options(void * object, uint64_t options) {
    [((MPSRayIntersector *)object) setRayMaskOptions:(MPSRayMaskOptions)options];
}

void metal_RayIntersector_set_ray_stride(void * object, uint64_t stride) {
    [((MPSRayIntersector *)object) setRayStride:stride];
}

void metal_RayIntersector_set_triangle_intersection_test_type(void * object, uint64_t test_type) {
    [((MPSRayIntersector *)object) setTriangleIntersectionTestType:(MPSTriangleIntersectionTestType)test_type];
}

void metal_RayIntersector_encode_intersection_to_command_buffer(void * object, void * command_buffer, uint64_t intersection_type, void * ray_buffer, uint64_t ray_buffer_offset, void * intersection_buffer, uint64_t intersection_buffer_offset, uint64_t ray_count, void * acceleration_structure) {
    [((MPSRayIntersector *)object) encodeIntersectionToCommandBuffer:(id<MTLCommandBuffer>)command_buffer intersectionType:(MPSIntersectionType)intersection_type rayBuffer:(id<MTLBuffer>)ray_buffer rayBufferOffset:ray_buffer_offset intersectionBuffer:(id<MTLBuffer>)intersection_buffer intersectionBufferOffset:intersection_buffer_offset rayCount:ray_count accelerationStructure:(MPSAccelerationStructure *)acceleration_structure];
}

uint64_t metal_RayIntersector_recommended_minimum_ray_batch_size_for_ray_count(void * object, uint64_t ray_count) {
    return [((MPSRayIntersector *)object) recommendedMinimumRayBatchSizeForRayCount:ray_count];
}

void * metal_AccelerationStructureGroup_new_with_device(void * device) {
    return [[MPSAccelerationStructureGroup alloc] initWithDevice:(id<MTLDevice>)device];
}

void * metal_AccelerationStructureGroup_device(void * object) {
    return [((MPSAccelerationStructureGroup *)object) device];
}

uint64_t metal_MPSAccelerationStructure_status(void * object) {
    return [((MPSAccelerationStructure *)object) status];
}

uint64_t metal_MPSAccelerationStructure_usage(void * object) {
    return [((MPSAccelerationStructure *)object) usage];
}

void metal_MPSAccelerationStructure_set_usage(void * object, uint64_t usage) {
    [((MPSAccelerationStructure *)object) setUsage:(MPSAccelerationStructureUsage)usage];
}

void * metal_MPSAccelerationStructure_group(void * object) {
    return [((MPSAccelerationStructure *)object) group];
}

void metal_MPSAccelerationStructure_encode_refit_to_command_buffer(void * object, void * buffer) {
    [((MPSAccelerationStructure *)object) encodeRefitToCommandBuffer:(id<MTLCommandBuffer>)buffer];
}

void metal_MPSAccelerationStructure_rebuild(void * object) {
    [((MPSAccelerationStructure *)object) rebuild];
}

void metal_PolygonAccelerationStructure_set_index_buffer(void * object, void * buffer) {
    [((MPSPolygonAccelerationStructure *)object) setIndexBuffer:(id<MTLBuffer>)buffer];
}

void metal_PolygonAccelerationStructure_set_index_buffer_offset(void * object, uint64_t offset) {
    [((MPSPolygonAccelerationStructure *)object) setIndexBufferOffset:offset];
}

void metal_PolygonAccelerationStructure_set_index_type(void * object, uint64_t data_type) {
    [((MPSPolygonAccelerationStructure *)object) setIndexType:(MPSDataType)data_type];
}

void metal_PolygonAccelerationStructure_set_mask_buffer(void * object, void * buffer) {
    [((MPSPolygonAccelerationStructure *)object) setMaskBuffer:(id<MTLBuffer>)buffer];
}

void metal_PolygonAccelerationStructure_set_mask_buffer_offset(void * object, uint64_t offset) {
    [((MPSPolygonAccelerationStructure *)object) setMaskBufferOffset:offset];
}

void metal_PolygonAccelerationStructure_set_vertex_buffer(void * object, void * buffer) {
    [((MPSPolygonAccelerationStructure *)object) setVertexBuffer:(id<MTLBuffer>)buffer];
}

void metal_PolygonAccelerationStructure_set_vertex_buffer_offset(void * object, uint64_t offset) {
    [((MPSPolygonAccelerationStructure *)object) setVertexBufferOffset:offset];
}

void metal_PolygonAccelerationStructure_set_vertex_stride(void * object, uint64_t stride) {
    [((MPSPolygonAccelerationStructure *)object) setVertexStride:stride];
}

void * metal_TriangleAccelerationStructure_from_device(void * device) {
    return [[MPSTriangleAccelerationStructure alloc] initWithDevice:(id<MTLDevice>)device];
}

uint64_t metal_TriangleAccelerationStructure_triangle_count(void * object) {
    return [((MPSTriangleAccelerationStructure *)object) triangleCount];
}

void metal_TriangleAccelerationStructure_set_triangle_count(void * object, uint64_t count) {
    [((MPSTriangleAccelerationStructure *)object) setTriangleCount:count];
}

void * metal_InstanceAccelerationStructure_init_with_group(void * group) {
    return [[MPSInstanceAccelerationStructure alloc] initWithGroup:(MPSAccelerationStructureGroup *)group];
}

void * metal_InstanceAccelerationStructure_acceleration_structures(void * object) {
    return [((MPSInstanceAccelerationStructure *)object) accelerationStructures];
}

void metal_InstanceAccelerationStructure_set_acceleration_structures(void * object, void *const * acs, uint64_t acs_count) {
    NSArray *array = [[NSArray alloc] initWithObjects:(const id *)acs count:acs_count];
    [((MPSInstanceAccelerationStructure *)object) setAccelerationStructures:array];
    [array release];
}

void * metal_InstanceAccelerationStructure_instance_buffer(void * object) {
    return [((MPSInstanceAccelerationStructure *)object) instanceBuffer];
}

void metal_InstanceAccelerationStructure_set_instance_buffer(void * object, void * buffer) {
    [((MPSInstanceAccelerationStructure *)object) setInstanceBuffer:(id<MTLBuffer>)buffer];
}

uint64_t metal_InstanceAccelerationStructure_instance_buffer_offset(void * object) {
    return [((MPSInstanceAccelerationStructure *)object) instanceBufferOffset];
}

void metal_InstanceAccelerationStructure_set_instance_buffer_offset(void * object, uint64_t offset) {
    [((MPSInstanceAccelerationStructure *)object) setInstanceBufferOffset:offset];
}

void * metal_InstanceAccelerationStructure_transform_buffer(void * object) {
    return [((MPSInstanceAccelerationStructure *)object) transformBuffer];
}

void metal_InstanceAccelerationStructure_set_transform_buffer(void * object, void * buffer) {
    [((MPSInstanceAccelerationStructure *)object) setTransformBuffer:(id<MTLBuffer>)buffer];
}

uint64_t metal_InstanceAccelerationStructure_transform_buffer_offset(void * object) {
    return [((MPSInstanceAccelerationStructure *)object) transformBufferOffset];
}

void metal_InstanceAccelerationStructure_set_transform_buffer_offset(void * object, uint64_t offset) {
    [((MPSInstanceAccelerationStructure *)object) setTransformBufferOffset:offset];
}

uint64_t metal_InstanceAccelerationStructure_transform_type(void * object) {
    return [((MPSInstanceAccelerationStructure *)object) transformType];
}

void metal_InstanceAccelerationStructure_set_transform_type(void * object, uint64_t transform_type) {
    [((MPSInstanceAccelerationStructure *)object) setTransformType:(MPSTransformType)transform_type];
}

void * metal_InstanceAccelerationStructure_mask_buffer(void * object) {
    return [((MPSInstanceAccelerationStructure *)object) maskBuffer];
}

void metal_InstanceAccelerationStructure_set_mask_buffer(void * object, void * buffer) {
    [((MPSInstanceAccelerationStructure *)object) setMaskBuffer:(id<MTLBuffer>)buffer];
}

uint64_t metal_InstanceAccelerationStructure_mask_buffer_offset(void * object) {
    return [((MPSInstanceAccelerationStructure *)object) maskBufferOffset];
}

void metal_InstanceAccelerationStructure_set_mask_buffer_offset(void * object, uint64_t offset) {
    [((MPSInstanceAccelerationStructure *)object) setMaskBufferOffset:offset];
}

uint64_t metal_InstanceAccelerationStructure_instance_count(void * object) {
    return [((MPSInstanceAccelerationStructure *)object) instanceCount];
}

void metal_InstanceAccelerationStructure_set_instance_count(void * object, uint64_t count) {
    [((MPSInstanceAccelerationStructure *)object) setInstanceCount:count];
}

uint8_t metal_mps_supports_device(void *device) {
    return MPSSupportsMTLDevice((id<MTLDevice>)device) ? 1 : 0;
}
