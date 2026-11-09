// SPDX-License-Identifier: MIT OR Apache-2.0
// Derived from metal-rs, Copyright 2017 GFX developers.

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <TargetConditionals.h>
#import <dispatch/dispatch.h>
#import <objc/message.h>
#include <stdint.h>
#include <stdbool.h>

static uint32_t feature_os(uint64_t feature_set) {
    if (feature_set < 10000) return 0;
    if (feature_set < 20000) return 1;
    if (feature_set >= 30000 && feature_set < 40000) return 2;
    __builtin_unreachable();
}

static uint32_t feature_os_version(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_set == 0) || (feature_set == 1)) ? (8) : ((((feature_set == 2) || (feature_set == 3) || (feature_set == 4)) ? (9) : ((((feature_set == 5) || (feature_set == 6) || (feature_set == 7)) ? (10) : ((((feature_set == 8) || (feature_set == 9) || (feature_set == 10) || (feature_set == 11)) ? (11) : ((((feature_set == 12) || (feature_set == 13) || (feature_set == 14) || (feature_set == 15) || (feature_set == 16)) ? (12) : ((((feature_set == 30000)) ? (9) : ((((feature_set == 30001)) ? (10) : ((((feature_set == 30002) || (feature_set == 30003)) ? (11) : ((((feature_set == 30004) || (feature_set == 30005)) ? (12) : ((((feature_set == 10000)) ? (11) : ((((feature_set == 10001) || (feature_set == 10002)) ? (12) : ((((feature_set == 10003)) ? (13) : ((((feature_set == 10004) || (feature_set == 10005)) ? (14) : ((__builtin_unreachable(), 0)))))))))))))))))))))))))));
}

static uint32_t feature_gpu_family(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_set == 0) || (feature_set == 2) || (feature_set == 5) || (feature_set == 8) || (feature_set == 12) || (feature_set == 30000) || (feature_set == 30001) || (feature_set == 30002) || (feature_set == 30004) || (feature_set == 10000) || (feature_set == 10001) || (feature_set == 10002) || (feature_set == 10003) || (feature_set == 10004)) ? (1) : ((((feature_set == 1) || (feature_set == 3) || (feature_set == 6) || (feature_set == 9) || (feature_set == 13) || (feature_set == 30003) || (feature_set == 30005) || (feature_set == 10005)) ? (2) : ((((feature_set == 4) || (feature_set == 7) || (feature_set == 10) || (feature_set == 14)) ? (3) : ((((feature_set == 11) || (feature_set == 15)) ? (4) : ((((feature_set == 16)) ? (5) : ((__builtin_unreachable(), 0)))))))))));
}

static uint32_t feature_version(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_set == 0) || (feature_set == 1) || (feature_set == 4) || (feature_set == 11) || (feature_set == 16) || (feature_set == 10000) || (feature_set == 10005) || (feature_set == 10002) || (feature_set == 30000) || (feature_set == 30003)) ? (1) : ((((feature_set == 2) || (feature_set == 3) || (feature_set == 7) || (feature_set == 15) || (feature_set == 10001) || (feature_set == 30001) || (feature_set == 30005)) ? (2) : ((((feature_set == 5) || (feature_set == 6) || (feature_set == 10) || (feature_set == 10003) || (feature_set == 30002)) ? (3) : ((((feature_set == 8) || (feature_set == 9) || (feature_set == 14) || (feature_set == 30004) || (feature_set == 10004)) ? (4) : ((((feature_set == 12) || (feature_set == 13)) ? (5) : ((__builtin_unreachable(), 0)))))))))));
}

static bool feature_supports_metal_kit(uint64_t feature_set) {
    (void)feature_set;
    return true;
}

uint8_t metal_FeatureSetCapabilities_supports_metal_kit(uint64_t feature_set) {
    return feature_supports_metal_kit(feature_set);
}

static bool feature_supports_metal_performance_shaders(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 2) : ((((feature_os(feature_set) == 2)) ? (true) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 13) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_metal_performance_shaders(uint64_t feature_set) {
    return feature_supports_metal_performance_shaders(feature_set);
}

static bool feature_supports_programmable_blending(uint64_t feature_set) {
    (void)feature_set;
    return feature_os(feature_set) != 1;
}

uint8_t metal_FeatureSetCapabilities_supports_programmable_blending(uint64_t feature_set) {
    return feature_supports_programmable_blending(feature_set);
}

static bool feature_supports_pvrtc_pixel_formats(uint64_t feature_set) {
    (void)feature_set;
    return feature_os(feature_set) != 1;
}

uint8_t metal_FeatureSetCapabilities_supports_pvrtc_pixel_formats(uint64_t feature_set) {
    return feature_supports_pvrtc_pixel_formats(feature_set);
}

static bool feature_supports_eac_etc_pixel_formats(uint64_t feature_set) {
    (void)feature_set;
    return feature_os(feature_set) != 1;
}

uint8_t metal_FeatureSetCapabilities_supports_eac_etc_pixel_formats(uint64_t feature_set) {
    return feature_supports_eac_etc_pixel_formats(feature_set);
}

static bool feature_supports_astc_pixel_formats(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 2) : ((((feature_os(feature_set) == 2)) ? (true) : ((((feature_os(feature_set) == 1)) ? (false) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_astc_pixel_formats(uint64_t feature_set) {
    return feature_supports_astc_pixel_formats(feature_set);
}

static bool feature_supports_linear_textures(uint64_t feature_set) {
    (void)feature_set;
    return feature_os(feature_set) != 1 || feature_os_version(feature_set) >= 13;
}

uint8_t metal_FeatureSetCapabilities_supports_linear_textures(uint64_t feature_set) {
    return feature_supports_linear_textures(feature_set);
}

static bool feature_supports_bc_pixel_formats(uint64_t feature_set) {
    (void)feature_set;
    return feature_os(feature_set) == 1;
}

uint8_t metal_FeatureSetCapabilities_supports_bc_pixel_formats(uint64_t feature_set) {
    return feature_supports_bc_pixel_formats(feature_set);
}

static bool feature_supports_msaa_depth_resolve(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 3) : ((((feature_os(feature_set) == 2)) ? (feature_gpu_family(feature_set) >= 2) : ((((feature_os(feature_set) == 1)) ? (false) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_msaa_depth_resolve(uint64_t feature_set) {
    return feature_supports_msaa_depth_resolve(feature_set);
}

static bool feature_supports_counting_occlusion_query(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 3) : ((((feature_os(feature_set) == 2)) ? (feature_gpu_family(feature_set) >= 2) : ((((feature_os(feature_set) == 1)) ? (true) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_counting_occlusion_query(uint64_t feature_set) {
    return feature_supports_counting_occlusion_query(feature_set);
}

static bool feature_supports_base_vertex_instance_drawing(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 3) : ((((feature_os(feature_set) == 2)) ? (feature_gpu_family(feature_set) >= 2) : ((((feature_os(feature_set) == 1)) ? (true) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_base_vertex_instance_drawing(uint64_t feature_set) {
    return feature_supports_base_vertex_instance_drawing(feature_set);
}

static bool feature_supports_indirect_buffers(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 3) : ((((feature_os(feature_set) == 2)) ? (feature_gpu_family(feature_set) >= 2) : ((((feature_os(feature_set) == 1)) ? (true) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_indirect_buffers(uint64_t feature_set) {
    return feature_supports_indirect_buffers(feature_set);
}

static bool feature_supports_cube_map_texture_arrays(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 4) : ((((feature_os(feature_set) == 2)) ? (false) : ((((feature_os(feature_set) == 1)) ? (true) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_cube_map_texture_arrays(uint64_t feature_set) {
    return feature_supports_cube_map_texture_arrays(feature_set);
}

static bool feature_supports_texture_barriers(uint64_t feature_set) {
    (void)feature_set;
    return feature_os(feature_set) == 1;
}

uint8_t metal_FeatureSetCapabilities_supports_texture_barriers(uint64_t feature_set) {
    return feature_supports_texture_barriers(feature_set);
}

static bool feature_supports_layered_rendering(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 5) : ((((feature_os(feature_set) == 2)) ? (false) : ((((feature_os(feature_set) == 1)) ? (true) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_layered_rendering(uint64_t feature_set) {
    return feature_supports_layered_rendering(feature_set);
}

static bool feature_supports_tessellation(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 3 && feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 2)) ? (feature_gpu_family(feature_set) >= 2) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 12) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_tessellation(uint64_t feature_set) {
    return feature_supports_tessellation(feature_set);
}

static bool feature_supports_resource_heaps(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 2)) ? (feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 13) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_resource_heaps(uint64_t feature_set) {
    return feature_supports_resource_heaps(feature_set);
}

static bool feature_supports_memoryless_render_targets(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 2)) ? (feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 1)) ? (false) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_memoryless_render_targets(uint64_t feature_set) {
    return feature_supports_memoryless_render_targets(feature_set);
}

static bool feature_supports_function_specialization(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 2)) ? (feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 12) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_function_specialization(uint64_t feature_set) {
    return feature_supports_function_specialization(feature_set);
}

static bool feature_supports_function_buffer_read_writes(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 3 && feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 2)) ? (feature_gpu_family(feature_set) >= 2) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 12) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_function_buffer_read_writes(uint64_t feature_set) {
    return feature_supports_function_buffer_read_writes(feature_set);
}

static bool feature_supports_function_texture_read_writes(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 4) : ((((feature_os(feature_set) == 2)) ? (false) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 12) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_function_texture_read_writes(uint64_t feature_set) {
    return feature_supports_function_texture_read_writes(feature_set);
}

static bool feature_supports_array_of_textures(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 3 && feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 2)) ? (feature_gpu_family(feature_set) >= 2) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 13) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_array_of_textures(uint64_t feature_set) {
    return feature_supports_array_of_textures(feature_set);
}

static bool feature_supports_array_of_samplers(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 3 && feature_os_version(feature_set) >= 11) : ((((feature_os(feature_set) == 2)) ? (feature_gpu_family(feature_set) >= 2) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 12) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_array_of_samplers(uint64_t feature_set) {
    return feature_supports_array_of_samplers(feature_set);
}

static bool feature_supports_stencil_texture_views(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 2)) ? (feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 12) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_stencil_texture_views(uint64_t feature_set) {
    return feature_supports_stencil_texture_views(feature_set);
}

static bool feature_supports_depth_16_pixel_format(uint64_t feature_set) {
    (void)feature_set;
    return feature_os(feature_set) == 1 && feature_os_version(feature_set) >= 12;
}

uint8_t metal_FeatureSetCapabilities_supports_depth_16_pixel_format(uint64_t feature_set) {
    return feature_supports_depth_16_pixel_format(feature_set);
}

static bool feature_supports_extended_range_pixel_formats(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 3 && feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 2)) ? (feature_gpu_family(feature_set) >= 2) : ((((feature_os(feature_set) == 1)) ? (false) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_extended_range_pixel_formats(uint64_t feature_set) {
    return feature_supports_extended_range_pixel_formats(feature_set);
}

static bool feature_supports_wide_color_pixel_format(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_os_version(feature_set) >= 11) : ((((feature_os(feature_set) == 2)) ? (feature_os_version(feature_set) >= 11) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 13) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_wide_color_pixel_format(uint64_t feature_set) {
    return feature_supports_wide_color_pixel_format(feature_set);
}

static bool feature_supports_combined_msaa_store_and_resolve_action(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 3 && feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 2)) ? (feature_gpu_family(feature_set) >= 2) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 12) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_combined_msaa_store_and_resolve_action(uint64_t feature_set) {
    return feature_supports_combined_msaa_store_and_resolve_action(feature_set);
}

static bool feature_supports_deferred_store_action(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 2)) ? (feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 12) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_deferred_store_action(uint64_t feature_set) {
    return feature_supports_deferred_store_action(feature_set);
}

static bool feature_supports_msaa_blits(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 2)) ? (feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 1)) ? (true) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_msaa_blits(uint64_t feature_set) {
    return feature_supports_msaa_blits(feature_set);
}

static bool feature_supports_srgb_writes(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 3 || (feature_gpu_family(feature_set) >= 2 && feature_version(feature_set) >= 3)) : ((((feature_os(feature_set) == 2)) ? (feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 1)) ? (feature_gpu_family(feature_set) >= 2) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_srgb_writes(uint64_t feature_set) {
    return feature_supports_srgb_writes(feature_set);
}

static bool feature_supports_16_bit_unsigned_integer_coordinates(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 2)) ? (feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 12) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_16_bit_unsigned_integer_coordinates(uint64_t feature_set) {
    return feature_supports_16_bit_unsigned_integer_coordinates(feature_set);
}

static bool feature_supports_extract_insert_and_reverse_bits(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 2)) ? (feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 12) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_extract_insert_and_reverse_bits(uint64_t feature_set) {
    return feature_supports_extract_insert_and_reverse_bits(feature_set);
}

static bool feature_supports_simd_barrier(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 2)) ? (feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 13) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_simd_barrier(uint64_t feature_set) {
    return feature_supports_simd_barrier(feature_set);
}

static bool feature_supports_sampler_max_anisotropy(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 2)) ? (feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 13) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_sampler_max_anisotropy(uint64_t feature_set) {
    return feature_supports_sampler_max_anisotropy(feature_set);
}

static bool feature_supports_sampler_lod_clamp(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 2)) ? (feature_os_version(feature_set) >= 10) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 13) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_sampler_lod_clamp(uint64_t feature_set) {
    return feature_supports_sampler_lod_clamp(feature_set);
}

static bool feature_supports_border_color(uint64_t feature_set) {
    (void)feature_set;
    return feature_os(feature_set) == 1 && feature_os_version(feature_set) >= 12;
}

uint8_t metal_FeatureSetCapabilities_supports_border_color(uint64_t feature_set) {
    return feature_supports_border_color(feature_set);
}

static bool feature_supports_dual_source_blending(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_os_version(feature_set) >= 11) : ((((feature_os(feature_set) == 2)) ? (feature_os_version(feature_set) >= 11) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 12) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_dual_source_blending(uint64_t feature_set) {
    return feature_supports_dual_source_blending(feature_set);
}

static bool feature_supports_argument_buffers(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_os_version(feature_set) >= 11) : ((((feature_os(feature_set) == 2)) ? (feature_os_version(feature_set) >= 11) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 13) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_argument_buffers(uint64_t feature_set) {
    return feature_supports_argument_buffers(feature_set);
}

static bool feature_supports_programmable_sample_positions(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_os_version(feature_set) >= 11) : ((((feature_os(feature_set) == 2)) ? (feature_os_version(feature_set) >= 11) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 13) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_programmable_sample_positions(uint64_t feature_set) {
    return feature_supports_programmable_sample_positions(feature_set);
}

static bool feature_supports_uniform_type(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_os_version(feature_set) >= 11) : ((((feature_os(feature_set) == 2)) ? (feature_os_version(feature_set) >= 11) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 13) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_uniform_type(uint64_t feature_set) {
    return feature_supports_uniform_type(feature_set);
}

static bool feature_supports_imageblocks(uint64_t feature_set) {
    (void)feature_set;
    return feature_os(feature_set) == 0 && feature_gpu_family(feature_set) >= 4;
}

uint8_t metal_FeatureSetCapabilities_supports_imageblocks(uint64_t feature_set) {
    return feature_supports_imageblocks(feature_set);
}

static bool feature_supports_tile_shaders(uint64_t feature_set) {
    (void)feature_set;
    return feature_os(feature_set) == 0 && feature_gpu_family(feature_set) >= 4;
}

uint8_t metal_FeatureSetCapabilities_supports_tile_shaders(uint64_t feature_set) {
    return feature_supports_tile_shaders(feature_set);
}

static bool feature_supports_imageblock_sample_coverage_control(uint64_t feature_set) {
    (void)feature_set;
    return feature_os(feature_set) == 0 && feature_gpu_family(feature_set) >= 4;
}

uint8_t metal_FeatureSetCapabilities_supports_imageblock_sample_coverage_control(uint64_t feature_set) {
    return feature_supports_imageblock_sample_coverage_control(feature_set);
}

static bool feature_supports_threadgroup_sharing(uint64_t feature_set) {
    (void)feature_set;
    return feature_os(feature_set) == 0 && feature_gpu_family(feature_set) >= 4;
}

uint8_t metal_FeatureSetCapabilities_supports_threadgroup_sharing(uint64_t feature_set) {
    return feature_supports_threadgroup_sharing(feature_set);
}

static bool feature_supports_post_depth_coverage(uint64_t feature_set) {
    (void)feature_set;
    return feature_os(feature_set) == 0 && feature_gpu_family(feature_set) >= 4;
}

uint8_t metal_FeatureSetCapabilities_supports_post_depth_coverage(uint64_t feature_set) {
    return feature_supports_post_depth_coverage(feature_set);
}

static bool feature_supports_quad_scoped_permute_operations(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 4) : ((((feature_os(feature_set) == 2)) ? (false) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 13) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_quad_scoped_permute_operations(uint64_t feature_set) {
    return feature_supports_quad_scoped_permute_operations(feature_set);
}

static bool feature_supports_raster_order_groups(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 4) : ((((feature_os(feature_set) == 2)) ? (false) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 13) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_raster_order_groups(uint64_t feature_set) {
    return feature_supports_raster_order_groups(feature_set);
}

static bool feature_supports_non_uniform_threadgroup_size(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 4) : ((((feature_os(feature_set) == 2)) ? (false) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 13) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_non_uniform_threadgroup_size(uint64_t feature_set) {
    return feature_supports_non_uniform_threadgroup_size(feature_set);
}

static bool feature_supports_multiple_viewports(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 5) : ((((feature_os(feature_set) == 2)) ? (false) : ((((feature_os(feature_set) == 1)) ? (feature_os_version(feature_set) >= 13) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_multiple_viewports(uint64_t feature_set) {
    return feature_supports_multiple_viewports(feature_set);
}

static bool feature_supports_device_notifications(uint64_t feature_set) {
    (void)feature_set;
    return feature_os(feature_set) == 1 && feature_os_version(feature_set) >= 13;
}

uint8_t metal_FeatureSetCapabilities_supports_device_notifications(uint64_t feature_set) {
    return feature_supports_device_notifications(feature_set);
}

static bool feature_supports_stencil_feedback(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 5) : ((((feature_os(feature_set) == 2)) ? (false) : ((((feature_os(feature_set) == 1)) ? (feature_gpu_family(feature_set) >= 2) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_stencil_feedback(uint64_t feature_set) {
    return feature_supports_stencil_feedback(feature_set);
}

static bool feature_supports_stencil_resolve(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 5) : ((((feature_os(feature_set) == 2)) ? (false) : ((((feature_os(feature_set) == 1)) ? (feature_gpu_family(feature_set) >= 2) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_stencil_resolve(uint64_t feature_set) {
    return feature_supports_stencil_resolve(feature_set);
}

static bool feature_supports_binary_archive(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 3) : ((((feature_os(feature_set) == 2)) ? (feature_gpu_family(feature_set) >= 3) : ((((feature_os(feature_set) == 1)) ? (feature_gpu_family(feature_set) >= 1) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_supports_binary_archive(uint64_t feature_set) {
    return feature_supports_binary_archive(feature_set);
}

static uint32_t feature_max_vertex_attributes(uint64_t feature_set) {
    (void)feature_set;
    return 31;
}

uint32_t metal_FeatureSetCapabilities_max_vertex_attributes(uint64_t feature_set) {
    return feature_max_vertex_attributes(feature_set);
}

static uint32_t feature_max_buffer_argument_entries(uint64_t feature_set) {
    (void)feature_set;
    return 31;
}

uint32_t metal_FeatureSetCapabilities_max_buffer_argument_entries(uint64_t feature_set) {
    return feature_max_buffer_argument_entries(feature_set);
}

static uint32_t feature_max_texture_argument_entries(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1) ? (128) : (31));
}

uint32_t metal_FeatureSetCapabilities_max_texture_argument_entries(uint64_t feature_set) {
    return feature_max_texture_argument_entries(feature_set);
}

static uint32_t feature_max_sampler_state_argument_entries(uint64_t feature_set) {
    (void)feature_set;
    return 16;
}

uint32_t metal_FeatureSetCapabilities_max_sampler_state_argument_entries(uint64_t feature_set) {
    return feature_max_sampler_state_argument_entries(feature_set);
}

static uint32_t feature_max_threadgroup_memory_argument_entries(uint64_t feature_set) {
    (void)feature_set;
    return 31;
}

uint32_t metal_FeatureSetCapabilities_max_threadgroup_memory_argument_entries(uint64_t feature_set) {
    return feature_max_threadgroup_memory_argument_entries(feature_set);
}

static uint32_t feature_max_inlined_constant_data_buffers(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1) ? (14) : (31));
}

uint32_t metal_FeatureSetCapabilities_max_inlined_constant_data_buffers(uint64_t feature_set) {
    return feature_max_inlined_constant_data_buffers(feature_set);
}

static uint32_t feature_max_inline_constant_buffer_length(uint64_t feature_set) {
    (void)feature_set;
    return 4 * 1024u;
}

uint32_t metal_FeatureSetCapabilities_max_inline_constant_buffer_length(uint64_t feature_set) {
    return feature_max_inline_constant_buffer_length(feature_set);
}

static uint32_t feature_max_threads_per_threadgroup(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1 || feature_gpu_family(feature_set) >= 4) ? (1024) : (512));
}

uint32_t metal_FeatureSetCapabilities_max_threads_per_threadgroup(uint64_t feature_set) {
    return feature_max_threads_per_threadgroup(feature_set);
}

static uint32_t feature_max_total_threadgroup_memory_allocation(uint64_t feature_set) {
    (void)feature_set;
    return (((((feature_os(feature_set) == 0)) && ((feature_gpu_family(feature_set) == 5)))) ? (64 * 1024u) : ((((((feature_os(feature_set) == 0)) && ((feature_gpu_family(feature_set) == 4)))) ? (((feature_os_version(feature_set) >= 12) ? (64 * 1024u) : (32 * 1024u))) : ((((((feature_os(feature_set) == 0)) && ((feature_gpu_family(feature_set) == 3)))) ? (16 * 1024u) : ((((((feature_os(feature_set) == 0)) && (1))) ? (16 * 1024u - 32) : ((((((feature_os(feature_set) == 2)) && ((feature_gpu_family(feature_set) == 1)))) ? (16 * 1024u - 32) : ((((((feature_os(feature_set) == 2)) && (1))) ? (16 * 1024u) : ((((((feature_os(feature_set) == 1)) && (1))) ? (32 * 1024u) : ((__builtin_unreachable(), 0)))))))))))))));
}

uint32_t metal_FeatureSetCapabilities_max_total_threadgroup_memory_allocation(uint64_t feature_set) {
    return feature_max_total_threadgroup_memory_allocation(feature_set);
}

static uint32_t feature_max_total_tile_memory_allocation(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 0 && feature_gpu_family(feature_set) == 4) ? (32 * 1024u) : (0));
}

uint32_t metal_FeatureSetCapabilities_max_total_tile_memory_allocation(uint64_t feature_set) {
    return feature_max_total_tile_memory_allocation(feature_set);
}

static uint32_t feature_threadgroup_memory_length_alignment(uint64_t feature_set) {
    (void)feature_set;
    return 16;
}

uint32_t metal_FeatureSetCapabilities_threadgroup_memory_length_alignment(uint64_t feature_set) {
    return feature_threadgroup_memory_length_alignment(feature_set);
}

static uint64_t feature_max_constant_buffer_function_memory_allocation(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1) ? ((64 * 1024u)) : (UINT64_MAX));
}

uint8_t metal_FeatureSetCapabilities_max_constant_buffer_function_memory_allocation(uint64_t feature_set, uint32_t * out) {
    uint64_t value = feature_max_constant_buffer_function_memory_allocation(feature_set);
    if (value == UINT64_MAX) return 0;
    *out = (uint32_t)value;
    return 1;
}

static uint32_t feature_max_fragment_inputs(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1) ? (32) : (60));
}

uint32_t metal_FeatureSetCapabilities_max_fragment_inputs(uint64_t feature_set) {
    return feature_max_fragment_inputs(feature_set);
}

static uint32_t feature_max_fragment_input_components(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1) ? (128) : (60));
}

uint32_t metal_FeatureSetCapabilities_max_fragment_input_components(uint64_t feature_set) {
    return feature_max_fragment_input_components(feature_set);
}

static uint32_t feature_max_function_constants(uint64_t feature_set) {
    (void)feature_set;
    return ((((feature_os(feature_set) == 0)) && (feature_os_version(feature_set) >= 11)) ? (65536) : (((((feature_os(feature_set) == 2)) && (feature_os_version(feature_set) >= 10)) ? (65536) : (((((feature_os(feature_set) == 1)) && (feature_os_version(feature_set) >= 12)) ? (65536) : (((1) ? (0) : ((__builtin_unreachable(), 0)))))))));
}

uint32_t metal_FeatureSetCapabilities_max_function_constants(uint64_t feature_set) {
    return feature_max_function_constants(feature_set);
}

static uint32_t feature_max_tessellation_factor(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_supports_tessellation(feature_set)) ? (((((feature_os(feature_set) == 0)) && (feature_gpu_family(feature_set) >= 5)) ? (64) : ((((feature_os(feature_set) == 0)) ? (16) : ((((feature_os(feature_set) == 2)) ? (16) : ((((feature_os(feature_set) == 1)) ? (64) : ((__builtin_unreachable(), 0)))))))))) : (0));
}

uint32_t metal_FeatureSetCapabilities_max_tessellation_factor(uint64_t feature_set) {
    return feature_max_tessellation_factor(feature_set);
}

static uint32_t feature_max_viewports_and_scissor_rectangles(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_supports_multiple_viewports(feature_set)) ? (16) : (1));
}

uint32_t metal_FeatureSetCapabilities_max_viewports_and_scissor_rectangles(uint64_t feature_set) {
    return feature_max_viewports_and_scissor_rectangles(feature_set);
}

static uint32_t feature_max_raster_order_groups(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_supports_raster_order_groups(feature_set)) ? (8) : (0));
}

uint32_t metal_FeatureSetCapabilities_max_raster_order_groups(uint64_t feature_set) {
    return feature_max_raster_order_groups(feature_set);
}

static uint32_t feature_max_buffer_length(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1 && feature_os_version(feature_set) >= 12) ? (1 * 1073741824u) : (256 * 1048576u));
}

uint32_t metal_FeatureSetCapabilities_max_buffer_length(uint64_t feature_set) {
    return feature_max_buffer_length(feature_set);
}

static uint32_t feature_min_buffer_offset_alignment(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1) ? (256) : (4));
}

uint32_t metal_FeatureSetCapabilities_min_buffer_offset_alignment(uint64_t feature_set) {
    return feature_min_buffer_offset_alignment(feature_set);
}

static uint32_t feature_max_1d_texture_size(uint64_t feature_set) {
    (void)feature_set;
    return (((((feature_os(feature_set) == 0)) && ((feature_gpu_family(feature_set) == 1))) || (((feature_os(feature_set) == 0)) && ((feature_gpu_family(feature_set) == 2)))) ? (((feature_version(feature_set) <= 2) ? (4096) : (8192))) : ((((((feature_os(feature_set) == 2)) && ((feature_gpu_family(feature_set) == 1)))) ? (8192) : (((1) ? (16384) : ((__builtin_unreachable(), 0)))))));
}

uint32_t metal_FeatureSetCapabilities_max_1d_texture_size(uint64_t feature_set) {
    return feature_max_1d_texture_size(feature_set);
}

static uint32_t feature_max_2d_texture_size(uint64_t feature_set) {
    (void)feature_set;
    return (((((feature_os(feature_set) == 0)) && ((feature_gpu_family(feature_set) == 1))) || (((feature_os(feature_set) == 0)) && ((feature_gpu_family(feature_set) == 2)))) ? (((feature_version(feature_set) <= 2) ? (4096) : (8192))) : ((((((feature_os(feature_set) == 2)) && ((feature_gpu_family(feature_set) == 1)))) ? (8192) : (((1) ? (16384) : ((__builtin_unreachable(), 0)))))));
}

uint32_t metal_FeatureSetCapabilities_max_2d_texture_size(uint64_t feature_set) {
    return feature_max_2d_texture_size(feature_set);
}

static uint32_t feature_max_cube_map_texture_size(uint64_t feature_set) {
    (void)feature_set;
    return (((((feature_os(feature_set) == 0)) && ((feature_gpu_family(feature_set) == 1))) || (((feature_os(feature_set) == 0)) && ((feature_gpu_family(feature_set) == 2)))) ? (((feature_version(feature_set) <= 2) ? (4096) : (8192))) : ((((((feature_os(feature_set) == 2)) && ((feature_gpu_family(feature_set) == 1)))) ? (8192) : (((1) ? (16384) : ((__builtin_unreachable(), 0)))))));
}

uint32_t metal_FeatureSetCapabilities_max_cube_map_texture_size(uint64_t feature_set) {
    return feature_max_cube_map_texture_size(feature_set);
}

static uint32_t feature_max_3d_texture_size(uint64_t feature_set) {
    (void)feature_set;
    return 2048;
}

uint32_t metal_FeatureSetCapabilities_max_3d_texture_size(uint64_t feature_set) {
    return feature_max_3d_texture_size(feature_set);
}

static uint32_t feature_max_array_layers(uint64_t feature_set) {
    (void)feature_set;
    return 2048;
}

uint32_t metal_FeatureSetCapabilities_max_array_layers(uint64_t feature_set) {
    return feature_max_array_layers(feature_set);
}

static uint32_t feature_copy_texture_buffer_alignment(uint64_t feature_set) {
    (void)feature_set;
    return (((((feature_os(feature_set) == 0)) && ((feature_gpu_family(feature_set) == 1))) || (((feature_os(feature_set) == 0)) && ((feature_gpu_family(feature_set) == 2))) || (((feature_os(feature_set) == 2)) && ((feature_gpu_family(feature_set) == 1)))) ? (64) : ((((((feature_os(feature_set) == 0)) && (1)) || (((feature_os(feature_set) == 2)) && (1))) ? (16) : ((((((feature_os(feature_set) == 1)) && (1))) ? (256) : ((__builtin_unreachable(), 0)))))));
}

uint32_t metal_FeatureSetCapabilities_copy_texture_buffer_alignment(uint64_t feature_set) {
    return feature_copy_texture_buffer_alignment(feature_set);
}

static uint64_t feature_new_texture_buffer_alignment(uint64_t feature_set) {
    (void)feature_set;
    return (((feature_os(feature_set) == 0)) ? (((feature_os_version(feature_set) >= 11) ? (UINT64_MAX) : (((feature_gpu_family(feature_set) == 3) ? ((16)) : ((64)))))) : ((((feature_os(feature_set) == 2)) ? (((feature_os_version(feature_set) >= 11) ? (UINT64_MAX) : ((64)))) : ((((feature_os(feature_set) == 1)) ? (UINT64_MAX) : ((__builtin_unreachable(), 0)))))));
}

uint8_t metal_FeatureSetCapabilities_new_texture_buffer_alignment(uint64_t feature_set, uint32_t * out) {
    uint64_t value = feature_new_texture_buffer_alignment(feature_set);
    if (value == UINT64_MAX) return 0;
    *out = (uint32_t)value;
    return 1;
}

static uint32_t feature_max_color_render_targets(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 0 && feature_gpu_family(feature_set) == 1) ? (4) : (8));
}

uint32_t metal_FeatureSetCapabilities_max_color_render_targets(uint64_t feature_set) {
    return feature_max_color_render_targets(feature_set);
}

static uint32_t feature_max_point_primitive_size(uint64_t feature_set) {
    (void)feature_set;
    return 511;
}

uint32_t metal_FeatureSetCapabilities_max_point_primitive_size(uint64_t feature_set) {
    return feature_max_point_primitive_size(feature_set);
}

static uint64_t feature_max_total_color_render_target_size(uint64_t feature_set) {
    (void)feature_set;
    return (((((feature_os(feature_set) == 0)) && ((feature_gpu_family(feature_set) == 1)))) ? ((128)) : ((((((feature_os(feature_set) == 0)) && ((feature_gpu_family(feature_set) == 2))) || (((feature_os(feature_set) == 0)) && ((feature_gpu_family(feature_set) == 3)))) ? ((256)) : ((((((feature_os(feature_set) == 0)) && (1))) ? ((512)) : ((((((feature_os(feature_set) == 2)) && (1))) ? ((256)) : ((((((feature_os(feature_set) == 1)) && (1))) ? (UINT64_MAX) : ((__builtin_unreachable(), 0)))))))))));
}

uint8_t metal_FeatureSetCapabilities_max_total_color_render_target_size(uint64_t feature_set, uint32_t * out) {
    uint64_t value = feature_max_total_color_render_target_size(feature_set);
    if (value == UINT64_MAX) return 0;
    *out = (uint32_t)value;
    return 1;
}

static uint32_t feature_max_visibility_query_offset(uint64_t feature_set) {
    (void)feature_set;
    return 64 * 1024u - 8;
}

uint32_t metal_FeatureSetCapabilities_max_visibility_query_offset(uint64_t feature_set) {
    return feature_max_visibility_query_offset(feature_set);
}

static uint32_t feature_a8_unorm_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 1u;
}

uint32_t metal_FeatureSetCapabilities_a8_unorm_capabilities(uint64_t feature_set) {
    return feature_a8_unorm_capabilities(feature_set);
}

static uint32_t feature_r8_unorm_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 63u;
}

uint32_t metal_FeatureSetCapabilities_r8_unorm_capabilities(uint64_t feature_set) {
    return feature_r8_unorm_capabilities(feature_set);
}

static uint32_t feature_r8_unorm_srgb_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1) ? (0u) : (((feature_supports_srgb_writes(feature_set)) ? (63u) : (61u))));
}

uint32_t metal_FeatureSetCapabilities_r8_unorm_srgb_capabilities(uint64_t feature_set) {
    return feature_r8_unorm_srgb_capabilities(feature_set);
}

static uint32_t feature_r8_snorm_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 0 && feature_gpu_family(feature_set) == 1) ? (31u) : (63u));
}

uint32_t metal_FeatureSetCapabilities_r8_snorm_capabilities(uint64_t feature_set) {
    return feature_r8_snorm_capabilities(feature_set);
}

static uint32_t feature_r8_uint_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 2u | 4u | 16u;
}

uint32_t metal_FeatureSetCapabilities_r8_uint_capabilities(uint64_t feature_set) {
    return feature_r8_uint_capabilities(feature_set);
}

static uint32_t feature_r8_sint_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 2u | 4u | 16u;
}

uint32_t metal_FeatureSetCapabilities_r8_sint_capabilities(uint64_t feature_set) {
    return feature_r8_sint_capabilities(feature_set);
}

static uint32_t feature_r16_unorm_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) != 1) ? (31u) : (63u));
}

uint32_t metal_FeatureSetCapabilities_r16_unorm_capabilities(uint64_t feature_set) {
    return feature_r16_unorm_capabilities(feature_set);
}

static uint32_t feature_r16_snorm_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) != 1) ? (31u) : (63u));
}

uint32_t metal_FeatureSetCapabilities_r16_snorm_capabilities(uint64_t feature_set) {
    return feature_r16_snorm_capabilities(feature_set);
}

static uint32_t feature_r16_uint_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 2u | 4u | 16u;
}

uint32_t metal_FeatureSetCapabilities_r16_uint_capabilities(uint64_t feature_set) {
    return feature_r16_uint_capabilities(feature_set);
}

static uint32_t feature_r16_sint_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 2u | 4u | 16u;
}

uint32_t metal_FeatureSetCapabilities_r16_sint_capabilities(uint64_t feature_set) {
    return feature_r16_sint_capabilities(feature_set);
}

static uint32_t feature_r16_float_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 63u;
}

uint32_t metal_FeatureSetCapabilities_r16_float_capabilities(uint64_t feature_set) {
    return feature_r16_float_capabilities(feature_set);
}

static uint32_t feature_rg8_unorm_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 63u;
}

uint32_t metal_FeatureSetCapabilities_rg8_unorm_capabilities(uint64_t feature_set) {
    return feature_rg8_unorm_capabilities(feature_set);
}

static uint32_t feature_rg8_unorm_srgb_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1) ? (0u) : (((feature_supports_srgb_writes(feature_set)) ? (63u) : (61u))));
}

uint32_t metal_FeatureSetCapabilities_rg8_unorm_srgb_capabilities(uint64_t feature_set) {
    return feature_rg8_unorm_srgb_capabilities(feature_set);
}

static uint32_t feature_rg8_snorm_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 0 && feature_gpu_family(feature_set) == 1) ? (31u) : (63u));
}

uint32_t metal_FeatureSetCapabilities_rg8_snorm_capabilities(uint64_t feature_set) {
    return feature_rg8_snorm_capabilities(feature_set);
}

static uint32_t feature_rg8_uint_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 2u | 4u | 16u;
}

uint32_t metal_FeatureSetCapabilities_rg8_uint_capabilities(uint64_t feature_set) {
    return feature_rg8_uint_capabilities(feature_set);
}

static uint32_t feature_rg8_sint_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 2u | 4u | 16u;
}

uint32_t metal_FeatureSetCapabilities_rg8_sint_capabilities(uint64_t feature_set) {
    return feature_rg8_sint_capabilities(feature_set);
}

static uint32_t feature_b5_g6_r5_unorm_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1) ? (0u) : (61u));
}

uint32_t metal_FeatureSetCapabilities_b5_g6_r5_unorm_capabilities(uint64_t feature_set) {
    return feature_b5_g6_r5_unorm_capabilities(feature_set);
}

static uint32_t feature_a1_bgr5_unorm_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1) ? (0u) : (61u));
}

uint32_t metal_FeatureSetCapabilities_a1_bgr5_unorm_capabilities(uint64_t feature_set) {
    return feature_a1_bgr5_unorm_capabilities(feature_set);
}

static uint32_t feature_abgr4_unorm_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1) ? (0u) : (61u));
}

uint32_t metal_FeatureSetCapabilities_abgr4_unorm_capabilities(uint64_t feature_set) {
    return feature_abgr4_unorm_capabilities(feature_set);
}

static uint32_t feature_bgr5_a1_unorm_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1) ? (0u) : (61u));
}

uint32_t metal_FeatureSetCapabilities_bgr5_a1_unorm_capabilities(uint64_t feature_set) {
    return feature_bgr5_a1_unorm_capabilities(feature_set);
}

static uint32_t feature_r32_uint_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 0 && feature_os_version(feature_set) == 8) ? (4u) : (((feature_os(feature_set) == 1) ? (4u | 2u | 16u) : (4u | 2u))));
}

uint32_t metal_FeatureSetCapabilities_r32_uint_capabilities(uint64_t feature_set) {
    return feature_r32_uint_capabilities(feature_set);
}

static uint32_t feature_r32_sint_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 0 && feature_os_version(feature_set) == 8) ? (4u) : (((feature_os(feature_set) == 1) ? (4u | 2u | 16u) : (4u | 2u))));
}

uint32_t metal_FeatureSetCapabilities_r32_sint_capabilities(uint64_t feature_set) {
    return feature_r32_sint_capabilities(feature_set);
}

static uint32_t feature_r32_float_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 0 && feature_os_version(feature_set) == 8) ? (4u | 8u | 16u) : (((feature_os(feature_set) == 1) ? (63u) : (2u | 4u | 8u | 16u))));
}

uint32_t metal_FeatureSetCapabilities_r32_float_capabilities(uint64_t feature_set) {
    return feature_r32_float_capabilities(feature_set);
}

static uint32_t feature_rg16_unorm_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1) ? (63u) : (31u));
}

uint32_t metal_FeatureSetCapabilities_rg16_unorm_capabilities(uint64_t feature_set) {
    return feature_rg16_unorm_capabilities(feature_set);
}

static uint32_t feature_rg16_snorm_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1) ? (63u) : (31u));
}

uint32_t metal_FeatureSetCapabilities_rg16_snorm_capabilities(uint64_t feature_set) {
    return feature_rg16_snorm_capabilities(feature_set);
}

static uint32_t feature_rg16_uint_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 2u | 4u | 16u;
}

uint32_t metal_FeatureSetCapabilities_rg16_uint_capabilities(uint64_t feature_set) {
    return feature_rg16_uint_capabilities(feature_set);
}

static uint32_t feature_rg16_sint_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 2u | 4u | 16u;
}

uint32_t metal_FeatureSetCapabilities_rg16_sint_capabilities(uint64_t feature_set) {
    return feature_rg16_sint_capabilities(feature_set);
}

static uint32_t feature_rg16_float_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 63u;
}

uint32_t metal_FeatureSetCapabilities_rg16_float_capabilities(uint64_t feature_set) {
    return feature_rg16_float_capabilities(feature_set);
}

static uint32_t feature_rgba8_unorm_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 63u;
}

uint32_t metal_FeatureSetCapabilities_rgba8_unorm_capabilities(uint64_t feature_set) {
    return feature_rgba8_unorm_capabilities(feature_set);
}

static uint32_t feature_rgba8_unorm_srgb_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_supports_srgb_writes(feature_set)) ? (63u) : (61u));
}

uint32_t metal_FeatureSetCapabilities_rgba8_unorm_srgb_capabilities(uint64_t feature_set) {
    return feature_rgba8_unorm_srgb_capabilities(feature_set);
}

static uint32_t feature_rgba8_snorm_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 0 && feature_gpu_family(feature_set) == 1) ? (31u) : (63u));
}

uint32_t metal_FeatureSetCapabilities_rgba8_snorm_capabilities(uint64_t feature_set) {
    return feature_rgba8_snorm_capabilities(feature_set);
}

static uint32_t feature_rgba8_uint_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 2u | 4u | 16u;
}

uint32_t metal_FeatureSetCapabilities_rgba8_uint_capabilities(uint64_t feature_set) {
    return feature_rgba8_uint_capabilities(feature_set);
}

static uint32_t feature_rgba8_sint_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 2u | 4u | 16u;
}

uint32_t metal_FeatureSetCapabilities_rgba8_sint_capabilities(uint64_t feature_set) {
    return feature_rgba8_sint_capabilities(feature_set);
}

static uint32_t feature_bgra8_unorm_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 63u;
}

uint32_t metal_FeatureSetCapabilities_bgra8_unorm_capabilities(uint64_t feature_set) {
    return feature_bgra8_unorm_capabilities(feature_set);
}

static uint32_t feature_bgra8_unorm_srgb_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_supports_srgb_writes(feature_set)) ? (63u) : (61u));
}

uint32_t metal_FeatureSetCapabilities_bgra8_unorm_srgb_capabilities(uint64_t feature_set) {
    return feature_bgra8_unorm_srgb_capabilities(feature_set);
}

static uint32_t feature_rgb10_a2_unorm_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((((((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 3) : ((((feature_os(feature_set) == 2)) ? (feature_gpu_family(feature_set) >= 2) : ((((feature_os(feature_set) == 1)) ? (true) : ((__builtin_unreachable(), 0))))))))) ? (63u) : (61u));
}

uint32_t metal_FeatureSetCapabilities_rgb10_a2_unorm_capabilities(uint64_t feature_set) {
    return feature_rgb10_a2_unorm_capabilities(feature_set);
}

static uint32_t feature_rgb10_a2_uint_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((((((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 3) : ((((feature_os(feature_set) == 2)) ? (feature_gpu_family(feature_set) >= 2) : ((((feature_os(feature_set) == 1)) ? (true) : ((__builtin_unreachable(), 0))))))))) ? (2u | 4u | 16u) : (4u | 16u));
}

uint32_t metal_FeatureSetCapabilities_rgb10_a2_uint_capabilities(uint64_t feature_set) {
    return feature_rgb10_a2_uint_capabilities(feature_set);
}

static uint32_t feature_rg11_b10_float_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((((((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 3) : ((((feature_os(feature_set) == 2)) ? (feature_gpu_family(feature_set) >= 2) : ((((feature_os(feature_set) == 1)) ? (true) : ((__builtin_unreachable(), 0))))))))) ? (63u) : (61u));
}

uint32_t metal_FeatureSetCapabilities_rg11_b10_float_capabilities(uint64_t feature_set) {
    return feature_rg11_b10_float_capabilities(feature_set);
}

static uint32_t feature_rgb9_e5_float_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1) ? (1u) : (((((((feature_os(feature_set) == 0)) ? (feature_gpu_family(feature_set) >= 3) : ((((feature_os(feature_set) == 2)) ? (feature_gpu_family(feature_set) >= 2) : ((((feature_os(feature_set) == 1)) ? (false) : ((__builtin_unreachable(), 0))))))))) ? (63u) : (61u))));
}

uint32_t metal_FeatureSetCapabilities_rgb9_e5_float_capabilities(uint64_t feature_set) {
    return feature_rgb9_e5_float_capabilities(feature_set);
}

static uint32_t feature_rg32_uint_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 0 && feature_os_version(feature_set) == 8) ? (4u) : (((feature_os(feature_set) == 1) ? (4u | 2u | 16u) : (4u | 2u))));
}

uint32_t metal_FeatureSetCapabilities_rg32_uint_capabilities(uint64_t feature_set) {
    return feature_rg32_uint_capabilities(feature_set);
}

static uint32_t feature_rg32_sint_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 0 && feature_os_version(feature_set) == 8) ? (4u) : (((feature_os(feature_set) == 1) ? (4u | 2u | 16u) : (4u | 2u))));
}

uint32_t metal_FeatureSetCapabilities_rg32_sint_capabilities(uint64_t feature_set) {
    return feature_rg32_sint_capabilities(feature_set);
}

static uint32_t feature_rg32_float_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1) ? (63u) : (((feature_os(feature_set) == 0 && feature_os_version(feature_set) == 8) ? (4u | 8u) : (2u | 4u | 8u))));
}

uint32_t metal_FeatureSetCapabilities_rg32_float_capabilities(uint64_t feature_set) {
    return feature_rg32_float_capabilities(feature_set);
}

static uint32_t feature_rgba16_unorm_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1) ? (63u) : (61u));
}

uint32_t metal_FeatureSetCapabilities_rgba16_unorm_capabilities(uint64_t feature_set) {
    return feature_rgba16_unorm_capabilities(feature_set);
}

static uint32_t feature_rgba16_snorm_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1) ? (63u) : (61u));
}

uint32_t metal_FeatureSetCapabilities_rgba16_snorm_capabilities(uint64_t feature_set) {
    return feature_rgba16_snorm_capabilities(feature_set);
}

static uint32_t feature_rgba16_uint_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 2u | 4u | 16u;
}

uint32_t metal_FeatureSetCapabilities_rgba16_uint_capabilities(uint64_t feature_set) {
    return feature_rgba16_uint_capabilities(feature_set);
}

static uint32_t feature_rgba16_sint_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 2u | 4u | 16u;
}

uint32_t metal_FeatureSetCapabilities_rgba16_sint_capabilities(uint64_t feature_set) {
    return feature_rgba16_sint_capabilities(feature_set);
}

static uint32_t feature_rgba16_float_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 63u;
}

uint32_t metal_FeatureSetCapabilities_rgba16_float_capabilities(uint64_t feature_set) {
    return feature_rgba16_float_capabilities(feature_set);
}

static uint32_t feature_rgba32_uint_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 0 && feature_os_version(feature_set) == 8) ? (4u) : (((feature_os(feature_set) == 1) ? (4u | 2u | 16u) : (4u | 2u))));
}

uint32_t metal_FeatureSetCapabilities_rgba32_uint_capabilities(uint64_t feature_set) {
    return feature_rgba32_uint_capabilities(feature_set);
}

static uint32_t feature_rgba32_sint_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 0 && feature_os_version(feature_set) == 8) ? (4u) : (((feature_os(feature_set) == 1) ? (4u | 2u | 16u) : (4u | 2u))));
}

uint32_t metal_FeatureSetCapabilities_rgba32_sint_capabilities(uint64_t feature_set) {
    return feature_rgba32_sint_capabilities(feature_set);
}

static uint32_t feature_rgba32_float_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1) ? (63u) : (((feature_os(feature_set) == 0 && feature_version(feature_set) == 8) ? (4u) : (2u | 4u))));
}

uint32_t metal_FeatureSetCapabilities_rgba32_float_capabilities(uint64_t feature_set) {
    return feature_rgba32_float_capabilities(feature_set);
}

static uint32_t feature_pvrtc_pixel_formats_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_supports_pvrtc_pixel_formats(feature_set)) ? (1u) : (0u));
}

uint32_t metal_FeatureSetCapabilities_pvrtc_pixel_formats_capabilities(uint64_t feature_set) {
    return feature_pvrtc_pixel_formats_capabilities(feature_set);
}

static uint32_t feature_eac_etc_pixel_formats_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_supports_eac_etc_pixel_formats(feature_set)) ? (1u) : (0u));
}

uint32_t metal_FeatureSetCapabilities_eac_etc_pixel_formats_capabilities(uint64_t feature_set) {
    return feature_eac_etc_pixel_formats_capabilities(feature_set);
}

static uint32_t feature_astc_pixel_formats_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_supports_astc_pixel_formats(feature_set)) ? (1u) : (0u));
}

uint32_t metal_FeatureSetCapabilities_astc_pixel_formats_capabilities(uint64_t feature_set) {
    return feature_astc_pixel_formats_capabilities(feature_set);
}

static uint32_t feature_bc_pixel_formats_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_supports_bc_pixel_formats(feature_set)) ? (1u) : (0u));
}

uint32_t metal_FeatureSetCapabilities_bc_pixel_formats_capabilities(uint64_t feature_set) {
    return feature_bc_pixel_formats_capabilities(feature_set);
}

static uint32_t feature_gbgr422_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 1u;
}

uint32_t metal_FeatureSetCapabilities_gbgr422_capabilities(uint64_t feature_set) {
    return feature_gbgr422_capabilities(feature_set);
}

static uint32_t feature_bgrg422_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 1u;
}

uint32_t metal_FeatureSetCapabilities_bgrg422_capabilities(uint64_t feature_set) {
    return feature_bgrg422_capabilities(feature_set);
}

static uint32_t feature_depth16_unorm_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_supports_depth_16_pixel_format(feature_set)) ? (1u | 16u | 32u) : (0u));
}

uint32_t metal_FeatureSetCapabilities_depth16_unorm_capabilities(uint64_t feature_set) {
    return feature_depth16_unorm_capabilities(feature_set);
}

static uint32_t feature_depth32_float_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1) ? (1u | 16u | 32u) : (((feature_supports_msaa_depth_resolve(feature_set)) ? (16u | 32u) : (16u))));
}

uint32_t metal_FeatureSetCapabilities_depth32_float_capabilities(uint64_t feature_set) {
    return feature_depth32_float_capabilities(feature_set);
}

static uint32_t feature_stencil8_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 16u;
}

uint32_t metal_FeatureSetCapabilities_stencil8_capabilities(uint64_t feature_set) {
    return feature_stencil8_capabilities(feature_set);
}

static uint32_t feature_depth24_unorm_stencil8_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1) ? (1u | 16u | 32u) : (0u));
}

uint32_t metal_FeatureSetCapabilities_depth24_unorm_stencil8_capabilities(uint64_t feature_set) {
    return feature_depth24_unorm_stencil8_capabilities(feature_set);
}

static uint32_t feature_depth32_float_stencil8_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1) ? (1u | 16u | 32u) : (((feature_supports_msaa_depth_resolve(feature_set)) ? (16u | 32u) : (16u))));
}

uint32_t metal_FeatureSetCapabilities_depth32_float_stencil8_capabilities(uint64_t feature_set) {
    return feature_depth32_float_stencil8_capabilities(feature_set);
}

static uint32_t feature_x24_stencil8_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_os(feature_set) == 1) ? (16u) : (0u));
}

uint32_t metal_FeatureSetCapabilities_x24_stencil8_capabilities(uint64_t feature_set) {
    return feature_x24_stencil8_capabilities(feature_set);
}

static uint32_t feature_x32_stencil8_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return 16u;
}

uint32_t metal_FeatureSetCapabilities_x32_stencil8_capabilities(uint64_t feature_set) {
    return feature_x32_stencil8_capabilities(feature_set);
}

static uint32_t feature_bgra10_xr_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_supports_extended_range_pixel_formats(feature_set)) ? (63u) : (0u));
}

uint32_t metal_FeatureSetCapabilities_bgra10_xr_capabilities(uint64_t feature_set) {
    return feature_bgra10_xr_capabilities(feature_set);
}

static uint32_t feature_bgra10_xr_srgb_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_supports_extended_range_pixel_formats(feature_set)) ? (63u) : (0u));
}

uint32_t metal_FeatureSetCapabilities_bgra10_xr_srgb_capabilities(uint64_t feature_set) {
    return feature_bgra10_xr_srgb_capabilities(feature_set);
}

static uint32_t feature_bgr10_xr_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_supports_extended_range_pixel_formats(feature_set)) ? (63u) : (0u));
}

uint32_t metal_FeatureSetCapabilities_bgr10_xr_capabilities(uint64_t feature_set) {
    return feature_bgr10_xr_capabilities(feature_set);
}

static uint32_t feature_bgr10_xr_srgb_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_supports_extended_range_pixel_formats(feature_set)) ? (63u) : (0u));
}

uint32_t metal_FeatureSetCapabilities_bgr10_xr_srgb_capabilities(uint64_t feature_set) {
    return feature_bgr10_xr_srgb_capabilities(feature_set);
}

static uint32_t feature_bgr10_a2_unorm_capabilities(uint64_t feature_set) {
    (void)feature_set;
    return ((feature_supports_wide_color_pixel_format(feature_set)) ? (((feature_os(feature_set) == 1) ? (61u) : (63u))) : (0u));
}

uint32_t metal_FeatureSetCapabilities_bgr10_a2_unorm_capabilities(uint64_t feature_set) {
    return feature_bgr10_a2_unorm_capabilities(feature_set);
}

void * metal_Device_system_default(void) {
    return (void *)MTLCreateSystemDefaultDevice();
}

void * metal_Device_all(void) {
#if TARGET_OS_IOS || TARGET_OS_VISION
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    NSArray *devices = device ? [[NSArray alloc] initWithObjects:device, nil] : [[NSArray alloc] init];
    [device release];
    return (void *)devices;
#else
    return (void *)MTLCopyAllDevices();
#endif
}

const uint8_t * metal_Device_name(void * object) {
    NSString *value = ((id (*)(id, SEL))objc_msgSend)((id)object, sel_registerName("name"));
    return (const uint8_t *)[value UTF8String];
}

const uint8_t * metal_Device_vendor(void * object) {
    NSString *value = ((id (*)(id, SEL))objc_msgSend)((id)object, sel_registerName("vendorName"));
    return (const uint8_t *)[value UTF8String];
}

const uint8_t * metal_Device_family_name(void * object) {
    NSString *value = ((id (*)(id, SEL))objc_msgSend)((id)object, sel_registerName("familyName"));
    return (const uint8_t *)[value UTF8String];
}

uint64_t metal_Device_registry_id(void * object) {
    return [(id<MTLDevice>)object registryID];
}

uint64_t metal_Device_location(void * object) {
    return [(id<MTLDevice>)object location];
}

uint64_t metal_Device_location_number(void * object) {
    return [(id<MTLDevice>)object locationNumber];
}

uint64_t metal_Device_max_threadgroup_memory_length(void * object) {
    return [(id<MTLDevice>)object maxThreadgroupMemoryLength];
}

MTLSize metal_Device_max_threads_per_threadgroup(void * object) {
    return [(id<MTLDevice>)object maxThreadsPerThreadgroup];
}

uint8_t metal_Device_is_low_power(void * object) {
    return [(id<MTLDevice>)object isLowPower];
}

uint8_t metal_Device_is_headless(void * object) {
    return [(id<MTLDevice>)object isHeadless];
}

uint8_t metal_Device_is_removable(void * object) {
    return [(id<MTLDevice>)object isRemovable];
}

uint8_t metal_Device_supports_raytracing(void * object) {
    return [(id<MTLDevice>)object supportsRaytracing];
}

uint8_t metal_Device_has_unified_memory(void * object) {
    return [(id<MTLDevice>)object hasUnifiedMemory];
}

uint64_t metal_Device_recommended_max_working_set_size(void * object) {
    return [(id<MTLDevice>)object recommendedMaxWorkingSetSize];
}

uint64_t metal_Device_max_transfer_rate(void * object) {
    return [(id<MTLDevice>)object maxTransferRate];
}

uint8_t metal_Device_supports_shader_barycentric_coordinates(void * object) {
    return [(id<MTLDevice>)object supportsShaderBarycentricCoordinates];
}

uint8_t metal_Device_supports_function_pointers(void * object) {
    return [(id<MTLDevice>)object supportsFunctionPointers];
}

uint8_t metal_Device_supports_dynamic_libraries(void * object) {
    return [(id<MTLDevice>)object supportsDynamicLibraries];
}

uint8_t metal_Device_d24_s8_supported(void * object) {
    return [(id<MTLDevice>)object isDepth24Stencil8PixelFormatSupported];
}

void * metal_Device_new_fence(void * object) {
    return (void *)[(id<MTLDevice>)object newFence];
}

void * metal_Device_new_command_queue(void * object) {
    return (void *)[(id<MTLDevice>)object newCommandQueue];
}

void * metal_Device_new_default_library(void * object) {
    return (void *)[(id<MTLDevice>)object newDefaultLibrary];
}

uint64_t metal_Device_argument_buffers_support(void * object) {
    return [(id<MTLDevice>)object argumentBuffersSupport];
}

uint64_t metal_Device_read_write_texture_support(void * object) {
    return [(id<MTLDevice>)object readWriteTextureSupport];
}

uint8_t metal_Device_raster_order_groups_supported(void * object) {
    return [(id<MTLDevice>)object areRasterOrderGroupsSupported];
}

uint8_t metal_Device_supports_32bit_float_filtering(void * object) {
    return [(id<MTLDevice>)object supports32BitFloatFiltering];
}

uint8_t metal_Device_supports_32bit_MSAA(void * object) {
    return [(id<MTLDevice>)object supports32BitMSAA];
}

uint8_t metal_Device_supports_query_texture_LOD(void * object) {
    return [(id<MTLDevice>)object supportsQueryTextureLOD];
}

uint8_t metal_Device_supports_BC_texture_compression(void * object) {
    return [(id<MTLDevice>)object supportsBCTextureCompression];
}

uint8_t metal_Device_supports_pull_model_interpolation(void * object) {
    return [(id<MTLDevice>)object supportsPullModelInterpolation];
}

void * metal_Device_new_event(void * object) {
    return (void *)[(id<MTLDevice>)object newEvent];
}

void * metal_Device_new_shared_event(void * object) {
    return (void *)[(id<MTLDevice>)object newSharedEvent];
}

uint64_t metal_Device_max_argument_buffer_sampler_count(void * object) {
    return [(id<MTLDevice>)object maxArgumentBufferSamplerCount];
}

uint64_t metal_Device_current_allocated_size(void * object) {
    return [(id<MTLDevice>)object currentAllocatedSize];
}

uint64_t metal_Device_max_buffer_length(void * object) {
    return [(id<MTLDevice>)object maxBufferLength];
}

void * metal_Device_counter_sets(void * object) {
    return (void *)[(id<MTLDevice>)object counterSets];
}

uint8_t metal_Device_supports_feature_set(void * object, uint64_t feature) {
    return [(id<MTLDevice>)object supportsFeatureSet:(MTLFeatureSet)feature];
}

uint8_t metal_Device_supports_family(void * object, int64_t family) {
    return [(id<MTLDevice>)object supportsFamily:(MTLGPUFamily)family];
}

uint8_t metal_Device_supports_vertex_amplification_count(void * object, uint64_t count) {
    return [(id<MTLDevice>)object supportsVertexAmplificationCount:(NSUInteger)count];
}

uint8_t metal_Device_supports_texture_sample_count(void * object, uint64_t count) {
    return [(id<MTLDevice>)object supportsTextureSampleCount:(NSUInteger)count];
}

uint8_t metal_Device_supports_counter_sampling(void * object, uint64_t sampling_point) {
    return [(id<MTLDevice>)object supportsCounterSampling:(MTLCounterSamplingPoint)sampling_point];
}

uint64_t metal_Device_minimum_linear_texture_alignment_for_pixel_format(void * object, uint64_t format) {
    return [(id<MTLDevice>)object minimumLinearTextureAlignmentForPixelFormat:(MTLPixelFormat)format];
}

uint64_t metal_Device_minimum_texture_buffer_alignment_for_pixel_format(void * object, uint64_t format) {
    return [(id<MTLDevice>)object minimumTextureBufferAlignmentForPixelFormat:(MTLPixelFormat)format];
}

void * metal_Device_new_command_queue_with_max_command_buffer_count(void * object, uint64_t count) {
    return (void *)[(id<MTLDevice>)object newCommandQueueWithMaxCommandBufferCount:(NSUInteger)count];
}

void * metal_Device_new_texture(void * object, void * descriptor) {
    return (void *)[(id<MTLDevice>)object newTextureWithDescriptor:(MTLTextureDescriptor *)descriptor];
}

void * metal_Device_new_sampler(void * object, void * descriptor) {
    return (void *)[(id<MTLDevice>)object newSamplerStateWithDescriptor:(MTLSamplerDescriptor *)descriptor];
}

void * metal_Device_new_depth_stencil_state(void * object, void * descriptor) {
    return (void *)[(id<MTLDevice>)object newDepthStencilStateWithDescriptor:(MTLDepthStencilDescriptor *)descriptor];
}

void * metal_Device_new_argument_encoder(void * object, void * arguments) {
    return (void *)[(id<MTLDevice>)object newArgumentEncoderWithArguments:(NSArray<MTLArgumentDescriptor *> *)arguments];
}

void * metal_Device_new_heap(void * object, void * descriptor) {
    return (void *)[(id<MTLDevice>)object newHeapWithDescriptor:(MTLHeapDescriptor *)descriptor];
}

void * metal_Device_new_acceleration_structure_with_size(void * object, uint64_t size) {
    return (void *)[(id<MTLDevice>)object newAccelerationStructureWithSize:(NSUInteger)size];
}

void * metal_Device_new_dynamic_library(void * object, void * library, void ** error) {
    return (void *)[(id<MTLDevice>)object newDynamicLibrary:(id<MTLLibrary>)library error:(NSError **)error];
}

void * metal_Device_new_dynamic_library_with_url(void * object, void * url, void ** error) {
    return (void *)[(id<MTLDevice>)object newDynamicLibraryWithURL:(NSURL *)url error:(NSError **)error];
}

void * metal_Device_new_binary_archive_with_descriptor(void * object, void * descriptor, void ** error) {
    return (void *)[(id<MTLDevice>)object newBinaryArchiveWithDescriptor:(MTLBinaryArchiveDescriptor *)descriptor error:(NSError **)error];
}

void * metal_Device_new_render_pipeline_state(void * object, void * descriptor, void ** error) {
    return (void *)[(id<MTLDevice>)object newRenderPipelineStateWithDescriptor:(MTLRenderPipelineDescriptor *)descriptor error:(NSError **)error];
}

void * metal_Device_new_compute_pipeline_state_with_function(void * object, void * function, void ** error) {
    return (void *)[(id<MTLDevice>)object newComputePipelineStateWithFunction:(id<MTLFunction>)function error:(NSError **)error];
}

void * metal_Device_new_counter_sample_buffer_with_descriptor(void * object, void * descriptor, void ** error) {
    return (void *)[(id<MTLDevice>)object newCounterSampleBufferWithDescriptor:(MTLCounterSampleBufferDescriptor *)descriptor error:(NSError **)error];
}

void * metal_Device_new_render_pipeline_state_with_reflection(void * object, void * descriptor, uint64_t reflection_options, void ** reflection, void ** error) {
    MTLRenderPipelineReflection *value = nil;
    id state = [(id<MTLDevice>)object newRenderPipelineStateWithDescriptor:(MTLRenderPipelineDescriptor *)descriptor options:(MTLPipelineOption)reflection_options reflection:&value error:(NSError **)error];
    *reflection = (void *)[value retain];
    return (void *)state;
}

void * metal_Device_new_mesh_render_pipeline_state_with_reflection(void * object, void * descriptor, uint64_t reflection_options, void ** reflection, void ** error) {
    MTLRenderPipelineReflection *value = nil;
    id state = [(id<MTLDevice>)object newRenderPipelineStateWithMeshDescriptor:(MTLMeshRenderPipelineDescriptor *)descriptor options:(MTLPipelineOption)reflection_options reflection:&value error:(NSError **)error];
    *reflection = (void *)[value retain];
    return (void *)state;
}

void * metal_Device_new_compute_pipeline_state_with_reflection(void * object, void * descriptor, uint64_t reflection_options, void ** reflection, void ** error) {
    MTLComputePipelineReflection *value = nil;
    id state = [(id<MTLDevice>)object newComputePipelineStateWithDescriptor:(MTLComputePipelineDescriptor *)descriptor options:(MTLPipelineOption)reflection_options reflection:&value error:(NSError **)error];
    *reflection = (void *)[value retain];
    return (void *)state;
}

void * metal_Device_new_mesh_render_pipeline_state(void * object, void * descriptor, void ** error) {
    return (void *)[(id<MTLDevice>)object newRenderPipelineStateWithMeshDescriptor:(MTLMeshRenderPipelineDescriptor *)descriptor options:MTLPipelineOptionNone reflection:NULL error:(NSError **)error];
}

void * metal_Device_new_compute_pipeline_state(void * object, void * descriptor, void ** error) {
    return (void *)[(id<MTLDevice>)object newComputePipelineStateWithDescriptor:(MTLComputePipelineDescriptor *)descriptor options:MTLPipelineOptionNone reflection:NULL error:(NSError **)error];
}

void * metal_Device_new_library_with_source(void * object, const uint8_t * src, void * options, void ** error) {
    NSString *source = [NSString stringWithUTF8String:(const char *)src];
    return (void *)[(id<MTLDevice>)object newLibraryWithSource:source options:(MTLCompileOptions *)options error:(NSError **)error];
}

void * metal_Device_new_library_with_file(void * object, const uint8_t * file, void ** error) {
    NSString *filename = [NSString stringWithUTF8String:(const char *)file];
    return (void *)[(id<MTLDevice>)object newLibraryWithFile:filename error:(NSError **)error];
}

void * metal_Device_new_library_with_data(void * object, const uint8_t * library_data, uint64_t length, void ** error) {
    dispatch_data_t data = dispatch_data_create(library_data, (size_t)length, dispatch_get_main_queue(), DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    id<MTLLibrary> library = [(id<MTLDevice>)object newLibraryWithData:data error:(NSError **)error];
    dispatch_release(data);
    return (void *)library;
}

void * metal_Device_new_buffer(void * object, uint64_t length, uint64_t options) {
    return (void *)[(id<MTLDevice>)object newBufferWithLength:(NSUInteger)length options:(MTLResourceOptions)options];
}

void * metal_Device_new_buffer_with_data(void * object, const uint8_t * bytes, uint64_t length, uint64_t options) {
    return (void *)[(id<MTLDevice>)object newBufferWithBytes:bytes length:(NSUInteger)length options:(MTLResourceOptions)options];
}

void * metal_Device_new_buffer_with_bytes_no_copy(void * object, uint8_t * bytes, uint64_t length, uint64_t options, void (*deallocator)(void *, uint8_t *, uint64_t), void * context, uint8_t has_deallocator) {
    if (!has_deallocator) {
        return (void *)[(id<MTLDevice>)object newBufferWithBytesNoCopy:bytes length:(NSUInteger)length options:(MTLResourceOptions)options deallocator:nil];
    }
    return (void *)[(id<MTLDevice>)object newBufferWithBytesNoCopy:bytes length:(NSUInteger)length options:(MTLResourceOptions)options deallocator:^(void *pointer, NSUInteger size) {
        deallocator(context, (uint8_t *)pointer, (uint64_t)size);
    }];
}

void * metal_Device_new_indirect_command_buffer_with_descriptor(void * object, void * descriptor, uint64_t max_command_count, uint64_t options) {
    return (void *)[(id<MTLDevice>)object newIndirectCommandBufferWithDescriptor:(MTLIndirectCommandBufferDescriptor *)descriptor maxCommandCount:(NSUInteger)max_command_count options:(MTLResourceOptions)options];
}

MTLSizeAndAlign metal_Device_heap_buffer_size_and_align(void * object, uint64_t length, uint64_t options) {
    return [(id<MTLDevice>)object heapBufferSizeAndAlignWithLength:(NSUInteger)length options:(MTLResourceOptions)options];
}

MTLSizeAndAlign metal_Device_heap_acceleration_structure_size_and_align_with_size(void * object, uint64_t size) {
    return [(id<MTLDevice>)object heapAccelerationStructureSizeAndAlignWithSize:(NSUInteger)size];
}

MTLSizeAndAlign metal_Device_heap_texture_size_and_align(void * object, void * descriptor) {
    return [(id<MTLDevice>)object heapTextureSizeAndAlignWithDescriptor:(MTLTextureDescriptor *)descriptor];
}

MTLAccelerationStructureSizes metal_Device_acceleration_structure_sizes_with_descriptor(void * object, void * desc) {
    return [(id<MTLDevice>)object accelerationStructureSizesWithDescriptor:(MTLAccelerationStructureDescriptor *)desc];
}

void metal_Device_sample_timestamps(void * object, uint64_t * cpu_timestamp, uint64_t * gpu_timestamp) {
    [(id<MTLDevice>)object sampleTimestamps:(MTLTimestamp *)cpu_timestamp gpuTimestamp:(MTLTimestamp *)gpu_timestamp];
}
