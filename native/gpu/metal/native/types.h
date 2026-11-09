// SPDX-License-Identifier: MIT OR Apache-2.0
// Ported from metal-rs, Copyright GFX developers.
#ifndef SILK_METAL_TYPES_H
#define SILK_METAL_TYPES_H

#include <stddef.h>
#include <stdint.h>
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

// Native APIs use SDK records directly; Silk stores the same C field layout.
// Upstream uses this shorter name for MTLVertexAmplificationViewMapping.
typedef MTLVertexAmplificationViewMapping VertexAmplificationViewMapping;

// Rust uses [f32; 2], not the SDK vector_float2. Field offsets and stride match,
// but Rust/Silk alignment is 4 while the SDK record alignment is 8. Use this
// non-conflicting C name when accessing a caller-owned Rust/Silk record.
typedef struct {
    float distance;
    uint32_t primitiveIndex;
    float coordinates[2];
} metal_MPSIntersectionDistancePrimitiveIndexCoordinates;

_Static_assert(sizeof(void *) == 8, "metal requires an Apple 64-bit target");
_Static_assert(sizeof(NSUInteger) == 8, "NSUInteger must match Silk u64");
_Static_assert(sizeof(NSInteger) == 8, "NSInteger must match Silk i64");
_Static_assert(sizeof(CGFloat) == 8, "CGFloat must match Silk f64");

// MTLSizeAndAlign: 16 bytes, alignment 8.
_Static_assert(sizeof(MTLSizeAndAlign) == 16, "MTLSizeAndAlign size");
_Static_assert(_Alignof(MTLSizeAndAlign) == 8, "MTLSizeAndAlign alignment");
_Static_assert(offsetof(MTLSizeAndAlign, size) == 0, "MTLSizeAndAlign.size offset");
_Static_assert(offsetof(MTLSizeAndAlign, align) == 8, "MTLSizeAndAlign.align offset");

// MTLAccelerationStructureInstanceDescriptor: 64 bytes, alignment 4.
_Static_assert(sizeof(MTLAccelerationStructureInstanceDescriptor) == 64, "MTLAccelerationStructureInstanceDescriptor size");
_Static_assert(_Alignof(MTLAccelerationStructureInstanceDescriptor) == 4, "MTLAccelerationStructureInstanceDescriptor alignment");
_Static_assert(offsetof(MTLAccelerationStructureInstanceDescriptor, transformationMatrix) == 0, "MTLAccelerationStructureInstanceDescriptor.transformation_matrix offset");
_Static_assert(offsetof(MTLAccelerationStructureInstanceDescriptor, options) == 48, "MTLAccelerationStructureInstanceDescriptor.options offset");
_Static_assert(offsetof(MTLAccelerationStructureInstanceDescriptor, mask) == 52, "MTLAccelerationStructureInstanceDescriptor.mask offset");
_Static_assert(offsetof(MTLAccelerationStructureInstanceDescriptor, intersectionFunctionTableOffset) == 56, "MTLAccelerationStructureInstanceDescriptor.intersection_function_table_offset offset");
_Static_assert(offsetof(MTLAccelerationStructureInstanceDescriptor, accelerationStructureIndex) == 60, "MTLAccelerationStructureInstanceDescriptor.acceleration_structure_index offset");

// MTLAccelerationStructureUserIDInstanceDescriptor: 68 bytes, alignment 4.
_Static_assert(sizeof(MTLAccelerationStructureUserIDInstanceDescriptor) == 68, "MTLAccelerationStructureUserIDInstanceDescriptor size");
_Static_assert(_Alignof(MTLAccelerationStructureUserIDInstanceDescriptor) == 4, "MTLAccelerationStructureUserIDInstanceDescriptor alignment");
_Static_assert(offsetof(MTLAccelerationStructureUserIDInstanceDescriptor, transformationMatrix) == 0, "MTLAccelerationStructureUserIDInstanceDescriptor.transformation_matrix offset");
_Static_assert(offsetof(MTLAccelerationStructureUserIDInstanceDescriptor, options) == 48, "MTLAccelerationStructureUserIDInstanceDescriptor.options offset");
_Static_assert(offsetof(MTLAccelerationStructureUserIDInstanceDescriptor, mask) == 52, "MTLAccelerationStructureUserIDInstanceDescriptor.mask offset");
_Static_assert(offsetof(MTLAccelerationStructureUserIDInstanceDescriptor, intersectionFunctionTableOffset) == 56, "MTLAccelerationStructureUserIDInstanceDescriptor.intersection_function_table_offset offset");
_Static_assert(offsetof(MTLAccelerationStructureUserIDInstanceDescriptor, accelerationStructureIndex) == 60, "MTLAccelerationStructureUserIDInstanceDescriptor.acceleration_structure_index offset");
_Static_assert(offsetof(MTLAccelerationStructureUserIDInstanceDescriptor, userID) == 64, "MTLAccelerationStructureUserIDInstanceDescriptor.user_id offset");

// MTLIndirectAccelerationStructureInstanceDescriptor: 72 bytes, alignment 8.
_Static_assert(sizeof(MTLIndirectAccelerationStructureInstanceDescriptor) == 72, "MTLIndirectAccelerationStructureInstanceDescriptor size");
_Static_assert(_Alignof(MTLIndirectAccelerationStructureInstanceDescriptor) == 8, "MTLIndirectAccelerationStructureInstanceDescriptor alignment");
_Static_assert(offsetof(MTLIndirectAccelerationStructureInstanceDescriptor, transformationMatrix) == 0, "MTLIndirectAccelerationStructureInstanceDescriptor.transformation_matrix offset");
_Static_assert(offsetof(MTLIndirectAccelerationStructureInstanceDescriptor, options) == 48, "MTLIndirectAccelerationStructureInstanceDescriptor.options offset");
_Static_assert(offsetof(MTLIndirectAccelerationStructureInstanceDescriptor, mask) == 52, "MTLIndirectAccelerationStructureInstanceDescriptor.mask offset");
_Static_assert(offsetof(MTLIndirectAccelerationStructureInstanceDescriptor, intersectionFunctionTableOffset) == 56, "MTLIndirectAccelerationStructureInstanceDescriptor.intersection_function_table_offset offset");
_Static_assert(offsetof(MTLIndirectAccelerationStructureInstanceDescriptor, userID) == 60, "MTLIndirectAccelerationStructureInstanceDescriptor.user_id offset");
_Static_assert(offsetof(MTLIndirectAccelerationStructureInstanceDescriptor, accelerationStructureID) == 64, "MTLIndirectAccelerationStructureInstanceDescriptor.acceleration_structure_id offset");

// MTLScissorRect: 32 bytes, alignment 8.
_Static_assert(sizeof(MTLScissorRect) == 32, "MTLScissorRect size");
_Static_assert(_Alignof(MTLScissorRect) == 8, "MTLScissorRect alignment");
_Static_assert(offsetof(MTLScissorRect, x) == 0, "MTLScissorRect.x offset");
_Static_assert(offsetof(MTLScissorRect, y) == 8, "MTLScissorRect.y offset");
_Static_assert(offsetof(MTLScissorRect, width) == 16, "MTLScissorRect.width offset");
_Static_assert(offsetof(MTLScissorRect, height) == 24, "MTLScissorRect.height offset");

// MTLViewport: 48 bytes, alignment 8.
_Static_assert(sizeof(MTLViewport) == 48, "MTLViewport size");
_Static_assert(_Alignof(MTLViewport) == 8, "MTLViewport alignment");
_Static_assert(offsetof(MTLViewport, originX) == 0, "MTLViewport.originX offset");
_Static_assert(offsetof(MTLViewport, originY) == 8, "MTLViewport.originY offset");
_Static_assert(offsetof(MTLViewport, width) == 16, "MTLViewport.width offset");
_Static_assert(offsetof(MTLViewport, height) == 24, "MTLViewport.height offset");
_Static_assert(offsetof(MTLViewport, znear) == 32, "MTLViewport.znear offset");
_Static_assert(offsetof(MTLViewport, zfar) == 40, "MTLViewport.zfar offset");

// MTLDrawPrimitivesIndirectArguments: 16 bytes, alignment 4.
_Static_assert(sizeof(MTLDrawPrimitivesIndirectArguments) == 16, "MTLDrawPrimitivesIndirectArguments size");
_Static_assert(_Alignof(MTLDrawPrimitivesIndirectArguments) == 4, "MTLDrawPrimitivesIndirectArguments alignment");
_Static_assert(offsetof(MTLDrawPrimitivesIndirectArguments, vertexCount) == 0, "MTLDrawPrimitivesIndirectArguments.vertexCount offset");
_Static_assert(offsetof(MTLDrawPrimitivesIndirectArguments, instanceCount) == 4, "MTLDrawPrimitivesIndirectArguments.instanceCount offset");
_Static_assert(offsetof(MTLDrawPrimitivesIndirectArguments, vertexStart) == 8, "MTLDrawPrimitivesIndirectArguments.vertexStart offset");
_Static_assert(offsetof(MTLDrawPrimitivesIndirectArguments, baseInstance) == 12, "MTLDrawPrimitivesIndirectArguments.baseInstance offset");

// MTLDrawIndexedPrimitivesIndirectArguments: 20 bytes, alignment 4.
_Static_assert(sizeof(MTLDrawIndexedPrimitivesIndirectArguments) == 20, "MTLDrawIndexedPrimitivesIndirectArguments size");
_Static_assert(_Alignof(MTLDrawIndexedPrimitivesIndirectArguments) == 4, "MTLDrawIndexedPrimitivesIndirectArguments alignment");
_Static_assert(offsetof(MTLDrawIndexedPrimitivesIndirectArguments, indexCount) == 0, "MTLDrawIndexedPrimitivesIndirectArguments.indexCount offset");
_Static_assert(offsetof(MTLDrawIndexedPrimitivesIndirectArguments, instanceCount) == 4, "MTLDrawIndexedPrimitivesIndirectArguments.instanceCount offset");
_Static_assert(offsetof(MTLDrawIndexedPrimitivesIndirectArguments, indexStart) == 8, "MTLDrawIndexedPrimitivesIndirectArguments.indexStart offset");
_Static_assert(offsetof(MTLDrawIndexedPrimitivesIndirectArguments, baseVertex) == 12, "MTLDrawIndexedPrimitivesIndirectArguments.baseVertex offset");
_Static_assert(offsetof(MTLDrawIndexedPrimitivesIndirectArguments, baseInstance) == 16, "MTLDrawIndexedPrimitivesIndirectArguments.baseInstance offset");

// VertexAmplificationViewMapping: 8 bytes, alignment 4.
_Static_assert(sizeof(VertexAmplificationViewMapping) == 8, "VertexAmplificationViewMapping size");
_Static_assert(_Alignof(VertexAmplificationViewMapping) == 4, "VertexAmplificationViewMapping alignment");
_Static_assert(offsetof(VertexAmplificationViewMapping, viewportArrayIndexOffset) == 0, "VertexAmplificationViewMapping.viewportArrayIndexOffset offset");
_Static_assert(offsetof(VertexAmplificationViewMapping, renderTargetArrayIndexOffset) == 4, "VertexAmplificationViewMapping.renderTargetArrayIndexOffset offset");

// MTLOrigin: 24 bytes, alignment 8.
_Static_assert(sizeof(MTLOrigin) == 24, "MTLOrigin size");
_Static_assert(_Alignof(MTLOrigin) == 8, "MTLOrigin alignment");
_Static_assert(offsetof(MTLOrigin, x) == 0, "MTLOrigin.x offset");
_Static_assert(offsetof(MTLOrigin, y) == 8, "MTLOrigin.y offset");
_Static_assert(offsetof(MTLOrigin, z) == 16, "MTLOrigin.z offset");

// MTLSize: 24 bytes, alignment 8.
_Static_assert(sizeof(MTLSize) == 24, "MTLSize size");
_Static_assert(_Alignof(MTLSize) == 8, "MTLSize alignment");
_Static_assert(offsetof(MTLSize, width) == 0, "MTLSize.width offset");
_Static_assert(offsetof(MTLSize, height) == 8, "MTLSize.height offset");
_Static_assert(offsetof(MTLSize, depth) == 16, "MTLSize.depth offset");

// MTLRegion: 48 bytes, alignment 8.
_Static_assert(sizeof(MTLRegion) == 48, "MTLRegion size");
_Static_assert(_Alignof(MTLRegion) == 8, "MTLRegion alignment");
_Static_assert(offsetof(MTLRegion, origin) == 0, "MTLRegion.origin offset");
_Static_assert(offsetof(MTLRegion, size) == 24, "MTLRegion.size offset");

// MTLSamplePosition: 8 bytes, alignment 4.
_Static_assert(sizeof(MTLSamplePosition) == 8, "MTLSamplePosition size");
_Static_assert(_Alignof(MTLSamplePosition) == 4, "MTLSamplePosition alignment");
_Static_assert(offsetof(MTLSamplePosition, x) == 0, "MTLSamplePosition.x offset");
_Static_assert(offsetof(MTLSamplePosition, y) == 4, "MTLSamplePosition.y offset");

// MTLResourceID: 8 bytes, alignment 8.
_Static_assert(sizeof(MTLResourceID) == 8, "MTLResourceID size");
_Static_assert(_Alignof(MTLResourceID) == 8, "MTLResourceID alignment");
_Static_assert(offsetof(MTLResourceID, _impl) == 0, "MTLResourceID._impl offset");

// MPSPackedFloat3: 12 bytes, alignment 4.
_Static_assert(sizeof(MPSPackedFloat3) == 12, "MPSPackedFloat3 size");
_Static_assert(_Alignof(MPSPackedFloat3) == 4, "MPSPackedFloat3 alignment");
_Static_assert(offsetof(MPSPackedFloat3, elements) == 0, "MPSPackedFloat3.elements offset");

// MPSRayOriginMinDistanceDirectionMaxDistance: 32 bytes, alignment 4.
_Static_assert(sizeof(MPSRayOriginMinDistanceDirectionMaxDistance) == 32, "MPSRayOriginMinDistanceDirectionMaxDistance size");
_Static_assert(_Alignof(MPSRayOriginMinDistanceDirectionMaxDistance) == 4, "MPSRayOriginMinDistanceDirectionMaxDistance alignment");
_Static_assert(offsetof(MPSRayOriginMinDistanceDirectionMaxDistance, origin) == 0, "MPSRayOriginMinDistanceDirectionMaxDistance.origin offset");
_Static_assert(offsetof(MPSRayOriginMinDistanceDirectionMaxDistance, minDistance) == 12, "MPSRayOriginMinDistanceDirectionMaxDistance.min_distance offset");
_Static_assert(offsetof(MPSRayOriginMinDistanceDirectionMaxDistance, direction) == 16, "MPSRayOriginMinDistanceDirectionMaxDistance.direction offset");
_Static_assert(offsetof(MPSRayOriginMinDistanceDirectionMaxDistance, maxDistance) == 28, "MPSRayOriginMinDistanceDirectionMaxDistance.max_distance offset");

// MPSIntersectionDistancePrimitiveIndexCoordinates: 16 bytes, alignment 4.
_Static_assert(sizeof(metal_MPSIntersectionDistancePrimitiveIndexCoordinates) == 16, "MPSIntersectionDistancePrimitiveIndexCoordinates size");
_Static_assert(_Alignof(metal_MPSIntersectionDistancePrimitiveIndexCoordinates) == 4, "MPSIntersectionDistancePrimitiveIndexCoordinates alignment");
_Static_assert(offsetof(metal_MPSIntersectionDistancePrimitiveIndexCoordinates, distance) == 0, "MPSIntersectionDistancePrimitiveIndexCoordinates.distance offset");
_Static_assert(offsetof(metal_MPSIntersectionDistancePrimitiveIndexCoordinates, primitiveIndex) == 4, "MPSIntersectionDistancePrimitiveIndexCoordinates.primitive_index offset");
_Static_assert(offsetof(metal_MPSIntersectionDistancePrimitiveIndexCoordinates, coordinates) == 8, "MPSIntersectionDistancePrimitiveIndexCoordinates.coordinates offset");

// MTLAccelerationStructureSizes: 24 bytes, alignment 8.
_Static_assert(sizeof(MTLAccelerationStructureSizes) == 24, "MTLAccelerationStructureSizes size");
_Static_assert(_Alignof(MTLAccelerationStructureSizes) == 8, "MTLAccelerationStructureSizes alignment");
_Static_assert(offsetof(MTLAccelerationStructureSizes, accelerationStructureSize) == 0, "MTLAccelerationStructureSizes.acceleration_structure_size offset");
_Static_assert(offsetof(MTLAccelerationStructureSizes, buildScratchBufferSize) == 8, "MTLAccelerationStructureSizes.build_scratch_buffer_size offset");
_Static_assert(offsetof(MTLAccelerationStructureSizes, refitScratchBufferSize) == 16, "MTLAccelerationStructureSizes.refit_scratch_buffer_size offset");

// NSRange: 16 bytes, alignment 8.
_Static_assert(sizeof(NSRange) == 16, "NSRange size");
_Static_assert(_Alignof(NSRange) == 8, "NSRange alignment");
_Static_assert(offsetof(NSRange, location) == 0, "NSRange.location offset");
_Static_assert(offsetof(NSRange, length) == 8, "NSRange.length offset");

// MTLClearColor: 32 bytes, alignment 8.
_Static_assert(sizeof(MTLClearColor) == 32, "MTLClearColor size");
_Static_assert(_Alignof(MTLClearColor) == 8, "MTLClearColor alignment");
_Static_assert(offsetof(MTLClearColor, red) == 0, "MTLClearColor.red offset");
_Static_assert(offsetof(MTLClearColor, green) == 8, "MTLClearColor.green offset");
_Static_assert(offsetof(MTLClearColor, blue) == 16, "MTLClearColor.blue offset");
_Static_assert(offsetof(MTLClearColor, alpha) == 24, "MTLClearColor.alpha offset");

// CGSize: 16 bytes, alignment 8.
_Static_assert(sizeof(CGSize) == 16, "CGSize size");
_Static_assert(_Alignof(CGSize) == 8, "CGSize alignment");
_Static_assert(offsetof(CGSize, width) == 0, "CGSize.width offset");
_Static_assert(offsetof(CGSize, height) == 8, "CGSize.height offset");

#endif
