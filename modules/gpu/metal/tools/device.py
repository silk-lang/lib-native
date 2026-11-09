#!/usr/bin/env python3
# SPDX-License-Identifier: MIT OR Apache-2.0
# Derived from metal-rs, Copyright 2017 GFX developers.
"""Generate the device bridge from the upstream capability expressions.

Generation requires the metal-rs source checkout; generated bindings do not.
The small expression translator preserves upstream decisions, including legacy
feature-table quirks, rather than querying the host GPU for historical limits.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit('Usage: python3 tools/device.py /path/to/metal-rs/src/device.rs')
SOURCE = Path(sys.argv[1]).read_text()
HEADER = '// SPDX-License-Identifier: MIT OR Apache-2.0\n// Derived from metal-rs, Copyright 2017 GFX developers.\n'


def closing(text, start, left='{', right='}'):
    depth = 0
    for index in range(start, len(text)):
        if text[index] == left:
            depth += 1
        elif text[index] == right:
            depth -= 1
            if depth == 0:
                return index
    raise ValueError('Unclosed delimiter: ' + text[start:])


def split_top(text, delimiter=','):
    result, start, depth = [], 0, 0
    for index, char in enumerate(text):
        if char in '({[':
            depth += 1
        elif char in ')}]':
            depth -= 1
        elif char == delimiter and depth == 0:
            result.append(text[start:index].strip())
            start = index + 1
    result.append(text[start:].strip())
    return [part for part in result if part]


def functions(text):
    result = []
    for match in re.finditer(r'(?P<public>pub )?(?:unsafe )?fn (?P<name>\w+)(?:<[^\n]*?>)?\(', text):
        start = text.index('{', match.end())
        end = closing(text, start)
        signature = text[match.end():start]
        result.append((match.group('name'), signature, text[start + 1:end], bool(match.group('public'))))
    return result


feature_values = dict((name, value) for name, value in re.findall(r'(\w+) = (\d+),', SOURCE[SOURCE.index('pub enum MTLFeatureSet'):SOURCE.index('pub enum MTLGPUFamily')]))
cap_source = SOURCE[SOURCE.index('impl MTLFeatureSet {'):SOURCE.index('/// See <https://developer.apple.com/documentation/metal/mtlargumentbufferstier>')]
cap_methods = functions(cap_source)
cap_names = {name for name, _, _, _ in cap_methods}
flags = {'Filter': 1, 'Write': 2, 'Color': 4, 'Blend': 8, 'Msaa': 16, 'Resolve': 32}


def atom(text):
    text = re.sub(r'self\.(\w+)\(\)', lambda m: 'feature_' + m[1] + '(feature_set)', text)
    text = text.replace('PixelFormatCapabilities::all()', '63u').replace('PixelFormatCapabilities::empty()', '0u')
    text = re.sub(r'!PixelFormatCapabilities::(\w+)', lambda m: str(63 ^ flags[m[1]]) + 'u', text)
    text = re.sub(r'PixelFormatCapabilities::(\w+)', lambda m: str(flags[m[1]]) + 'u', text)
    text = text.replace('Os::iOS', '0').replace('Os::macOS', '1').replace('Os::tvOS', '2')
    text = re.sub(r'\b(KB|MB|GB)\b', lambda m: {'KB': '1024u', 'MB': '1048576u', 'GB': '1073741824u'}[m[1]], text)
    text = re.sub(r'\bself\b', 'feature_set', text)
    text = re.sub(r'\bNone\b', 'UINT64_MAX', text)
    text = re.sub(r'\bSome\((.*?)\)', r'(\1)', text)
    text = re.sub(r'\b(?:' + '|'.join(feature_values) + r')\b', lambda m: feature_values[m[0]], text)
    return ' '.join(text.split())


def pattern_condition(target, pattern):
    guard = None
    if ' if ' in pattern:
        pattern, guard = pattern.split(' if ', 1)
    alternatives = split_top(pattern, '|')
    conditions = []
    for alternative in alternatives:
        if alternative == '_':
            condition = '1'
        elif alternative.startswith('('):
            targets = split_top(target[1:-1])
            patterns = split_top(alternative[1:-1])
            condition = '(' + ' && '.join(pattern_condition(t, p) for t, p in zip(targets, patterns)) + ')'
        else:
            condition = '(' + atom(target) + ' == ' + atom(alternative) + ')'
        conditions.append(condition)
    condition = '(' + ' || '.join(conditions) + ')'
    return '(' + condition + ' && (' + atom(guard) + '))' if guard else condition


def expression(text):
    text = text.strip()
    text = re.sub(r'#\[[^\n]*\]\s*', '', text)
    if text.startswith('{') and closing(text, 0) == len(text) - 1:
        return expression(text[1:-1])
    if text.startswith('let '):
        name, rest = text[4:].split('=', 1)
        pieces = split_top(rest, ';')
        if len(pieces) != 2:
            raise ValueError('Unexpected let expression: ' + text)
        value = expression(pieces[0])
        return re.sub(r'\b' + name.strip() + r'\b', '(' + value + ')', expression(pieces[1]))
    if text.startswith('if '):
        opening = text.index('{')
        end = closing(text, opening)
        condition = atom(text[3:opening])
        yes = expression(text[opening + 1:end])
        rest = text[end + 1:].strip()
        if not rest.startswith('else '):
            raise ValueError('Missing else: ' + text)
        no = expression(rest[5:])
        return '((' + condition + ') ? (' + yes + ') : (' + no + '))'
    if text.startswith('match '):
        opening = text.index('{')
        target = text[6:opening].strip()
        body = text[opening + 1:closing(text, opening)].strip()
        arms = []
        while body:
            pattern, body = body.split('=>', 1)
            body = body.lstrip()
            if body.startswith('{'):
                end = closing(body, 0) + 1
            else:
                first = split_top(body)[0]
                end = len(first)
            arm, body = body[:end], body[end:].lstrip().lstrip(',').lstrip()
            arms.append((pattern_condition(target, pattern.strip()), expression(arm)))
        result = '(__builtin_unreachable(), 0)'
        for condition, value in reversed(arms):
            result = '(' + condition + ' ? (' + value + ') : (' + result + '))'
        return result
    return atom(text)


silk = [HEADER, 'import types { MTLSize, MTLSizeAndAlign, MTLAccelerationStructureSizes }\n']
native = [HEADER, '#import <Foundation/Foundation.h>\n#import <Metal/Metal.h>\n#import <TargetConditionals.h>\n#import <dispatch/dispatch.h>\n#import <objc/message.h>\n#include <stdint.h>\n#include <stdbool.h>\n']
api = []
declarations = []
wrappers = {'FeatureSetCapabilities': [], 'Device': []}


def emit(namespace, name, params, result, body, notes='', rust_type=None, static=False):
    """Parameters are (name, Silk type, C type), with bool marshalling."""
    symbol = 'metal_' + namespace + '_' + name
    result_c = {'()': 'void', 'bool': 'uint8_t', 'u32': 'uint32_t', 'u64': 'uint64_t', 'i64': 'int64_t', '?*mut u8': 'void *', '?[*]const u8': 'const uint8_t *', 'MTLSize': 'MTLSize', 'MTLSizeAndAlign': 'MTLSizeAndAlign', 'MTLAccelerationStructureSizes': 'MTLAccelerationStructureSizes'}[result]
    cparams = ', '.join(ctype.replace('$', pname) if '$' in ctype else ctype + ' ' + pname for pname, _, ctype in params) or 'void'
    native.append(result_c + ' ' + symbol + '(' + cparams + ') {\n' + body + '\n}\n')
    silk_params = ', '.join(pname + ': ' + stype for pname, stype, _ in params)
    foreign_params = ', '.join(pname + ': ' + ('u8' if stype == 'bool' else stype) for pname, stype, _ in params)
    callbacks = [pname for pname, stype, _ in params if stype.startswith('extern ')]
    directive = '@callbacks(' + ', '.join('"' + name + '"' for name in callbacks) + ')\n' if callbacks else ''
    declarations.append(directive + 'unsafe extern "C" fn ' + symbol + '(' + foreign_params + ') -> ' + ('u8' if result == 'bool' else result))
    lines = []
    for pname, stype, _ in params:
        if stype == 'bool':
            lines.append('    let mut ' + pname + '_abi: u8 = 0\n    if ' + pname + ' {\n      ' + pname + '_abi = 1\n    }')
    args = ', '.join(pname + ('_abi' if stype == 'bool' else '') for pname, stype, _ in params)
    call = 'unsafe ' + symbol + '(' + args + ')'
    lines.append('    ' + (call if result == '()' else 'return ' + call + (' != 0' if result == 'bool' else '')))
    comment = ('  /// ' + notes + '\n') if notes else ''
    wrappers[namespace].append(comment + '  pub unsafe fn ' + name + '(' + silk_params + ') -> ' + result + ' {\n' + '\n'.join(lines) + '\n  }')
    api.append({'rust': (rust_type or ('MTLFeatureSet' if namespace == 'FeatureSetCapabilities' else ('Device' if static else 'DeviceRef'))) + '.' + name, 'silk': 'device.' + namespace + '.' + name, 'notes': notes})


# Private helpers use the same historical enum categorization and match tables.
native.append('static uint32_t feature_os(uint64_t feature_set) {\n    if (feature_set < 10000) return 0;\n    if (feature_set < 20000) return 1;\n    if (feature_set >= 30000 && feature_set < 40000) return 2;\n    __builtin_unreachable();\n}\n')
for name, signature, body, public in cap_methods:
    if name == 'os':
        continue
    return_type = signature.split('->', 1)[1].strip()
    ctype = 'uint64_t' if return_type == 'Option<u32>' else ('bool' if return_type == 'bool' else 'uint32_t')
    body = re.sub(r'use MTLFeatureSet::\*;\s*', '', body)
    native.append('static ' + ctype + ' feature_' + name + '(uint64_t feature_set) {\n    (void)feature_set;\n    return ' + expression(body) + ';\n}\n')
    if not public:
        continue
    params = [('feature_set', 'u64', 'uint64_t')]
    notes = 'feature_set must be a defined MTLFeatureSet value; preserves the upstream historical capability table.'
    if return_type == 'Option<u32>':
        params.append(('out', '*mut u32', 'uint32_t *'))
        emit('FeatureSetCapabilities', name, params, 'bool', '    uint64_t value = feature_' + name + '(feature_set);\n    if (value == UINT64_MAX) return 0;\n    *out = (uint32_t)value;\n    return 1;', notes + ' Returns false for None without writing out; otherwise writes the Some value.')
    else:
        emit('FeatureSetCapabilities', name, params, 'bool' if return_type == 'bool' else 'u32', '    return feature_' + name + '(feature_set);', notes)


OBJ = ('object', '?*mut u8', 'void *')
ERROR = ('error', '?*mut ?*mut u8', 'void **')
owned = 'Returns an owned +1 object; release it when finished. Receiver and arguments must satisfy Metal availability and lifetime requirements.'
error_note = owned + ' error receives a borrowed +0 NSError, including compilation warnings when a non-null result is returned; pass null to ignore.'


def param(name, kind='object'):
    if kind == 'object':
        return (name, '?*mut u8', 'void *')
    if kind == 'string':
        return (name, '?[*]const u8', 'const uint8_t *')
    if kind == 'u64':
        return (name, 'u64', 'uint64_t')
    if kind == 'i64':
        return (name, 'i64', 'int64_t')
    if kind == 'out_u64':
        return (name, '*mut u64', 'uint64_t *')
    raise ValueError(kind)


def device(name, params, result, body, notes=''):
    emit('Device', name, [OBJ] + params, result, body, notes)


emit('Device', 'system_default', [], '?*mut u8', '    return (void *)MTLCreateSystemDefaultDevice();', 'Returns an owned +1 device, or null if Metal is unavailable.', static=True)
emit('Device', 'all', [], '?*mut u8', '''#if TARGET_OS_IOS || TARGET_OS_VISION
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    NSArray *devices = device ? [[NSArray alloc] initWithObjects:device, nil] : [[NSArray alloc] init];
    [device release];
    return (void *)devices;
#else
    return (void *)MTLCopyAllDevices();
#endif''', 'Returns an owned +1 NSArray of devices. Elements are borrowed from the array; retain them to outlive it.', static=True)
for name, selector in [('name', 'name'), ('vendor', 'vendorName'), ('family_name', 'familyName')]:
    body = '    NSString *value = ((id (*)(id, SEL))objc_msgSend)((id)object, sel_registerName("' + selector + '"));\n    return (const uint8_t *)[value UTF8String];'
    device(name, [], '?[*]const u8', body, 'Returns borrowed UTF-8 storage owned by the NSString property. ' + ('Private vendor API; caller must ensure selector availability.' if name != 'name' else 'Keep the device alive.'))

# Scalar and object getters: use selectors from the upstream method body.
device_source = SOURCE[SOURCE.index('impl DeviceRef {'):]
device_methods = functions(device_source)
implemented = {'system_default', 'all', 'name', 'vendor', 'family_name'}
scalar_types = {'u64': 'u64', 'NSUInteger': 'u64', 'MTLDeviceLocation': 'u64', 'MTLArgumentBuffersTier': 'u64', 'MTLReadWriteTextureTier': 'u64', 'bool': 'bool'}
for name, signature, body, public in device_methods:
    match = re.search(r'msg_send(?:_bool)?!\[self,\s*(\w+)\]', body)
    if not match or name in implemented:
        continue
    result = signature.split('->', 1)[1].strip()
    if result in scalar_types:
        selector = 'areRasterOrderGroupsSupported' if name == 'raster_order_groups_supported' else match[1]
        device(name, [], scalar_types[result], '    return [(id<MTLDevice>)object ' + selector + '];', 'Caller must ensure the device and selector are available.')
    elif result == 'MTLSize':
        device(name, [], 'MTLSize', '    return [(id<MTLDevice>)object ' + match[1] + '];', 'Returns the threadgroup dimensions by value.')
    elif name.startswith('new_'):
        device(name, [], '?*mut u8', '    return (void *)[(id<MTLDevice>)object ' + match[1] + '];', owned)
    elif name == 'counter_sets':
        device(name, [], '?*mut u8', '    return (void *)[(id<MTLDevice>)object counterSets];', 'Returns a borrowed +0 NSArray of counter sets; retain the array or individual elements to extend their lifetimes.')
    else:
        raise ValueError('Unhandled getter ' + name)
    implemented.add(name)

# One-argument queries, using scalar enum representations at the ABI boundary.
queries = [
    ('supports_feature_set', 'feature', 'u64', 'supportsFeatureSet', 'MTLFeatureSet', 'bool'),
    ('supports_family', 'family', 'i64', 'supportsFamily', 'MTLGPUFamily', 'bool'),
    ('supports_vertex_amplification_count', 'count', 'u64', 'supportsVertexAmplificationCount', 'NSUInteger', 'bool'),
    ('supports_texture_sample_count', 'count', 'u64', 'supportsTextureSampleCount', 'NSUInteger', 'bool'),
    ('supports_counter_sampling', 'sampling_point', 'u64', 'supportsCounterSampling', 'MTLCounterSamplingPoint', 'bool'),
    ('minimum_linear_texture_alignment_for_pixel_format', 'format', 'u64', 'minimumLinearTextureAlignmentForPixelFormat', 'MTLPixelFormat', 'u64'),
    ('minimum_texture_buffer_alignment_for_pixel_format', 'format', 'u64', 'minimumTextureBufferAlignmentForPixelFormat', 'MTLPixelFormat', 'u64'),
]
for name, pname, kind, selector, cast, result in queries:
    device(name, [param(pname, kind)], result, '    return [(id<MTLDevice>)object ' + selector + ':(' + cast + ')' + pname + '];', 'Caller must ensure the device and selector are available.')
    implemented.add(name)

constructors = [
    ('new_command_queue_with_max_command_buffer_count', 'count', 'u64', 'newCommandQueueWithMaxCommandBufferCount', 'NSUInteger'),
    ('new_texture', 'descriptor', 'object', 'newTextureWithDescriptor', 'MTLTextureDescriptor *'),
    ('new_sampler', 'descriptor', 'object', 'newSamplerStateWithDescriptor', 'MTLSamplerDescriptor *'),
    ('new_depth_stencil_state', 'descriptor', 'object', 'newDepthStencilStateWithDescriptor', 'MTLDepthStencilDescriptor *'),
    ('new_argument_encoder', 'arguments', 'object', 'newArgumentEncoderWithArguments', 'NSArray<MTLArgumentDescriptor *> *'),
    ('new_heap', 'descriptor', 'object', 'newHeapWithDescriptor', 'MTLHeapDescriptor *'),
    ('new_acceleration_structure_with_size', 'size', 'u64', 'newAccelerationStructureWithSize', 'NSUInteger'),
]
for name, pname, kind, selector, cast in constructors:
    device(name, [param(pname, kind)], '?*mut u8', '    return (void *)[(id<MTLDevice>)object ' + selector + ':(' + cast + ')' + pname + '];', owned)
    implemented.add(name)

fallible = [
    ('new_dynamic_library', 'library', 'newDynamicLibrary', 'id<MTLLibrary>'),
    ('new_dynamic_library_with_url', 'url', 'newDynamicLibraryWithURL', 'NSURL *'),
    ('new_binary_archive_with_descriptor', 'descriptor', 'newBinaryArchiveWithDescriptor', 'MTLBinaryArchiveDescriptor *'),
    ('new_render_pipeline_state', 'descriptor', 'newRenderPipelineStateWithDescriptor', 'MTLRenderPipelineDescriptor *'),
    ('new_compute_pipeline_state_with_function', 'function', 'newComputePipelineStateWithFunction', 'id<MTLFunction>'),
    ('new_counter_sample_buffer_with_descriptor', 'descriptor', 'newCounterSampleBufferWithDescriptor', 'MTLCounterSampleBufferDescriptor *'),
]
for name, pname, selector, cast in fallible:
    device(name, [param(pname), ERROR], '?*mut u8', '    return (void *)[(id<MTLDevice>)object ' + selector + ':(' + cast + ')' + pname + ' error:(NSError **)error];', error_note)
    implemented.add(name)

for name, selector, descriptor, reflection in [
    ('new_render_pipeline_state_with_reflection', 'newRenderPipelineStateWithDescriptor', 'MTLRenderPipelineDescriptor', 'MTLRenderPipelineReflection'),
    ('new_mesh_render_pipeline_state_with_reflection', 'newRenderPipelineStateWithMeshDescriptor', 'MTLMeshRenderPipelineDescriptor', 'MTLRenderPipelineReflection'),
    ('new_compute_pipeline_state_with_reflection', 'newComputePipelineStateWithDescriptor', 'MTLComputePipelineDescriptor', 'MTLComputePipelineReflection'),
]:
    device(name, [param('descriptor'), param('reflection_options', 'u64'), ('reflection', '*mut ?*mut u8', 'void **'), ERROR], '?*mut u8', '    ' + reflection + ' *value = nil;\n    id state = [(id<MTLDevice>)object ' + selector + ':(' + descriptor + ' *)descriptor options:(MTLPipelineOption)reflection_options reflection:&value error:(NSError **)error];\n    *reflection = (void *)[value retain];\n    return (void *)state;', error_note + ' reflection receives a separately owned +1 reflection object, or null, and must be released.')
    implemented.add(name)
for name, selector, descriptor in [
    ('new_mesh_render_pipeline_state', 'newRenderPipelineStateWithMeshDescriptor', 'MTLMeshRenderPipelineDescriptor'),
    ('new_compute_pipeline_state', 'newComputePipelineStateWithDescriptor', 'MTLComputePipelineDescriptor'),
]:
    device(name, [param('descriptor'), ERROR], '?*mut u8', '    return (void *)[(id<MTLDevice>)object ' + selector + ':(' + descriptor + ' *)descriptor options:MTLPipelineOptionNone reflection:NULL error:(NSError **)error];', error_note + ' Uses the SDK descriptor selector with no reflection options.')
    implemented.add(name)

device('new_library_with_source', [param('src', 'string'), param('options'), ERROR], '?*mut u8', '    NSString *source = [NSString stringWithUTF8String:(const char *)src];\n    return (void *)[(id<MTLDevice>)object newLibraryWithSource:source options:(MTLCompileOptions *)options error:(NSError **)error];', error_note + ' src is NUL-terminated UTF-8.')
device('new_library_with_file', [param('file', 'string'), ERROR], '?*mut u8', '    NSString *filename = [NSString stringWithUTF8String:(const char *)file];\n    return (void *)[(id<MTLDevice>)object newLibraryWithFile:filename error:(NSError **)error];', error_note + ' file is a NUL-terminated UTF-8 filesystem path.')
device('new_library_with_data', [('library_data', '?[*]const u8', 'const uint8_t *'), param('length', 'u64'), ERROR], '?*mut u8', '    dispatch_data_t data = dispatch_data_create(library_data, (size_t)length, dispatch_get_main_queue(), DISPATCH_DATA_DESTRUCTOR_DEFAULT);\n    id<MTLLibrary> library = [(id<MTLDevice>)object newLibraryWithData:data error:(NSError **)error];\n    dispatch_release(data);\n    return (void *)library;', error_note + ' library_data points to length readable bytes; dispatch copies the bytes before this returns.')
implemented.update(['new_library_with_source', 'new_library_with_file', 'new_library_with_data'])

device('new_buffer', [param('length', 'u64'), param('options', 'u64')], '?*mut u8', '    return (void *)[(id<MTLDevice>)object newBufferWithLength:(NSUInteger)length options:(MTLResourceOptions)options];', owned)
device('new_buffer_with_data', [('bytes', '?[*]const u8', 'const uint8_t *'), param('length', 'u64'), param('options', 'u64')], '?*mut u8', '    return (void *)[(id<MTLDevice>)object newBufferWithBytes:bytes length:(NSUInteger)length options:(MTLResourceOptions)options];', owned + ' bytes points to length readable bytes, copied by Metal.')
device('new_buffer_with_bytes_no_copy', [('bytes', '?[*]mut u8', 'uint8_t *'), param('length', 'u64'), param('options', 'u64'), ('deallocator', 'extern "C" fn(?*mut u8, ?[*]mut u8, u64) -> ()', 'void (*$)(void *, uint8_t *, uint64_t)'), param('context'), ('has_deallocator', 'bool', 'uint8_t')], '?*mut u8', '''    if (!has_deallocator) {
        return (void *)[(id<MTLDevice>)object newBufferWithBytesNoCopy:bytes length:(NSUInteger)length options:(MTLResourceOptions)options deallocator:nil];
    }
    return (void *)[(id<MTLDevice>)object newBufferWithBytesNoCopy:bytes length:(NSUInteger)length options:(MTLResourceOptions)options deallocator:^(void *pointer, NSUInteger size) {
        deallocator(context, (uint8_t *)pointer, (uint64_t)size);
    }];''', owned + ' bytes must satisfy Metal no-copy alignment and lifetime requirements. When has_deallocator is true, the copied block captures the callback/context values; keep both valid until Metal invokes the callback, potentially on another thread. The callback receives context, bytes, and length. When false, deallocator is not called and the caller retains responsibility for storage lifetime.')
device('new_indirect_command_buffer_with_descriptor', [param('descriptor'), param('max_command_count', 'u64'), param('options', 'u64')], '?*mut u8', '    return (void *)[(id<MTLDevice>)object newIndirectCommandBufferWithDescriptor:(MTLIndirectCommandBufferDescriptor *)descriptor maxCommandCount:(NSUInteger)max_command_count options:(MTLResourceOptions)options];', owned)
implemented.update(['new_buffer', 'new_buffer_with_data', 'new_buffer_with_bytes_no_copy', 'new_indirect_command_buffer_with_descriptor'])

aggregates = [
    ('heap_buffer_size_and_align', [param('length', 'u64'), param('options', 'u64')], 'MTLSizeAndAlign', 'heapBufferSizeAndAlignWithLength:(NSUInteger)length options:(MTLResourceOptions)options'),
    ('heap_acceleration_structure_size_and_align_with_size', [param('size', 'u64')], 'MTLSizeAndAlign', 'heapAccelerationStructureSizeAndAlignWithSize:(NSUInteger)size'),
    ('heap_texture_size_and_align', [param('descriptor')], 'MTLSizeAndAlign', 'heapTextureSizeAndAlignWithDescriptor:(MTLTextureDescriptor *)descriptor'),
    ('acceleration_structure_sizes_with_descriptor', [param('desc')], 'MTLAccelerationStructureSizes', 'accelerationStructureSizesWithDescriptor:(MTLAccelerationStructureDescriptor *)desc'),
]
for name, params, record, message in aggregates:
    device(name, params, record, '    return [(id<MTLDevice>)object ' + message + '];', 'Returns the aggregate by value; caller must ensure API availability and argument validity.')
    implemented.add(name)
device('sample_timestamps', [param('cpu_timestamp', 'out_u64'), param('gpu_timestamp', 'out_u64')], '()', '    [(id<MTLDevice>)object sampleTimestamps:(MTLTimestamp *)cpu_timestamp gpuTimestamp:(MTLTimestamp *)gpu_timestamp];', 'Writes the paired CPU and GPU timestamps to the supplied outputs.')
implemented.add('sample_timestamps')

expected = {name for name, _, _, public in device_methods if public} | {'system_default', 'all'}
if implemented != expected:
    raise ValueError('Device generation inventory mismatch: missing=' + str(expected - implemented) + ', extra=' + str(implemented - expected))
for namespace, methods in wrappers.items():
    silk.append('/// Unsafe raw bindings. Object ownership is explicit; no handles are automatically released.\npub struct ' + namespace + ' {}\n\nimpl ' + namespace + ' {\n' + '\n\n'.join(methods) + '\n}\n')
silk.append('\n'.join(declarations) + '\n')
for relative, content in [('src/device.silk', '\n'.join(silk)), ('native/device.m', '\n'.join(native)), ('api/device.json', json.dumps(api, indent=2) + '\n')]:
    destination = ROOT / relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(content)
print('Generated', len(api), 'public device/capability methods.')
