// SPDX-License-Identifier: MIT OR Apache-2.0
// Port of metal-rs, Copyright 2017 GFX developers.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <stdint.h>

static NSString *metal_library_string(const char *value) {
    return value ? [NSString stringWithUTF8String:value] : nil;
}

const char * metal_VertexAttribute_name(void * object) {
    MTLVertexAttribute * obj = (MTLVertexAttribute *)object;
    return [[obj name] UTF8String];
}

uint64_t metal_VertexAttribute_attribute_index(void * object) {
    MTLVertexAttribute * obj = (MTLVertexAttribute *)object;
    return [obj attributeIndex];
}

uint64_t metal_VertexAttribute_attribute_type(void * object) {
    MTLVertexAttribute * obj = (MTLVertexAttribute *)object;
    return [obj attributeType];
}

uint8_t metal_VertexAttribute_is_active(void * object) {
    MTLVertexAttribute * obj = (MTLVertexAttribute *)object;
    return ([obj isActive] != NO);
}

uint8_t metal_VertexAttribute_is_patch_data(void * object) {
    MTLVertexAttribute * obj = (MTLVertexAttribute *)object;
    return ([obj isPatchData] != NO);
}

uint8_t metal_VertexAttribute_is_patch_control_point_data(void * object) {
    MTLVertexAttribute * obj = (MTLVertexAttribute *)object;
    return ([obj isPatchControlPointData] != NO);
}

const char * metal_Attribute_name(void * object) {
    MTLAttribute * obj = (MTLAttribute *)object;
    return [[obj name] UTF8String];
}

uint64_t metal_Attribute_attribute_index(void * object) {
    MTLAttribute * obj = (MTLAttribute *)object;
    return [obj attributeIndex];
}

uint64_t metal_Attribute_attribute_type(void * object) {
    MTLAttribute * obj = (MTLAttribute *)object;
    return [obj attributeType];
}

uint8_t metal_Attribute_is_active(void * object) {
    MTLAttribute * obj = (MTLAttribute *)object;
    return ([obj isActive] != NO);
}

uint8_t metal_Attribute_is_patch_data(void * object) {
    MTLAttribute * obj = (MTLAttribute *)object;
    return ([obj isPatchData] != NO);
}

uint8_t metal_Attribute_is_patch_control_point_data(void * object) {
    MTLAttribute * obj = (MTLAttribute *)object;
    return ([obj isPatchControlPointData] != NO);
}

const char * metal_FunctionConstant_name(void * object) {
    MTLFunctionConstant * obj = (MTLFunctionConstant *)object;
    return [[obj name] UTF8String];
}

uint64_t metal_FunctionConstant_data_type(void * object) {
    MTLFunctionConstant * obj = (MTLFunctionConstant *)object;
    return [obj type];
}

uint64_t metal_FunctionConstant_index(void * object) {
    MTLFunctionConstant * obj = (MTLFunctionConstant *)object;
    return [obj index];
}

uint8_t metal_FunctionConstant_required(void * object) {
    MTLFunctionConstant * obj = (MTLFunctionConstant *)object;
    return ([obj required] != NO);
}

void * metal_FunctionDescriptor_new(void) {
    return (void *)[MTLFunctionDescriptor new];
}

const char * metal_FunctionDescriptor_name(void * object) {
    MTLFunctionDescriptor * obj = (MTLFunctionDescriptor *)object;
    return [[obj name] UTF8String];
}

void metal_FunctionDescriptor_set_name(void * object, const char * name) {
    MTLFunctionDescriptor * obj = (MTLFunctionDescriptor *)object;
    [obj setName:metal_library_string(name)];
}

const char * metal_FunctionDescriptor_specialized_name(void * object) {
    MTLFunctionDescriptor * obj = (MTLFunctionDescriptor *)object;
    return [[obj specializedName] UTF8String];
}

void metal_FunctionDescriptor_set_specialized_name(void * object, const char * name) {
    MTLFunctionDescriptor * obj = (MTLFunctionDescriptor *)object;
    [obj setSpecializedName:metal_library_string(name)];
}

void * metal_FunctionDescriptor_constant_values(void * object) {
    MTLFunctionDescriptor * obj = (MTLFunctionDescriptor *)object;
    return (void *)[obj constantValues];
}

void metal_FunctionDescriptor_set_constant_values(void * object, void * values) {
    MTLFunctionDescriptor * obj = (MTLFunctionDescriptor *)object;
    [obj setConstantValues:(id)values];
}

uint64_t metal_FunctionDescriptor_options(void * object) {
    MTLFunctionDescriptor * obj = (MTLFunctionDescriptor *)object;
    return [obj options];
}

void metal_FunctionDescriptor_set_options(void * object, uint64_t value) {
    MTLFunctionDescriptor * obj = (MTLFunctionDescriptor *)object;
    [obj setOptions:(MTLFunctionOptions)value];
}

void * metal_FunctionHandle_device(void * object) {
    id<MTLFunctionHandle> obj = (id<MTLFunctionHandle>)object;
    return (void *)[obj device];
}

const char * metal_FunctionHandle_name(void * object) {
    id<MTLFunctionHandle> obj = (id<MTLFunctionHandle>)object;
    return [[obj name] UTF8String];
}

uint64_t metal_FunctionHandle_function_type(void * object) {
    id<MTLFunctionHandle> obj = (id<MTLFunctionHandle>)object;
    return [obj functionType];
}

void * metal_Function_device(void * object) {
    id<MTLFunction> obj = (id<MTLFunction>)object;
    return (void *)[obj device];
}

const char * metal_Function_name(void * object) {
    id<MTLFunction> obj = (id<MTLFunction>)object;
    return [[obj name] UTF8String];
}

uint64_t metal_Function_function_type(void * object) {
    id<MTLFunction> obj = (id<MTLFunction>)object;
    return [obj functionType];
}

const char * metal_Function_label(void * object) {
    id<MTLFunction> obj = (id<MTLFunction>)object;
    return [[obj label] UTF8String];
}

void metal_Function_set_label(void * object, const char * label) {
    id<MTLFunction> obj = (id<MTLFunction>)object;
    [obj setLabel:metal_library_string(label)];
}

uint64_t metal_Function_patch_type(void * object) {
    id<MTLFunction> obj = (id<MTLFunction>)object;
    return [obj patchType];
}

uint64_t metal_Function_patch_control_point_count(void * object) {
    id<MTLFunction> obj = (id<MTLFunction>)object;
    return [obj patchControlPointCount];
}

void * metal_Function_vertex_attributes(void * object) {
    id<MTLFunction> obj = (id<MTLFunction>)object;
    return (void *)[obj vertexAttributes];
}

void * metal_Function_stage_input_attributes(void * object) {
    id<MTLFunction> obj = (id<MTLFunction>)object;
    return (void *)[obj stageInputAttributes];
}

void * metal_Function_new_argument_encoder(void * object, uint64_t buffer_index) {
    id<MTLFunction> obj = (id<MTLFunction>)object;
    return (void *)[obj newArgumentEncoderWithBufferIndex:buffer_index];
}

void * metal_Function_function_constants_dictionary(void * object) {
    id<MTLFunction> obj = (id<MTLFunction>)object;
    return (void *)[obj functionConstantsDictionary];
}

uint64_t metal_Function_options(void * object) {
    id<MTLFunction> obj = (id<MTLFunction>)object;
    return [obj options];
}

void * metal_FunctionConstantValues_new(void) {
    return (void *)[MTLFunctionConstantValues new];
}

void metal_FunctionConstantValues_set_constant_value_at_index(void * object, const void * value, uint64_t ty, uint64_t index) {
    MTLFunctionConstantValues * obj = (MTLFunctionConstantValues *)object;
    [obj setConstantValue:value type:(MTLDataType)ty atIndex:index];
}

void metal_FunctionConstantValues_set_constant_values_with_range(void * object, const void * values, uint64_t ty, NSRange range) {
    MTLFunctionConstantValues * obj = (MTLFunctionConstantValues *)object;
    [obj setConstantValues:values type:(MTLDataType)ty withRange:range];
}

void metal_FunctionConstantValues_set_constant_value_with_name(void * object, const void * value, uint64_t ty, const char * name) {
    MTLFunctionConstantValues * obj = (MTLFunctionConstantValues *)object;
    [obj setConstantValue:value type:(MTLDataType)ty withName:metal_library_string(name)];
}

void * metal_CompileOptions_new(void) {
    return (void *)[MTLCompileOptions new];
}

void * metal_CompileOptions_preprocessor_macros(void * object) {
    MTLCompileOptions * obj = (MTLCompileOptions *)object;
    return (void *)[obj preprocessorMacros];
}

void metal_CompileOptions_set_preprocessor_macros(void * object, void * defines) {
    MTLCompileOptions * obj = (MTLCompileOptions *)object;
    [obj setPreprocessorMacros:(id)defines];
}

uint8_t metal_CompileOptions_is_fast_math_enabled(void * object) {
    MTLCompileOptions * obj = (MTLCompileOptions *)object;
    return ([obj fastMathEnabled] != NO);
}

void metal_CompileOptions_set_fast_math_enabled(void * object, uint8_t enabled) {
    MTLCompileOptions * obj = (MTLCompileOptions *)object;
    [obj setFastMathEnabled:(enabled != 0)];
}

uint64_t metal_CompileOptions_language_version(void * object) {
    MTLCompileOptions * obj = (MTLCompileOptions *)object;
    return [obj languageVersion];
}

void metal_CompileOptions_set_language_version(void * object, uint64_t version) {
    MTLCompileOptions * obj = (MTLCompileOptions *)object;
    [obj setLanguageVersion:(MTLLanguageVersion)version];
}

uint64_t metal_CompileOptions_library_type(void * object) {
    MTLCompileOptions * obj = (MTLCompileOptions *)object;
    return [obj libraryType];
}

void metal_CompileOptions_set_library_type(void * object, uint64_t lib_type) {
    MTLCompileOptions * obj = (MTLCompileOptions *)object;
    [obj setLibraryType:(MTLLibraryType)lib_type];
}

const char * metal_CompileOptions_install_name(void * object) {
    MTLCompileOptions * obj = (MTLCompileOptions *)object;
    return [[obj installName] UTF8String];
}

void metal_CompileOptions_set_install_name(void * object, const char * name) {
    MTLCompileOptions * obj = (MTLCompileOptions *)object;
    [obj setInstallName:metal_library_string(name)];
}

void * metal_CompileOptions_libraries(void * object) {
    MTLCompileOptions * obj = (MTLCompileOptions *)object;
    return (void *)[obj libraries];
}

void * metal_CompileOptions_libraries_as_nsarray(void * object) {
    MTLCompileOptions * obj = (MTLCompileOptions *)object;
    return (void *)[obj libraries];
}

void metal_CompileOptions_set_libraries(void * object, void *const * libraries, uint64_t count) {
    MTLCompileOptions * obj = (MTLCompileOptions *)object;
    NSArray *array = [[NSArray alloc] initWithObjects:(id const *)libraries count:count];
    [obj setLibraries:array];
    [array release];
}

void metal_CompileOptions_set_libraries_nsarray(void * object, void * libraries) {
    MTLCompileOptions * obj = (MTLCompileOptions *)object;
    [obj setLibraries:(id)libraries];
}

uint8_t metal_CompileOptions_preserve_invariance(void * object) {
    MTLCompileOptions * obj = (MTLCompileOptions *)object;
    return ([obj preserveInvariance] != NO);
}

void metal_CompileOptions_set_preserve_invariance(void * object, uint8_t preserve) {
    MTLCompileOptions * obj = (MTLCompileOptions *)object;
    [obj setPreserveInvariance:(preserve != 0)];
}

void * metal_Library_device(void * object) {
    id<MTLLibrary> obj = (id<MTLLibrary>)object;
    return (void *)[obj device];
}

const char * metal_Library_label(void * object) {
    id<MTLLibrary> obj = (id<MTLLibrary>)object;
    return [[obj label] UTF8String];
}

void metal_Library_set_label(void * object, const char * label) {
    id<MTLLibrary> obj = (id<MTLLibrary>)object;
    [obj setLabel:metal_library_string(label)];
}

void * metal_Library_get_function(void * object, const char * name, void * constants, void ** error) {
    id<MTLLibrary> obj = (id<MTLLibrary>)object;
    NSString *functionName = metal_library_string(name);
    NSError *failure = nil;
    id<MTLFunction> result;
    if (constants) {
        result = [obj newFunctionWithName:functionName constantValues:(MTLFunctionConstantValues *)constants error:&failure];
    } else {
        result = [obj newFunctionWithName:functionName];
    }
    if (!result && !failure) {
        failure = [NSError errorWithDomain:MTLLibraryErrorDomain code:MTLLibraryErrorFunctionNotFound
            userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Function '%@' does not exist", functionName]}];
    }
    if (error) *error = (void *)failure;
    return (void *)result;
}

void * metal_Library_function_names(void * object) {
    id<MTLLibrary> obj = (id<MTLLibrary>)object;
    return (void *)[obj functionNames];
}

uint64_t metal_Library_library_type(void * object) {
    id<MTLLibrary> obj = (id<MTLLibrary>)object;
    return [obj type];
}

const char * metal_Library_install_name(void * object) {
    id<MTLLibrary> obj = (id<MTLLibrary>)object;
    return [[obj installName] UTF8String];
}

void * metal_Library_new_function_with_descriptor(void * object, void * descriptor, void ** error) {
    id<MTLLibrary> obj = (id<MTLLibrary>)object;
    NSError *failure = nil;
    id<MTLFunction> result = [obj newFunctionWithDescriptor:(MTLFunctionDescriptor *)descriptor error:&failure];
    if (!result && !failure) {
        failure = [NSError errorWithDomain:MTLLibraryErrorDomain code:MTLLibraryErrorFunctionNotFound
            userInfo:@{NSLocalizedDescriptionKey: @"new_function_with_descriptor() failed"}];
    }
    if (error) *error = (void *)failure;
    return (void *)result;
}

void * metal_Library_new_intersection_function_with_descriptor(void * object, void * descriptor, void ** error) {
    id<MTLLibrary> obj = (id<MTLLibrary>)object;
    NSError *failure = nil;
    id<MTLFunction> result = [obj newIntersectionFunctionWithDescriptor:(MTLIntersectionFunctionDescriptor *)descriptor error:&failure];
    if (!result && !failure) {
        failure = [NSError errorWithDomain:MTLLibraryErrorDomain code:MTLLibraryErrorFunctionNotFound
            userInfo:@{NSLocalizedDescriptionKey: @"new_intersection_function_with_descriptor() failed"}];
    }
    if (error) *error = (void *)failure;
    return (void *)result;
}

void * metal_DynamicLibrary_device(void * object) {
    id<MTLDynamicLibrary> obj = (id<MTLDynamicLibrary>)object;
    return (void *)[obj device];
}

const char * metal_DynamicLibrary_label(void * object) {
    id<MTLDynamicLibrary> obj = (id<MTLDynamicLibrary>)object;
    return [[obj label] UTF8String];
}

void metal_DynamicLibrary_set_label(void * object, const char * label) {
    id<MTLDynamicLibrary> obj = (id<MTLDynamicLibrary>)object;
    [obj setLabel:metal_library_string(label)];
}

const char * metal_DynamicLibrary_install_name(void * object) {
    id<MTLDynamicLibrary> obj = (id<MTLDynamicLibrary>)object;
    return [[obj installName] UTF8String];
}

uint8_t metal_DynamicLibrary_serialize_to_url(void * object, void * url, void ** error) {
    id<MTLDynamicLibrary> obj = (id<MTLDynamicLibrary>)object;
    NSError *failure = nil;
    BOOL result = [obj serializeToURL:(NSURL *)url error:&failure];
    if (error) *error = (void *)failure;
    return result != NO;
}

void * metal_BinaryArchiveDescriptor_new(void) {
    return (void *)[MTLBinaryArchiveDescriptor new];
}

void * metal_BinaryArchiveDescriptor_url(void * object) {
    MTLBinaryArchiveDescriptor * obj = (MTLBinaryArchiveDescriptor *)object;
    return (void *)[obj url];
}

void metal_BinaryArchiveDescriptor_set_url(void * object, void * url) {
    MTLBinaryArchiveDescriptor * obj = (MTLBinaryArchiveDescriptor *)object;
    [obj setUrl:(id)url];
}

void * metal_BinaryArchive_device(void * object) {
    id<MTLBinaryArchive> obj = (id<MTLBinaryArchive>)object;
    return (void *)[obj device];
}

const char * metal_BinaryArchive_label(void * object) {
    id<MTLBinaryArchive> obj = (id<MTLBinaryArchive>)object;
    return [[obj label] UTF8String];
}

void metal_BinaryArchive_set_label(void * object, const char * label) {
    id<MTLBinaryArchive> obj = (id<MTLBinaryArchive>)object;
    [obj setLabel:metal_library_string(label)];
}

uint8_t metal_BinaryArchive_add_compute_pipeline_functions_with_descriptor(void * object, void * descriptor, void ** error) {
    id<MTLBinaryArchive> obj = (id<MTLBinaryArchive>)object;
    NSError *failure = nil;
    BOOL result = [obj addComputePipelineFunctionsWithDescriptor:(MTLComputePipelineDescriptor *)descriptor error:&failure];
    if (error) *error = (void *)failure;
    return result != NO;
}

uint8_t metal_BinaryArchive_add_render_pipeline_functions_with_descriptor(void * object, void * descriptor, void ** error) {
    id<MTLBinaryArchive> obj = (id<MTLBinaryArchive>)object;
    NSError *failure = nil;
    BOOL result = [obj addRenderPipelineFunctionsWithDescriptor:(MTLRenderPipelineDescriptor *)descriptor error:&failure];
    if (error) *error = (void *)failure;
    return result != NO;
}

uint8_t metal_BinaryArchive_serialize_to_url(void * object, void * url, void ** error) {
    id<MTLBinaryArchive> obj = (id<MTLBinaryArchive>)object;
    NSError *failure = nil;
    BOOL result = [obj serializeToURL:(NSURL *)url error:&failure];
    if (error) *error = (void *)failure;
    return result != NO;
}

void * metal_LinkedFunctions_new(void) {
    return (void *)[MTLLinkedFunctions new];
}

void * metal_LinkedFunctions_functions(void * object) {
    MTLLinkedFunctions * obj = (MTLLinkedFunctions *)object;
    return (void *)[obj functions];
}

void metal_LinkedFunctions_set_functions(void * object, void *const * functions, uint64_t count) {
    MTLLinkedFunctions * obj = (MTLLinkedFunctions *)object;
    NSArray *array = [[NSArray alloc] initWithObjects:(id const *)functions count:count];
    [obj setFunctions:array];
    [array release];
}

void * metal_LinkedFunctions_binary_functions(void * object) {
    MTLLinkedFunctions * obj = (MTLLinkedFunctions *)object;
    return (void *)[obj binaryFunctions];
}

void metal_LinkedFunctions_set_binary_functions(void * object, void *const * functions, uint64_t count) {
    MTLLinkedFunctions * obj = (MTLLinkedFunctions *)object;
    NSArray *array = [[NSArray alloc] initWithObjects:(id const *)functions count:count];
    [obj setBinaryFunctions:array];
    [array release];
}

const char * metal_StructMember_name(void * object) {
    MTLStructMember * obj = (MTLStructMember *)object;
    return [[obj name] UTF8String];
}

uint64_t metal_StructMember_offset(void * object) {
    MTLStructMember * obj = (MTLStructMember *)object;
    return [obj offset];
}

uint64_t metal_StructMember_data_type(void * object) {
    MTLStructMember * obj = (MTLStructMember *)object;
    return [obj dataType];
}

void * metal_StructMember_struct_type(void * object) {
    MTLStructMember * obj = (MTLStructMember *)object;
    return (void *)[obj structType];
}

void * metal_StructMember_array_type(void * object) {
    MTLStructMember * obj = (MTLStructMember *)object;
    return (void *)[obj arrayType];
}

void * metal_StructMemberArray_object_at(void * object, uint64_t index) {
    NSArray<MTLStructMember *> * obj = (NSArray<MTLStructMember *> *)object;
    return (void *)[obj objectAtIndexedSubscript:index];
}

uint64_t metal_StructMemberArray_count(void * object) {
    NSArray<MTLStructMember *> * obj = (NSArray<MTLStructMember *> *)object;
    return [obj count];
}

void * metal_StructType_members(void * object) {
    MTLStructType * obj = (MTLStructType *)object;
    return (void *)[obj members];
}

void * metal_StructType_member_from_name(void * object, const char * name) {
    MTLStructType * obj = (MTLStructType *)object;
    return (void *)[obj memberByName:metal_library_string(name)];
}

uint64_t metal_ArrayType_array_length(void * object) {
    MTLArrayType * obj = (MTLArrayType *)object;
    return [obj arrayLength];
}

uint64_t metal_ArrayType_stride(void * object) {
    MTLArrayType * obj = (MTLArrayType *)object;
    return [obj stride];
}

uint64_t metal_ArrayType_element_type(void * object) {
    MTLArrayType * obj = (MTLArrayType *)object;
    return [obj elementType];
}

void * metal_ArrayType_element_struct_type(void * object) {
    MTLArrayType * obj = (MTLArrayType *)object;
    return (void *)[obj elementStructType];
}

void * metal_ArrayType_element_array_type(void * object) {
    MTLArrayType * obj = (MTLArrayType *)object;
    return (void *)[obj elementArrayType];
}

const char * metal_Argument_name(void * object) {
    MTLArgument * obj = (MTLArgument *)object;
    return [[obj name] UTF8String];
}

uint64_t metal_Argument_type_(void * object) {
    MTLArgument * obj = (MTLArgument *)object;
    return [obj type];
}

uint64_t metal_Argument_access(void * object) {
    MTLArgument * obj = (MTLArgument *)object;
    return [obj access];
}

uint64_t metal_Argument_index(void * object) {
    MTLArgument * obj = (MTLArgument *)object;
    return [obj index];
}

uint8_t metal_Argument_is_active(void * object) {
    MTLArgument * obj = (MTLArgument *)object;
    return ([obj isActive] != NO);
}

uint64_t metal_Argument_buffer_alignment(void * object) {
    MTLArgument * obj = (MTLArgument *)object;
    return [obj bufferAlignment];
}

uint64_t metal_Argument_buffer_data_size(void * object) {
    MTLArgument * obj = (MTLArgument *)object;
    return [obj bufferDataSize];
}

uint64_t metal_Argument_buffer_data_type(void * object) {
    MTLArgument * obj = (MTLArgument *)object;
    return [obj bufferDataType];
}

uint64_t metal_Argument_threadgroup_memory_alignment(void * object) {
    MTLArgument * obj = (MTLArgument *)object;
    return [obj threadgroupMemoryAlignment];
}

uint64_t metal_Argument_threadgroup_memory_data_size(void * object) {
    MTLArgument * obj = (MTLArgument *)object;
    return [obj threadgroupMemoryDataSize];
}

uint64_t metal_Argument_texture_type(void * object) {
    MTLArgument * obj = (MTLArgument *)object;
    return [obj textureType];
}

uint64_t metal_Argument_texture_data_type(void * object) {
    MTLArgument * obj = (MTLArgument *)object;
    return [obj textureDataType];
}

void * metal_Argument_buffer_struct_type(void * object) {
    MTLArgument * obj = (MTLArgument *)object;
    return (void *)[obj bufferStructType];
}

void * metal_ArgumentDescriptor_new(void) {
    return (void *)[[MTLArgumentDescriptor argumentDescriptor] retain];
}

void metal_ArgumentDescriptor_set_data_type(void * object, uint64_t ty) {
    MTLArgumentDescriptor * obj = (MTLArgumentDescriptor *)object;
    [obj setDataType:(MTLDataType)ty];
}

void metal_ArgumentDescriptor_set_index(void * object, uint64_t index) {
    MTLArgumentDescriptor * obj = (MTLArgumentDescriptor *)object;
    [obj setIndex:index];
}

void metal_ArgumentDescriptor_set_access(void * object, uint64_t access) {
    MTLArgumentDescriptor * obj = (MTLArgumentDescriptor *)object;
    [obj setAccess:(MTLArgumentAccess)access];
}

void metal_ArgumentDescriptor_set_array_length(void * object, uint64_t length) {
    MTLArgumentDescriptor * obj = (MTLArgumentDescriptor *)object;
    [obj setArrayLength:length];
}

void metal_ArgumentDescriptor_set_texture_type(void * object, uint64_t ty) {
    MTLArgumentDescriptor * obj = (MTLArgumentDescriptor *)object;
    [obj setTextureType:(MTLTextureType)ty];
}

void * metal_CounterSampleBufferDescriptor_new(void) {
    return (void *)[MTLCounterSampleBufferDescriptor new];
}

void * metal_CounterSampleBufferDescriptor_counter_set(void * object) {
    MTLCounterSampleBufferDescriptor * obj = (MTLCounterSampleBufferDescriptor *)object;
    return (void *)[obj counterSet];
}

void metal_CounterSampleBufferDescriptor_set_counter_set(void * object, void * counter_set) {
    MTLCounterSampleBufferDescriptor * obj = (MTLCounterSampleBufferDescriptor *)object;
    [obj setCounterSet:(id)counter_set];
}

const char * metal_CounterSampleBufferDescriptor_label(void * object) {
    MTLCounterSampleBufferDescriptor * obj = (MTLCounterSampleBufferDescriptor *)object;
    return [[obj label] UTF8String];
}

void metal_CounterSampleBufferDescriptor_set_label(void * object, const char * label) {
    MTLCounterSampleBufferDescriptor * obj = (MTLCounterSampleBufferDescriptor *)object;
    [obj setLabel:metal_library_string(label)];
}

uint64_t metal_CounterSampleBufferDescriptor_sample_count(void * object) {
    MTLCounterSampleBufferDescriptor * obj = (MTLCounterSampleBufferDescriptor *)object;
    return [obj sampleCount];
}

void metal_CounterSampleBufferDescriptor_set_sample_count(void * object, uint64_t sample_count) {
    MTLCounterSampleBufferDescriptor * obj = (MTLCounterSampleBufferDescriptor *)object;
    [obj setSampleCount:sample_count];
}

uint64_t metal_CounterSampleBufferDescriptor_storage_mode(void * object) {
    MTLCounterSampleBufferDescriptor * obj = (MTLCounterSampleBufferDescriptor *)object;
    return [obj storageMode];
}

void metal_CounterSampleBufferDescriptor_set_storage_mode(void * object, uint64_t storage_mode) {
    MTLCounterSampleBufferDescriptor * obj = (MTLCounterSampleBufferDescriptor *)object;
    [obj setStorageMode:(MTLStorageMode)storage_mode];
}

uint64_t metal_CounterSampleBuffer_sample_count(void * object) {
    id<MTLCounterSampleBuffer> obj = (id<MTLCounterSampleBuffer>)object;
    return [obj sampleCount];
}

void metal_CounterSampleBuffer_resolve_counter_range(void * object, NSRange range, uint64_t * out) {
    id<MTLCounterSampleBuffer> obj = (id<MTLCounterSampleBuffer>)object;
    NSData *data = [obj resolveCounterRange:range];
    [data getBytes:out length:range.length * sizeof(NSUInteger)];
}

const char * metal_CounterSet_name(void * object) {
    id<MTLCounterSet> obj = (id<MTLCounterSet>)object;
    return [[obj name] UTF8String];
}
