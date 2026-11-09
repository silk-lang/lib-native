# SPDX-License-Identifier: MIT OR Apache-2.0
# Port of metal-rs (Copyright GFX developers).
"""Generate the pipeline slice from the pinned, locally fetched metal-rs source.

This generator returns files to the integration driver; it performs no writes.
"""
from pathlib import Path
import re
import json

SOURCE = None  # Set by tools/generate.py to the pinned checkout's src directory.
SOURCES = ['pipeline/mod.rs', 'pipeline/render.rs', 'pipeline/compute.rs', 'renderpass.rs', 'computepass.rs', 'blitpass.rs', 'acceleration_structure_pass.rs']
RECORDS = {'MTLClearColor', 'MTLSize'}
PROTOCOLS = {'ComputePipelineState', 'RenderPipelineState'}
HEADER = '// SPDX-License-Identifier: MIT OR Apache-2.0\n// Port of metal-rs; Copyright 2016-2026 GFX developers.\n'


def block_end(text, start):
    depth = 1
    pos = start + 1
    while depth:
        if text[pos] == '{':
            depth += 1
        elif text[pos] == '}':
            depth -= 1
        pos += 1
    return pos


def rust_methods():
    for source in SOURCES:
        text = (SOURCE / source).read_text()
        for implementation in re.finditer(r'impl (\w+)\s*\{', text):
            owner = implementation[1]
            if owner.startswith('MTL'):
                continue  # ValueTypes owns record helpers and scalar defaults.
            start = implementation.end() - 1
            content = text[start + 1:block_end(text, start) - 1]
            for method in re.finditer(r'pub\s+(?:unsafe\s+)?fn\s+(\w+)(?:<[^>]*>)?\s*\((.*?)\)\s*(?:->\s*([^\{]+))?\s*\{', content, re.S):
                body_start = method.end() - 1
                body = content[body_start + 1:block_end(content, body_start) - 1]
                params = []
                for parameter in method[2].split(','):
                    parameter = parameter.strip()
                    if parameter == '&self':
                        params.append(('object', 'object'))
                    elif parameter:
                        name, kind = parameter.split(':', 1)
                        params.append((name.strip(), kind.strip()))
                yield owner, method[1], params, (method[3] or '()').strip(), body


def kind(rust):
    if rust == '()':
        return 'void'
    if rust == '&str':
        return 'string'
    if rust in RECORDS:
        return rust
    if rust.startswith('&['):
        return 'slice'
    if rust in ('bool', 'u32', 'u64', 'f64'):
        return rust
    if rust == 'MTLMultisampleStencilResolveFilter' or rust == 'MTLDispatchType':
        return 'u32'
    if rust == 'NSUInteger' or rust.startswith('MTL'):
        return 'u64'
    if rust in ('object', 'Self', 'IntersectionFunctionTable', 'VisibleFunctionTable') or rust.startswith(('&', 'Option<', 'Vec<', '*mut')):
        return 'object'
    raise ValueError('Unknown Rust type: ' + rust)


SILK = {'void': '()', 'object': '?*mut u8', 'string': '?[*]const u8', 'bool': 'bool', 'u32': 'u32', 'u64': 'u64', 'f64': 'f64'}
C = {'void': 'void', 'object': 'void *', 'string': 'const char *', 'bool': 'uint8_t', 'u32': 'uint32_t', 'u64': 'uint64_t', 'f64': 'double'}


def generate():
    silk = [HEADER, '// Unsafe raw Metal bindings. Callers provide valid live objects and SDK availability.\n// All new/copy results are +1; release them through the runtime. Other objects,\n// arrays and UTF-8 strings are borrowed (+0), unless a method states otherwise.\n// Descriptor subclasses also accept the RenderPassAttachmentDescriptor operations.\n// Slice pointers must address count initialized, non-null Objective-C object handles.\n', 'import types { MTLClearColor, MTLSize }\n\n']
    native = [HEADER, '#import <Foundation/Foundation.h>\n#import <Metal/Metal.h>\n#import <objc/message.h>\n#include <stdint.h>\n#include "types.h"\n\n']
    declarations = []
    classes = {}
    coverage = []
    for owner, name, parameters, returns, body in rust_methods():
        cls = owner.removesuffix('Ref')
        symbol = f'metal_{cls}_{name}'
        result_kind = kind(returns)
        params = []
        for arg, rust in parameters:
            k = kind(rust)
            if k == 'slice':
                params.extend([(arg, '?[*]const ?*mut u8', 'void * const *', 'slice'), (arg + '_count', 'u64', 'uint64_t', 'u64')])
            elif k in RECORDS:
                params.append((arg, k, k, k))
            else:
                params.append((arg, SILK[k], C[k], k))
        if result_kind in RECORDS:
            silk_return = result_kind
            c_return = result_kind
        else:
            silk_return = SILK[result_kind]
            c_return = C[result_kind]
        if cls == 'RenderPipelineDescriptor' and name == 'serialize_vertex_data':
            params.append(('error', '?*mut ?*mut u8', 'void **', 'error'))
        silk_params = ', '.join(f'{arg}: {st}' for arg, st, ct, k in params)
        extern_params = ', '.join(f'{arg}: {"u8" if k == "bool" else st}' for arg, st, ct, k in params)
        c_params = ', '.join(f'{ct} {arg}' for arg, st, ct, k in params) or 'void'
        abi_return = 'u8' if result_kind == 'bool' else silk_return
        declarations.append(f'unsafe extern "C" fn {symbol}({extern_params}) -> {abi_return}\n')
        silk_lines = []
        notes = []
        if name == 'new' or name.startswith('new_'):
            notes.append('Returns +1 owned object; caller must release.')
        elif result_kind == 'object':
            if returns.startswith('Vec<'):
                notes.append('Returns borrowed +0 NSArray handle rather than a Rust Vec.')
            else:
                notes.append('Returns borrowed +0 object handle.')
        elif result_kind == 'string':
            notes.append('Returns borrowed NUL-terminated UTF-8; owner must remain alive and unchanged.')
        if any(k == 'slice' for _, _, _, k in params):
            notes.append('Slice input uses pointer/count; elements are borrowed for this call.')
        if any(k in RECORDS for _, _, _, k in params):
            notes.append('Aggregate inputs are passed by value.')
        if result_kind in RECORDS:
            notes.append('Returns the aggregate result by value.')
        if cls == 'RenderPipelineReflection' and name == 'new':
            notes.append('Private upstream API: caller must establish selector availability and argument validity; null indicates initialization failure.')
        if name == 'serialize_vertex_data':
            notes = ['Private upstream API, flags fixed to zero as upstream; returns +1 serialized data. Optional error receives borrowed +0 NSError.']
        elif name == 'serialize_fragment_data':
            notes.append('Private upstream API: caller must establish selector availability.')
        for note in notes:
            silk_lines.append('  // ' + note + '\n')
        silk_lines.append(f'  pub unsafe fn {name}({silk_params}) -> {silk_return} {{\n')
        call_args = []
        for arg, st, ct, k in params:
            if k == 'bool':
                silk_lines.append(f'    let mut {arg}_abi: u8 = 0\n    if {arg} {{\n      {arg}_abi = 1\n    }}\n')
                call_args.append(arg + '_abi')
            else:
                call_args.append(arg)
        call = f'unsafe {symbol}({", ".join(call_args)})'
        if result_kind == 'bool':
            silk_lines.append(f'    return {call} != 0\n')
        elif silk_return == '()':
            silk_lines.append(f'    {call}\n')
        else:
            silk_lines.append(f'    return {call}\n')
        silk_lines.append('  }\n')
        classes.setdefault(cls, []).append(''.join(silk_lines))
        recv = f'((id<MTL{cls}>)object)' if cls in PROTOCOLS else f'((MTL{cls} *)object)'
        if cls == 'ArgumentArray':
            recv = '((NSArray *)object)'
        c_lines = []
        if cls == 'RenderPipelineReflection' and name == 'new':
            expr = '((id (*)(id, SEL, void *, void *, void *, id, uint64_t, uint64_t))objc_msgSend)([MTLRenderPipelineReflection alloc], sel_registerName("initWithVertexData:fragmentData:serializedVertexDescriptor:device:options:flags:"), vertex_data, fragment_data, vertex_desc, (id)device, options, flags)'
        elif name == 'serialize_vertex_data':
            expr = '((id (*)(id, SEL, uint64_t, NSError **))objc_msgSend)((id)object, sel_registerName("newSerializedVertexDataWithFlags:error:"), 0, (NSError **)error)'
        elif name == 'serialize_fragment_data':
            expr = '((id (*)(id, SEL))objc_msgSend)((id)object, sel_registerName("serializeFragmentData"))'
        elif name == 'new':
            factory = re.search(r'msg_send!\[\s*(?:class|class!\([^)]*\))\s*,\s*(\w+)\s*\]', body)
            if not factory:
                raise ValueError('Unrecognized factory ' + cls)
            selector = factory[1]
            expr = f'[MTL{cls} {selector}]'
            if selector != 'new':
                expr = '[' + expr + ' retain]'
        elif name == 'label':
            expr = f'[[{recv} label] UTF8String]'
        elif name == 'set_label':
            c_lines.append('    NSString *text = label ? [[NSString alloc] initWithUTF8String:label] : nil;\n')
            expr = f'[{recv} setLabel:text]'
        elif any(k == 'slice' for _, _, _, k in params):
            arg = next(arg for arg, _, _, k in params if k == 'slice')
            selector = re.search(r'msg_send!\[\s*self,\s*(\w+)\s*:', body)[1]
            c_lines.append(f'    NSArray *items = [[NSArray alloc] initWithObjects:(id const *){arg} count:(NSUInteger){arg}_count];\n')
            expr = f'[{recv} {selector}:items]'
        else:
            messages = re.findall(r'msg_send(?:_bool)?!\[\s*self\s*,(.*?)\]', body, re.S)
            if not messages:
                raise ValueError('No selector: ' + cls + '.' + name)
            message = messages[0].strip()
            message = {
                'isMeshThreadgroupSizeIsMultipleOfThreadExecutionWidth': 'meshThreadgroupSizeIsMultipleOfThreadExecutionWidth',
                'isObjectThreadgroupSizeIsMultipleOfThreadExecutionWidth': 'objectThreadgroupSizeIsMultipleOfThreadExecutionWidth',
            }.get(message, message)
            if ':' not in message:
                expr = f'[{recv} {message}]'
            else:
                pieces = re.findall(r'(\w+)\s*:\s*(\w+)', message)
                if not pieces:
                    raise ValueError('No selector pieces: ' + message)
                param_kinds = {arg: k for arg, _, _, k in params}
                arguments = []
                for selector, arg in pieces:
                    k = param_kinds[arg]
                    if k == 'object':
                        value = '(id)' + arg
                    elif k == 'bool':
                        value = '(' + arg + ' != 0)'
                    else:
                        value = arg
                    arguments.append(selector + ':' + value)
                expr = f'[{recv} {" ".join(arguments)}]'
        if result_kind in RECORDS:
            c_lines.append(f'    return {expr};\n')
        elif result_kind == 'void':
            c_lines.append(f'    {expr};\n')
        elif result_kind == 'bool':
            c_lines.append(f'    return {expr} ? 1 : 0;\n')
        else:
            c_lines.append(f'    return ({c_return}){expr};\n')
        if name == 'set_label':
            c_lines.append('    [text release];\n')
        if any(k == 'slice' for _, _, _, k in params):
            c_lines.append('    [items release];\n')
        native.append(f'{c_return} {symbol}({c_params}) {{\n' + ''.join(c_lines) + '}\n\n')
        coverage.append({'rust': f'{owner}.{name}', 'silk': f'pipeline.{cls}.{name}', 'notes': ' '.join(notes) or 'Direct Metal selector binding; caller establishes object validity and API availability.'})
    silk.extend(declarations)
    silk.append('\n')
    for cls, methods in classes.items():
        silk.append(f'pub struct {cls} {{}}\nimpl {cls} {{\n' + '\n'.join(methods) + '}\n\n')
    return {'src/pipeline.silk': ''.join(silk), 'native/pipeline.m': ''.join(native), 'api/pipeline.json': json.dumps(coverage, indent=2) + '\n'}
