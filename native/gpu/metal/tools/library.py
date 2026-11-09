# SPDX-License-Identifier: MIT OR Apache-2.0
# Port of metal-rs, Copyright 2017 GFX developers.
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]
OBJ = '?*mut u8'
STR = '?[*]const u8'
ERR = '?*mut ?*mut u8'
CTYPES = {
    OBJ: 'void *', STR: 'const char *', ERR: 'void **',
    'u64': 'uint64_t', 'u8': 'uint8_t', 'bool': 'uint8_t', '()': 'void',
    '?*const u8': 'const void *', 'NSRange': 'NSRange',
    '?[*]const ?*mut u8': 'void *const *', '[*]mut u64': 'uint64_t *',
}
CLASSES = {
    'VertexAttribute': 'MTLVertexAttribute *', 'Attribute': 'MTLAttribute *',
    'FunctionConstant': 'MTLFunctionConstant *', 'FunctionDescriptor': 'MTLFunctionDescriptor *',
    'IntersectionFunctionDescriptor': 'MTLIntersectionFunctionDescriptor *',
    'FunctionHandle': 'id<MTLFunctionHandle>', 'Function': 'id<MTLFunction>',
    'FunctionConstantValues': 'MTLFunctionConstantValues *', 'CompileOptions': 'MTLCompileOptions *',
    'Library': 'id<MTLLibrary>', 'DynamicLibrary': 'id<MTLDynamicLibrary>',
    'BinaryArchiveDescriptor': 'MTLBinaryArchiveDescriptor *', 'BinaryArchive': 'id<MTLBinaryArchive>',
    'LinkedFunctions': 'MTLLinkedFunctions *', 'StructMember': 'MTLStructMember *',
    'StructMemberArray': 'NSArray<MTLStructMember *> *', 'StructType': 'MTLStructType *',
    'ArrayType': 'MTLArrayType *', 'Argument': 'MTLArgument *', 'ArgumentDescriptor': 'MTLArgumentDescriptor *',
    'CounterSampleBufferDescriptor': 'MTLCounterSampleBufferDescriptor *',
    'CounterSampleBuffer': 'id<MTLCounterSampleBuffer>', 'Counter': 'id<MTLCounter>',
    'CounterSet': 'id<MTLCounterSet>', 'CommonCounter': 'NSString *',
}
methods = {name: [] for name in CLASSES}

def add(t, name, args, result, body, note='', static=False):
    methods[t].append((name, args, result, body, note, static))

def get(t, name, selector=None, result='u64', note=''):
    sel = selector or name
    expr = f'[obj {sel}]'
    if result == STR:
        expr = f'[{expr} UTF8String]'
        note = note or 'Borrowed NUL-terminated UTF-8; valid while the owning NSString remains alive.'
    elif result == OBJ:
        expr = f'(void *){expr}'
        note = note or 'Borrowed +0 Objective-C object; retain explicitly if needed.'
    elif result == 'bool':
        expr = f'({expr} != NO)'
    add(t, name, [], result, f'return {expr};', note)

def setter(t, name, selector, ty='u64', arg='value', cast=''):
    expr = arg
    if ty == STR:
        expr = f'metal_library_string({arg})'
    elif ty == OBJ:
        expr = f'(id){arg}'
    elif ty == 'bool':
        expr = f'({arg} != 0)'
    if cast:
        expr = f'({cast}){expr}'
    add(t, name, [(arg, ty)], '()', f'[obj {selector}:{expr}];')

def new(t):
    add(t, 'new', [], OBJ, f'return (void *)[MTL{t} new];', 'Returns an owned +1 object; release exactly once.', static=True)

def label(t):
    get(t, 'label', result=STR)
    setter(t, 'set_label', 'setLabel', STR, 'label')

def error_method(t, name, selector, arg, cast, result='bool'):
    body = f'NSError *failure = nil;\n    {"BOOL" if result == "bool" else "id"} result = [obj {selector}:({cast}){arg} error:&failure];\n    if (error) *error = (void *)failure;\n    return {"result != NO" if result == "bool" else "(void *)result"};'
    add(t, name, [(arg, OBJ), ('error', ERR)], result, body,
        'NSError output is borrowed +0 and cleared on success. ' + ('Returns the Metal BOOL result.' if result == 'bool' else 'Result is owned +1, or null on failure.'))

for t in ('VertexAttribute', 'Attribute'):
    get(t, 'name', result=STR)
    get(t, 'attribute_index', 'attributeIndex')
    get(t, 'attribute_type', 'attributeType')
    get(t, 'is_active', 'isActive', 'bool')
    get(t, 'is_patch_data', 'isPatchData', 'bool')
    get(t, 'is_patch_control_point_data', 'isPatchControlPointData', 'bool')
get('FunctionConstant', 'name', result=STR)
get('FunctionConstant', 'data_type', 'type')
get('FunctionConstant', 'index')
get('FunctionConstant', 'required', result='bool')
new('FunctionDescriptor')
for name, sel in [('name', 'Name'), ('specialized_name', 'SpecializedName')]:
    get('FunctionDescriptor', name, sel[0].lower() + sel[1:], STR)
    setter('FunctionDescriptor', 'set_' + name, 'set' + sel, STR, 'name')
get('FunctionDescriptor', 'constant_values', 'constantValues', OBJ)
setter('FunctionDescriptor', 'set_constant_values', 'setConstantValues', OBJ, 'values')
get('FunctionDescriptor', 'options')
setter('FunctionDescriptor', 'set_options', 'setOptions', cast='MTLFunctionOptions')
for t in ('FunctionHandle', 'Function'):
    get(t, 'device', result=OBJ)
    get(t, 'name', result=STR)
    get(t, 'function_type', 'functionType')
label('Function')
get('Function', 'patch_type', 'patchType')
get('Function', 'patch_control_point_count', 'patchControlPointCount')
get('Function', 'vertex_attributes', 'vertexAttributes', OBJ, 'Borrowed +0 NSArray of MTLVertexAttribute objects.')
get('Function', 'stage_input_attributes', 'stageInputAttributes', OBJ, 'Borrowed +0 NSArray of MTLAttribute objects.')
add('Function', 'new_argument_encoder', [('buffer_index', 'u64')], OBJ,
    'return (void *)[obj newArgumentEncoderWithBufferIndex:buffer_index];', 'Returns an owned +1 argument encoder.')
get('Function', 'function_constants_dictionary', 'functionConstantsDictionary', OBJ, 'Borrowed +0 NSDictionary mapping NSString names to MTLFunctionConstant objects.')
get('Function', 'options')
new('FunctionConstantValues')
add('FunctionConstantValues', 'set_constant_value_at_index', [('value', '?*const u8'), ('ty', 'u64'), ('index', 'u64')], '()',
    '[obj setConstantValue:value type:(MTLDataType)ty atIndex:index];', 'Value must point to a live value matching the Metal data type during this call.')
add('FunctionConstantValues', 'set_constant_values_with_range', [('values', '?*const u8'), ('ty', 'u64'), ('range', 'NSRange')], '()',
    '[obj setConstantValues:values type:(MTLDataType)ty withRange:range];', 'Range is passed by value. Values must address range.length values matching the Metal data type during this call.')
add('FunctionConstantValues', 'set_constant_value_with_name', [('value', '?*const u8'), ('ty', 'u64'), ('name', STR)], '()',
    '[obj setConstantValue:value type:(MTLDataType)ty withName:metal_library_string(name)];', 'Value must point to a live value matching the Metal data type during this call.')
new('CompileOptions')
get('CompileOptions', 'preprocessor_macros', 'preprocessorMacros', OBJ, 'Borrowed +0 NSDictionary of NSString keys and NSString or NSNumber values.')
setter('CompileOptions', 'set_preprocessor_macros', 'setPreprocessorMacros', OBJ, 'defines')
get('CompileOptions', 'is_fast_math_enabled', 'fastMathEnabled', 'bool')
setter('CompileOptions', 'set_fast_math_enabled', 'setFastMathEnabled', 'bool', 'enabled')
get('CompileOptions', 'language_version', 'languageVersion')
setter('CompileOptions', 'set_language_version', 'setLanguageVersion', 'u64', 'version', 'MTLLanguageVersion')
get('CompileOptions', 'library_type', 'libraryType')
setter('CompileOptions', 'set_library_type', 'setLibraryType', 'u64', 'lib_type', 'MTLLibraryType')
get('CompileOptions', 'install_name', 'installName', STR)
setter('CompileOptions', 'set_install_name', 'setInstallName', STR, 'name')
for method in ('libraries', 'libraries_as_nsarray'):
    get('CompileOptions', method, 'libraries', OBJ, 'Borrowed +0 NSArray of MTLDynamicLibrary objects; replaces the Rust Vec with its source NSArray.')
add('CompileOptions', 'set_libraries', [('libraries', '?[*]const ?*mut u8'), ('count', 'u64')], '()',
    'NSArray *array = [[NSArray alloc] initWithObjects:(id const *)libraries count:count];\n    [obj setLibraries:array];\n    [array release];', 'Pointer addresses count nonnull library objects; null pointer permitted only for count zero.')
setter('CompileOptions', 'set_libraries_nsarray', 'setLibraries', OBJ, 'libraries')
get('CompileOptions', 'preserve_invariance', 'preserveInvariance', 'bool')
setter('CompileOptions', 'set_preserve_invariance', 'setPreserveInvariance', 'bool', 'preserve')
get('Library', 'device', result=OBJ)
label('Library')
add('Library', 'get_function', [('name', STR), ('constants', OBJ), ('error', ERR)], OBJ, '''NSString *functionName = metal_library_string(name);
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
    return (void *)result;''', 'Returns owned +1 function or null. Null constants selects the unspecialized Metal overload; nonnull selects specialization. NSError is borrowed +0; missing-function convenience errors retain the upstream message.')
get('Library', 'function_names', 'functionNames', OBJ, 'Borrowed +0 NSArray of NSString names; replaces the Rust Vec<String>.')
get('Library', 'library_type', 'type')
get('Library', 'install_name', 'installName', STR, 'Borrowed UTF-8 install name; null represents upstream None.')
for name, selector, cast in [('new_function_with_descriptor', 'newFunctionWithDescriptor', 'MTLFunctionDescriptor *'), ('new_intersection_function_with_descriptor', 'newIntersectionFunctionWithDescriptor', 'MTLIntersectionFunctionDescriptor *')]:
    add('Library', name, [('descriptor', OBJ), ('error', ERR)], OBJ,
        f'''NSError *failure = nil;
    id<MTLFunction> result = [obj {selector}:({cast})descriptor error:&failure];
    if (!result && !failure) {{
        failure = [NSError errorWithDomain:MTLLibraryErrorDomain code:MTLLibraryErrorFunctionNotFound
            userInfo:@{{NSLocalizedDescriptionKey: @"{name}() failed"}}];
    }}
    if (error) *error = (void *)failure;
    return (void *)result;''', 'Result is owned +1 or null. NSError output is borrowed +0; fallback error preserves the upstream convenience failure message.')
get('DynamicLibrary', 'device', result=OBJ)
label('DynamicLibrary')
get('DynamicLibrary', 'install_name', 'installName', STR)
error_method('DynamicLibrary', 'serialize_to_url', 'serializeToURL', 'url', 'NSURL *')
new('BinaryArchiveDescriptor')
get('BinaryArchiveDescriptor', 'url', result=OBJ)
setter('BinaryArchiveDescriptor', 'set_url', 'setUrl', OBJ, 'url')
get('BinaryArchive', 'device', result=OBJ)
label('BinaryArchive')
error_method('BinaryArchive', 'add_compute_pipeline_functions_with_descriptor', 'addComputePipelineFunctionsWithDescriptor', 'descriptor', 'MTLComputePipelineDescriptor *')
error_method('BinaryArchive', 'add_render_pipeline_functions_with_descriptor', 'addRenderPipelineFunctionsWithDescriptor', 'descriptor', 'MTLRenderPipelineDescriptor *')
error_method('BinaryArchive', 'serialize_to_url', 'serializeToURL', 'url', 'NSURL *')
new('LinkedFunctions')
for name, sel in [('functions', 'Functions'), ('binary_functions', 'BinaryFunctions')]:
    get('LinkedFunctions', name, sel[0].lower() + sel[1:], OBJ, 'Borrowed +0 NSArray of MTLFunction objects; replaces the Rust Vec.')
    add('LinkedFunctions', 'set_' + name, [('functions', '?[*]const ?*mut u8'), ('count', 'u64')], '()',
        f'NSArray *array = [[NSArray alloc] initWithObjects:(id const *)functions count:count];\n    [obj set{sel}:array];\n    [array release];', 'Pointer addresses count nonnull functions; null pointer permitted only for count zero.')
get('StructMember', 'name', result=STR)
get('StructMember', 'offset')
get('StructMember', 'data_type', 'dataType')
get('StructMember', 'struct_type', 'structType', OBJ)
get('StructMember', 'array_type', 'arrayType', OBJ)
add('StructMemberArray', 'object_at', [('index', 'u64')], OBJ, 'return (void *)[obj objectAtIndexedSubscript:index];', 'Borrowed +0 member. Index must be within the NSArray bounds.')
get('StructMemberArray', 'count')
get('StructType', 'members', result=OBJ)
add('StructType', 'member_from_name', [('name', STR)], OBJ, 'return (void *)[obj memberByName:metal_library_string(name)];', 'Borrowed +0 member, or null when absent.')
get('ArrayType', 'array_length', 'arrayLength')
get('ArrayType', 'stride')
get('ArrayType', 'element_type', 'elementType')
get('ArrayType', 'element_struct_type', 'elementStructType', OBJ)
get('ArrayType', 'element_array_type', 'elementArrayType', OBJ)
get('Argument', 'name', result=STR)
get('Argument', 'type_', 'type')
get('Argument', 'access')
get('Argument', 'index')
get('Argument', 'is_active', 'isActive', 'bool')
for name, sel in [('buffer_alignment', 'bufferAlignment'), ('buffer_data_size', 'bufferDataSize'), ('buffer_data_type', 'bufferDataType'), ('threadgroup_memory_alignment', 'threadgroupMemoryAlignment'), ('threadgroup_memory_data_size', 'threadgroupMemoryDataSize'), ('texture_type', 'textureType'), ('texture_data_type', 'textureDataType')]:
    get('Argument', name, sel)
get('Argument', 'buffer_struct_type', 'bufferStructType', OBJ)
add('ArgumentDescriptor', 'new', [], OBJ, 'return (void *)[[MTLArgumentDescriptor argumentDescriptor] retain];', 'Returns owned +1; explicitly retains the upstream autoreleased factory result.', static=True)
for name, sel, arg, cast in [('set_data_type', 'setDataType', 'ty', 'MTLDataType'), ('set_index', 'setIndex', 'index', ''), ('set_access', 'setAccess', 'access', 'MTLArgumentAccess'), ('set_array_length', 'setArrayLength', 'length', ''), ('set_texture_type', 'setTextureType', 'ty', 'MTLTextureType')]:
    setter('ArgumentDescriptor', name, sel, 'u64', arg, cast)
new('CounterSampleBufferDescriptor')
get('CounterSampleBufferDescriptor', 'counter_set', 'counterSet', OBJ)
setter('CounterSampleBufferDescriptor', 'set_counter_set', 'setCounterSet', OBJ, 'counter_set')
label('CounterSampleBufferDescriptor')
get('CounterSampleBufferDescriptor', 'sample_count', 'sampleCount')
setter('CounterSampleBufferDescriptor', 'set_sample_count', 'setSampleCount', 'u64', 'sample_count')
get('CounterSampleBufferDescriptor', 'storage_mode', 'storageMode')
setter('CounterSampleBufferDescriptor', 'set_storage_mode', 'setStorageMode', 'u64', 'storage_mode', 'MTLStorageMode')
get('CounterSampleBuffer', 'sample_count', 'sampleCount')
add('CounterSampleBuffer', 'resolve_counter_range', [('range', 'NSRange'), ('out', '[*]mut u64')], '()',
    'NSData *data = [obj resolveCounterRange:range];\n    [data getBytes:out length:range.length * sizeof(NSUInteger)];',
    'Range is passed by value. Writes range.length u64 samples into caller-owned output, matching the upstream Vec<NSUInteger> helper. Range must be valid and range.length * 8 must not overflow; output must hold that many bytes.')
get('CounterSet', 'name', result=STR)

silk = ['// SPDX-License-Identifier: MIT OR Apache-2.0', '// Port of metal-rs, Copyright 2017 GFX developers.', '// Unsafe raw Metal API. The caller guarantees receiver type, object lifetime and OS availability.', '// Objects are borrowed +0 unless a method explicitly documents owned +1.', '// String inputs are NUL-terminated UTF-8. Release owned returns using the runtime API.', '', 'import types { NSRange }', '']
native = ['// SPDX-License-Identifier: MIT OR Apache-2.0', '// Port of metal-rs, Copyright 2017 GFX developers.', '#import <Foundation/Foundation.h>', '#import <Metal/Metal.h>', '#include <stdint.h>', '', 'static NSString *metal_library_string(const char *value) {', '    return value ? [NSString stringWithUTF8String:value] : nil;', '}', '']
coverage = []
for t, entries in methods.items():
    silk += [f'pub struct {t} {{}}']
    if not entries:
        silk += ['']
        continue
    silk += [f'impl {t} {{']
    declarations = []
    for name, args, result, body, note, static in entries:
        params = ([] if static else [('object', OBJ)]) + args
        sym = f'metal_{t}_{name}'
        sig = ', '.join(f'{n}: {ty}' for n, ty in params)
        if note:
            silk += ['  // ' + note]
        silk += [f'  pub unsafe fn {name}({sig}) -> {result} {{']
        call_args = []
        for n, ty in params:
            if ty == 'bool':
                silk += [f'    let mut {n}_abi: u8 = 0', f'    if {n} {{', f'      {n}_abi = 1', '    }']
                call_args.append(n + '_abi')
            else:
                call_args.append(n)
        call = f'unsafe {sym}({", ".join(call_args)})'
        if result == 'bool':
            silk += [f'    return {call} != 0']
        elif result == '()':
            silk += ['    ' + call]
        else:
            silk += ['    return ' + call]
        silk += ['  }']
        abi_sig = ', '.join(f'{n}: {"u8" if ty == "bool" else ty}' for n, ty in params)
        abi_result = 'u8' if result == 'bool' else result
        declarations += [f'unsafe extern "C" fn {sym}({abi_sig}) -> {abi_result}']
        c_sig = ', '.join(f'{CTYPES[ty]} {n}' for n, ty in params) or 'void'
        native += [f'{CTYPES[result]} {sym}({c_sig}) {{']
        if not static:
            native += [f'    {CLASSES[t]} obj = ({CLASSES[t]})object;']
        native += ['    ' + body, '}', '']
        coverage.append({'rust': f'{t if static else t + "Ref"}.{name}', 'silk': f'library.{t}.{name}', 'notes': note or 'Direct Metal selector bridge; raw object lifetime and availability are caller obligations.'})
    silk += ['}', ''] + declarations + ['']
for file, content in [('src/library.silk', '\n'.join(silk)), ('native/library.m', '\n'.join(native)), ('api/library.json', json.dumps(coverage, indent=2) + '\n')]:
    (ROOT / file).parent.mkdir(parents=True, exist_ok=True)
    (ROOT / file).write_text(content)
