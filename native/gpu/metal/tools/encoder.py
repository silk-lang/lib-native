# SPDX-License-Identifier: MIT OR Apache-2.0
# Derived from metal-rs, copyright GFX developers.
"""Render the encoder binding slice from the checked-out upstream Rust sources.

The upstream source is the selector/signature authority. This renderer deliberately
rejects unknown Rust argument forms instead of emitting incomplete bindings.
Call render(upstream_path) to obtain the three generated file contents.
"""
import json
import re
from collections import OrderedDict
from pathlib import Path

RECORDS = {
    'MTLViewport': 'MTLViewport', 'MTLScissorRect': 'MTLScissorRect',
    'VertexAmplificationViewMapping': 'MTLVertexAmplificationViewMapping',
    'MTLSize': 'MTLSize', 'MTLOrigin': 'MTLOrigin', 'MTLRegion': 'MTLRegion',
    'NSRange': 'NSRange',
}
SCALARS = {
    'NSUInteger': ('u64', 'NSUInteger'), 'NSInteger': ('i64', 'NSInteger'),
    'u32': ('u32', 'uint32_t'), 'u8': ('u8', 'uint8_t'),
    'f32': ('f32', 'float'), 'f64': ('f64', 'double'),
    'bool': ('bool', 'uint8_t'),
}


def balanced(text, start, left, right):
    depth = 0
    for index in range(start, len(text)):
        if text[index] == left:
            depth += 1
        elif text[index] == right:
            depth -= 1
            if depth == 0:
                return index
    raise ValueError('Unbalanced upstream declaration')


def methods(text):
    for block in re.finditer(r'impl (\w+)\s*\{', text):
        owner = block.group(1)
        start = text.index('{', block.start())
        end = balanced(text, start, '{', '}')
        body = text[start + 1:end]
        for method in re.finditer(r'pub fn (\w+)\s*\(', body):
            name = method.group(1)
            args_start = body.index('(', method.start())
            args_end = balanced(body, args_start, '(', ')')
            body_start = body.index('{', args_end)
            body_end = balanced(body, body_start, '{', '}')
            ret = body[args_end + 1:body_start].strip()
            yield owner, name, body[args_start + 1:args_end], ret.removeprefix('->').strip(), body[body_start + 1:body_end]


def render(upstream):
    upstream = Path(upstream)
    groups = OrderedDict()
    native = [
        '// SPDX-License-Identifier: MIT OR Apache-2.0',
        '// Derived from metal-rs, copyright GFX developers.',
        '#import <Foundation/Foundation.h>', '#import <Metal/Metal.h>',
        '#include <stdint.h>', '',
        '// Objects and array entries are borrowed unless explicitly documented +1.',
        '// The caller guarantees object validity, platform availability, and array extents.', '',
    ]
    declarations = []
    coverage = []
    records_used = set()
    for file in ('encoder.rs', 'indirect_encoder.rs'):
        for rust_owner, name, args_text, ret, body in methods((upstream / 'src' / file).read_text()):
            owner = rust_owner.removesuffix('Ref')
            params = []
            expressions = {}
            bool_names = []
            counts = []
            notes = []
            has_self = False
            args = [part.strip() for part in args_text.split(',') if part.strip()]
            for arg in args:
                if arg == '&self':
                    has_self = True
                    params.append(('object', '?*mut u8', 'void *'))
                    continue
                arg_name, typ = arg.split(':', 1)
                arg_name, typ = arg_name.strip(), typ.strip().removeprefix('crate::')
                if typ == 'Range<f32>':
                    params.extend([(arg_name + '_start', 'f32', 'float'), (arg_name + '_end', 'f32', 'float')])
                    expressions[arg_name + '.start'] = arg_name + '_start'
                    expressions[arg_name + '.end'] = arg_name + '_end'
                    notes.append(arg_name + ' is flattened into start/end scalars.')
                elif typ == 'Option<&[VertexAmplificationViewMapping]>':
                    records_used.add('VertexAmplificationViewMapping')
                    params.append((arg_name, '?[*]const VertexAmplificationViewMapping', 'const MTLVertexAmplificationViewMapping *'))
                    expressions[arg_name + '.map_or(std::ptr::null(), |vm| vm.as_ptr())'] = arg_name
                    notes.append('view_mappings is null or points to count entries.')
                elif typ.startswith('&[') and typ.endswith(']'):
                    element = typ[2:-1]
                    if element in RECORDS:
                        records_used.add(element)
                        st, ct = element, RECORDS[element]
                    elif element == 'NSUInteger':
                        st, ct = 'u64', 'NSUInteger'
                    elif element.startswith('&') or element.startswith('Option<&'):
                        st, ct = '?*mut u8', 'id'
                    else:
                        raise ValueError((owner, name, typ))
                    params.append((arg_name, '?[*]const ' + st, 'const ' + ct + ' *'))
                    expressions[arg_name + '.as_ptr()'] = arg_name
                    if arg_name == 'offsets':
                        expressions[arg_name + '.len()'] = 'data_count'
                        notes.append('offsets and data each contain data_count elements.')
                    else:
                        counts.append((arg_name + '_count', 'u64', 'NSUInteger'))
                        expressions[arg_name + '.len()'] = arg_name + '_count'
                        notes.append(arg_name + ' points to ' + arg_name + '_count entries; null is allowed only for a zero count.')
                elif typ == '&str':
                    params.append((arg_name, '?[*]const u8', 'const char *'))
                    expressions[arg_name] = '[NSString stringWithUTF8String:' + arg_name + ']'
                    notes.append(arg_name + ' is a non-null NUL-terminated UTF-8 string.')
                elif typ in RECORDS:
                    records_used.add(typ)
                    params.append((arg_name, typ, RECORDS[typ]))
                    expressions[arg_name] = arg_name
                    notes.append(arg_name + ' is passed by value.')
                elif typ in SCALARS:
                    st, ct = SCALARS[typ]
                    params.append((arg_name, st, ct))
                    expressions[arg_name] = arg_name
                    if typ == 'bool':
                        bool_names.append(arg_name)
                elif typ.startswith('MTL'):
                    params.append((arg_name, 'u64', 'NSUInteger'))
                    expressions[arg_name] = '(' + typ + ')' + arg_name
                elif typ.startswith('&') or typ.startswith('Option<&'):
                    params.append((arg_name, '?*mut u8', 'void *'))
                    expressions[arg_name] = '(id)' + arg_name
                elif typ == '*const std::ffi::c_void':
                    params.append((arg_name, '?*const u8', 'const void *'))
                    expressions[arg_name] = arg_name
                else:
                    raise ValueError((owner, name, typ))
            params += counts
            if ret == '':
                silk_ret, c_ret = '()', 'void'
            elif ret == '&str':
                silk_ret, c_ret = '?[*]const u8', 'const char *'
                notes.append('Returned UTF-8 storage is borrowed from the encoder label NSString.')
            elif ret in SCALARS:
                silk_ret, c_ret = SCALARS[ret]
            elif ret.startswith('MTL'):
                silk_ret, c_ret = 'u64', 'NSUInteger'
            elif ret == '*mut std::ffi::c_void':
                silk_ret, c_ret = '?*mut u8', 'void *'
                notes.append('Returned mutable storage is borrowed from the argument buffer.')
            elif ret == 'Self' or ret == 'ArgumentEncoder' or ret.startswith('&'):
                silk_ret, c_ret = '?*mut u8', 'void *'
                notes.append('Returned object is owned (+1); release it exactly once.' if name.startswith('new') else 'Returned object is borrowed (+0).')
            else:
                raise ValueError((owner, name, ret))
            message = re.search(r'msg_send(?:_bool)?!\s*\[', body)
            if not message:
                raise ValueError((owner, name, 'No selector'))
            msg_start = body.index('[', message.start())
            msg_end = balanced(body, msg_start, '[', ']')
            message = body[msg_start + 1:msg_end].split(',', 1)[1].strip()
            message = re.sub(r'NSRange\s*\{\s*location:\s*(\w+),\s*length:\s*(\w+)\.len\(\) as _,?\s*\}',
                             lambda m: 'NSMakeRange(' + m[1] + ', ' + m[2] + '_count)', message)
            # The three debug-string methods create one local NSString in Rust.
            if name in ('set_label', 'insert_debug_signpost', 'push_debug_group'):
                string_arg = 'label' if name == 'set_label' else 'name'
                message = message.replace('nslabel', expressions[string_arg])
            elif ':' in message:
                parts = list(re.finditer(r'\b([A-Za-z_]\w*)\s*:(?!:)', message))
                converted = []
                for index, match in enumerate(parts):
                    expr = message[match.end():parts[index + 1].start() if index + 1 < len(parts) else len(message)].strip()
                    expr = re.sub(r'\s+as\s+(?:u64|NSUInteger|_)$', '', expr)
                    expr = expressions.get(expr, expr)
                    if not (expr.startswith('NSMakeRange(') or expr in expressions.values()):
                        raise ValueError((owner, name, 'Unknown selector expression', expr))
                    converted.append(match[1] + ':' + expr)
                message = ' '.join(converted)
            target = '(id<MTL' + owner + '>)object'
            if owner == 'IndirectCommandBufferDescriptor':
                target = '(MTLIndirectCommandBufferDescriptor *)object'
            if not has_self:
                target = 'MTL' + owner
            call = '[' + target + ' ' + message + ']'
            if ret == '&str':
                call = '[' + call + ' UTF8String]'
            if silk_ret == 'bool':
                call = '(' + call + ' ? 1 : 0)'
            symbol = 'metal_' + owner + '_' + name
            c_params = ', '.join(ct + ' ' + n for n, st, ct in params) or 'void'
            native.append(c_ret + ' ' + symbol + '(' + c_params + ') {')
            native.append('    ' + ('' if c_ret == 'void' else 'return ') + call + ';')
            native.extend(['}', ''])
            raw_params = ', '.join(n + ': ' + ('u8' if st == 'bool' else st) for n, st, ct in params)
            declarations.append('unsafe extern "C" fn ' + symbol + '(' + raw_params + ') -> ' + ('u8' if silk_ret == 'bool' else silk_ret))
            wrapper = []
            if notes:
                wrapper.extend('    // ' + note for note in notes)
            wrapper.append('    pub unsafe fn ' + name + '(' + ', '.join(n + ': ' + st for n, st, ct in params) + ') -> ' + silk_ret + ' {')
            for bool_name in bool_names:
                wrapper += ['        let mut ' + bool_name + '_abi: u8 = 0', '        if ' + bool_name + ' {', '            ' + bool_name + '_abi = 1', '        }']
            actuals = ', '.join(n + ('_abi' if n in bool_names else '') for n, st, ct in params)
            invocation = 'unsafe ' + symbol + '(' + actuals + ')'
            if silk_ret == 'bool':
                invocation = '(' + invocation + ') != 0'
            wrapper.append('        ' + ('' if silk_ret == '()' else 'return ') + invocation)
            wrapper.append('    }')
            groups.setdefault(owner, []).append('\n'.join(wrapper))
            coverage.append({'rust': rust_owner + '.' + name, 'silk': 'encoder.' + owner + '.' + name, 'notes': ' '.join(notes) or 'Direct Metal selector binding; receiver and resource arguments are borrowed.'})
    silk = ['// SPDX-License-Identifier: MIT OR Apache-2.0', '// Derived from metal-rs, copyright GFX developers.',
            '// Unsafe low-level bindings: callers guarantee object validity, API availability,',
            '// encoder state, resource lifetimes, and pointer extents for the Metal operation.',
            '// Object arguments are borrowed. Inherited operations use CommandEncoder or Resource.',
            'import types { ' + ', '.join(sorted(records_used)) + ' }', '']
    for owner, wrappers in groups.items():
        silk += ['pub struct ' + owner + ' {}', '', 'impl ' + owner + ' {', '\n\n'.join(wrappers), '}', '']
    silk += declarations
    return {'src/encoder.silk': '\n'.join(silk) + '\n', 'native/encoder.m': '\n'.join(native), 'api/encoder.json': json.dumps(coverage, indent=2) + '\n'}
