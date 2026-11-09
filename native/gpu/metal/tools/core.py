#!/usr/bin/env python3
"""Generate the mechanical core Metal bridge. SPDX-License-Identifier: MIT OR Apache-2.0."""
from pathlib import Path
import json
ROOT = Path(__file__).resolve().parent.parent
methods = []
T = {'obj': ('?*mut u8','void *'), 'text': ('?[*]const u8','const char *'), 'u64': ('u64','uint64_t'), 'u32': ('u32','uint32_t'), 'f64': ('f64','double'), 'bool': ('bool','uint8_t'), 'void': ('()','void'), 'error': ('?*mut ?*mut u8','void **'), 'size': ('CGSize','CGSize'), 'objects': ('?[*]const ?*mut u8','void *const *'), 'callback': ('extern "C" fn(?*mut u8, ?*mut u8) -> ()','callback'), 'eventcb': ('extern "C" fn(?*mut u8, ?*mut u8, u64) -> ()','eventcb')}
classes = {'CommandQueue':'id<MTLCommandQueue>','CommandBuffer':'id<MTLCommandBuffer>','Drawable':'id<MTLDrawable>','MetalDrawable':'id<CAMetalDrawable>','MetalLayer':'CAMetalLayer *','Event':'id<MTLEvent>','SharedEvent':'id<MTLSharedEvent>','SharedEventListener':'MTLSharedEventListener *','Fence':'id<MTLFence>','CaptureScope':'id<MTLCaptureScope>','CaptureManager':'MTLCaptureManager *','CaptureDescriptor':'MTLCaptureDescriptor *','URL':'NSURL *','Array':'NSArray *','NsObject':'id'}
def add(cls, name, ret='void', args=(), expr=None, body=None, static=False, notes=''):
    pars = ([] if static else [('object','obj')]) + list(args)
    if body is None:
        recv = '(' + classes[cls] + ')object'
        call = '['+recv+' '+(expr or name)+']'
        if ret=='text': call='['+call+' UTF8String]'
        body = ('return '+call+';' if ret!='void' else call+';')
    methods.append((cls,name,ret,pars,body,notes))
def prop(cls,name,sel,typ,setter=None):
    add(cls,name,typ,expr=sel)
    if setter:
        val = '[NSString stringWithUTF8String:value]' if typ=='text' else '(id)value' if typ=='obj' else 'value'
        add(cls,'set_'+name,args=[('value',typ)],expr=setter+':'+val)
for cls in ['CommandQueue','CommandBuffer','Fence']:
    prop(cls,'label','label','text','setLabel')
for cls in ['CommandQueue','Event','Fence','MetalLayer']:
    prop(cls,'device','device','obj','setDevice' if cls=='MetalLayer' else None)
for name,sel in [('new_command_buffer','commandBuffer'),('new_command_buffer_with_unretained_references','commandBufferWithUnretainedReferences')]:
    add('CommandQueue',name,'obj',expr=sel,notes='Borrowed/autoreleased result, despite new_ name.')
for name,sel in [('enqueue','enqueue'),('commit','commit'),('wait_until_completed','waitUntilCompleted'),('wait_until_scheduled','waitUntilScheduled'),('pop_debug_group','popDebugGroup')]: add('CommandBuffer',name,expr=sel)
add('CommandBuffer','status','u32')
add('CommandBuffer','present_drawable',args=[('drawable','obj')],expr='presentDrawable:(id<MTLDrawable>)drawable')
add('CommandBuffer','push_debug_group',args=[('name','text')],expr='pushDebugGroup:[NSString stringWithUTF8String:name]')
for name,sel in [('new_blit_command_encoder','blitCommandEncoder'),('new_compute_command_encoder','computeCommandEncoder'),('new_acceleration_structure_command_encoder','accelerationStructureCommandEncoder')]: add('CommandBuffer',name,'obj',expr=sel,notes='Borrowed/autoreleased encoder.')
for name,sel in [('blit_command_encoder_with_descriptor','blitCommandEncoderWithDescriptor'),('compute_command_encoder_with_descriptor','computeCommandEncoderWithDescriptor'),('new_render_command_encoder','renderCommandEncoderWithDescriptor'),('new_parallel_render_command_encoder','parallelRenderCommandEncoderWithDescriptor'),('acceleration_structure_command_encoder_with_descriptor','accelerationStructureCommandEncoderWithDescriptor')]: add('CommandBuffer',name,'obj',[('descriptor','obj')],sel+':(id)descriptor',notes='Borrowed/autoreleased encoder.')
add('CommandBuffer','compute_command_encoder_with_dispatch_type','obj',[('ty','u32')],'computeCommandEncoderWithDispatchType:(MTLDispatchType)ty')
for name,sel in [('encode_signal_event','encodeSignalEvent'),('encode_wait_for_event','encodeWaitForEvent')]: add('CommandBuffer',name,args=[('event','obj'),('value','u64')],expr=sel+':(id<MTLEvent>)event value:value')
for cls,name,sel,proto in [('CommandBuffer','add_completed_handler','addCompletedHandler','MTLCommandBuffer'),('CommandBuffer','add_scheduled_handler','addScheduledHandler','MTLCommandBuffer'),('Drawable','add_presented_handler','addPresentedHandler','MTLDrawable')]:
    add(cls,name,args=[('callback','callback'),('context','obj')],body=f'[(id<{proto}>)object {sel}:^(id<{proto}> value) {{ callback(context, value); }}];',notes='Callback/context must remain valid until invocation. Called by Metal on an arbitrary thread; do not unwind or suspend.')
add('Drawable','present')
add('Drawable','drawable_id','u64',expr='drawableID')
add('Drawable','presented_time','f64',expr='presentedTime')
add('MetalDrawable','texture','obj')
add('MetalLayer','new','obj',body='return [CAMetalLayer new];',static=True,notes='Owned +1 result.')
for name,sel,typ,setter in [('pixel_format','pixelFormat','u64','setPixelFormat'),('presents_with_transaction','presentsWithTransaction','bool','setPresentsWithTransaction'),('display_sync_enabled','displaySyncEnabled','bool','setDisplaySyncEnabled'),('maximum_drawable_count','maximumDrawableCount','u64','setMaximumDrawableCount'),('contents_scale','contentsScale','f64','setContentsScale'),('framebuffer_only','framebufferOnly','bool','setFramebufferOnly'),('wants_extended_dynamic_range_content','wantsExtendedDynamicRangeContent','bool','setWantsExtendedDynamicRangeContent')]: prop('MetalLayer',name,sel,typ,setter)
add('MetalLayer','is_opaque','bool',expr='isOpaque')
add('MetalLayer','set_opaque',args=[('value','bool')],expr='setOpaque:value')
add('MetalLayer','drawable_size','size',expr='drawableSize',notes='Returns the drawable dimensions by value.')
add('MetalLayer','set_drawable_size',args=[('value','size')],expr='setDrawableSize:value',notes='Copies the drawable dimensions by value.')
add('MetalLayer','set_edge_antialiasing_mask',args=[('mask','u64')],expr='setEdgeAntialiasingMask:(CAEdgeAntialiasingMask)mask')
add('MetalLayer','set_masks_to_bounds',args=[('masks','bool')],expr='setMasksToBounds:masks')
add('MetalLayer','remove_all_animations',expr='removeAllAnimations')
add('MetalLayer','next_drawable','obj',expr='nextDrawable',notes='Nullable borrowed drawable; nil is a normal timeout/unavailable result.')
prop('SharedEvent','signaled_value','signaledValue','u64','setSignaledValue')
add('SharedEvent','notify',args=[('listener','obj'),('value','u64'),('callback','eventcb'),('context','obj')],body='[(id<MTLSharedEvent>)object notifyListener:(MTLSharedEventListener *)listener atValue:value block:^(id<MTLSharedEvent> event, uint64_t reached) { callback(context, event, reached); }];',notes='Context must outlive notification. Callback runs on listener queue; no unwind/suspension.')
for name in ['from_queue_handle','from_queue']:
    add('SharedEventListener',name,'obj',[('queue','obj')],body='return [[MTLSharedEventListener alloc] initWithDispatchQueue:(dispatch_queue_t)queue];',static=True,notes='Owned +1 result; queue is a dispatch_queue_t.')
for name,sel in [('begin_scope','beginScope'),('end_scope','endScope')]: add('CaptureScope',name,expr=sel)
prop('CaptureScope','label','label','text')
add('CaptureManager','shared','obj',body='return [MTLCaptureManager sharedCaptureManager];',static=True)
for name,sel,param in [('new_capture_scope_with_device','newCaptureScopeWithDevice','device'),('new_capture_scope_with_command_queue','newCaptureScopeWithCommandQueue','command_queue')]: add('CaptureManager',name,'obj',[(param,'obj')],sel+':(id)'+param,notes='Owned +1 scope.')
prop('CaptureManager','default_capture_scope','defaultCaptureScope','obj','setDefaultCaptureScope')
add('CaptureManager','start_capture','bool',[('descriptor','obj'),('error','error')],body='return [(MTLCaptureManager *)object startCaptureWithDescriptor:(MTLCaptureDescriptor *)descriptor error:(NSError **)error];',notes='NSError output is borrowed. Metal capture must be enabled before starting.')
for name,sel,param in [('start_capture_with_device','startCaptureWithDevice','device'),('start_capture_with_command_queue','startCaptureWithCommandQueue','command_queue'),('start_capture_with_scope','startCaptureWithScope','scope')]: add('CaptureManager',name,args=[(param,'obj')],expr=sel+':(id)'+param)
add('CaptureManager','stop_capture',expr='stopCapture')
add('CaptureManager','is_capturing','bool',expr='isCapturing')
add('CaptureManager','supports_destination','bool',[('destination','u64')],'supportsDestination:(MTLCaptureDestination)destination')
add('CaptureDescriptor','new','obj',body='return [MTLCaptureDescriptor new];',static=True,notes='Owned +1 descriptor.')
for name in ['device','scope','command_queue']: add('CaptureDescriptor','set_capture_'+name,args=[('value','obj')],expr='setCaptureObject:(id)value')
add('CaptureDescriptor','output_url','text',body='return [[[(MTLCaptureDescriptor *)object outputURL] path] UTF8String];')
add('CaptureDescriptor','set_output_url',args=[('path','text')],expr='setOutputURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:path]]')
prop('CaptureDescriptor','destination','destination','u64','setDestination')
add('URL','new_with_string','obj',[('text','text')],body='return [[NSURL alloc] initWithString:[NSString stringWithUTF8String:text]];',static=True,notes='Owned +1 URL; malformed input may return nil.')
add('URL','absolute_string','text',expr='absoluteString')
add('URL','path','text')
for name in ['from_slice','from_owned_slice']:
    add('Array',name,'obj',[('objects','objects'),('count','u64')],body='return [NSArray arrayWithObjects:(id const *)objects count:count];',static=True,notes='Autoreleased array retains its elements; caller retains ownership of input objects.')
add('Array','count','u64')
add('Array','object_at','obj',[('index','u64')],'objectAtIndex:index',notes='Borrowed element; index must be less than count.')
add('NsObject','conforms_to_protocol','u32',[('name','text')],body='Protocol *protocol = objc_getProtocol(name); if (!protocol) return 2; return [(id)object conformsToProtocol:protocol] ? 1 : 0;',notes='Returns 0=false, 1=true, 2=unknown protocol (upstream error).')

header='// Copyright GFX developers. SPDX-License-Identifier: MIT OR Apache-2.0\n// Generated by tools/core.py; edit its API table instead.\n'
silk=[header,'import types { CGSize }\n']
c=[header,'#import <Foundation/Foundation.h>\n#import <Metal/Metal.h>\n#import <QuartzCore/CAMetalLayer.h>\n#import <objc/runtime.h>\n#include <stdint.h>\n']
entries=[]
for cls in dict.fromkeys(m[0] for m in methods):
    silk += [f'pub struct {cls} {{}}\n',f'impl {cls} {{']
    for cl,name,ret,pars,body,notes in [m for m in methods if m[0]==cls]:
        symbol=f'metal_{cls}_{name}'
        args=', '.join(f'{n}: {T[t][0]}' for n,t in pars)
        callargs=', '.join(n+'_byte' if t=='bool' else n for n,t in pars)
        if notes: silk.append('  /// '+notes)
        silk.append(f'  pub unsafe fn {name}({args}) -> {T[ret][0]} {{')
        for n,t in pars:
            if t=='bool':
                silk += [f'    let mut {n}_byte: u8 = 0', f'    if {n} {{ {n}_byte = 1 }}']
        silk += [f'    return unsafe {symbol}({callargs})'+(' != 0' if ret=='bool' else ''),'  }']
        cpars=[]
        for n,t in pars:
            if t=='callback': cpars.append(f'void (*{n})(void *, void *)')
            elif t=='eventcb': cpars.append(f'void (*{n})(void *, void *, uint64_t)')
            else: cpars.append(T[t][1]+' '+n)
        c.append(f'{T[ret][1]} {symbol}({", ".join(cpars) or "void"}) {{ {body} }}')
        entries.append({'rust':f'{cls}.{name}','silk':f'core.{cls}.{name}','notes':notes})
    silk += ['}\n']
for cls,name,ret,pars,body,notes in methods:
    args=', '.join(f'{n}: '+('u8' if t=='bool' else T[t][0]) for n,t in pars)
    cb=[n for n,t in pars if t in ['callback','eventcb']]
    if cb: silk.append('@callbacks("'+cb[0]+'")')
    silk.append(f'unsafe extern "C" fn metal_{cls}_{name}({args}) -> '+('u8' if ret=='bool' else T[ret][0]))
for path,text in [('src/core.silk','\n'.join(silk)+'\n'),('native/core.m','\n'.join(c)+'\n'),('api/core.json',json.dumps(entries,indent=2)+'\n')]:
    dest=ROOT/path; dest.parent.mkdir(parents=True,exist_ok=True); dest.write_text(text)
