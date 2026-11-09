# gpu-metal

Metal for Silk, for programs that talk to Apple GPUs directly.

A low-level Silk port of [gfx-rs/metal-rs](https://github.com/gfx-rs/metal-rs), pinned to [`e47bc888d3837f85ea8b01c9e66ccf73236293b9`](https://github.com/gfx-rs/metal-rs/tree/e47bc888d3837f85ea8b01c9e66ccf73236293b9).

The port maps all 1,216 explicitly declared upstream public methods, including historical feature-set capabilities, Metal ray tracing, Metal Performance Shaders, capture, and upstream private-feature methods. It supplies 703 numeric constants and 21 C-layout records. The inventory is in `api/`; it measures declaration coverage, not exhaustive runtime validation.

Silk calls a compiled Objective-C bridge. There is **no Rust dependency**. The bridge handles Objective-C messaging and blocks; C-layout records now cross Silk's C ABI directly by value. The public interface is deliberately unsafe: object types, pointer extents, API availability, encoder state, resource synchronization, and callback lifetimes remain caller obligations.

## Build and test

From `native/gpu/metal`, with:

- Apple Silicon macOS with a Metal device.
- Xcode/macOS SDK 15.4 or newer for the complete declaration surface.
- Silk `0.1.0-dev.61+91f335268900`, as pinned by the workspace.
- LLVM 22 clang (`SILK_CLANG`). The build compiles the Objective-C bridge in `native/` with it,
  as `[native].sources` in `silk.toml`.
- Python 3.9+ only for inventory checks or regeneration.

```sh
silk build                                               # bridge and Silk static library
silk check                                               # type-check every exported Silk module
silk run --manifest-path examples/compute/silk.toml          # GPU compute example
silk run --manifest-path examples/headless-render/silk.toml  # GPU render example
python3 tools/coverage.py                                # check upstream declaration inventory
```

Add `--release` to build or run in release mode. The bridge's objects are cached and recompile
only when a `.m` file or `types.h` changes. On other hosts the package builds without its bridge,
and the two examples build as programs that report that they need macOS and Metal, so
`silk build --workspace` covers them everywhere.

The bridge targets macOS 14.0 (the `-mmacosx-version-min` flag in `silk.toml`). A deployment target does not make every newer Metal selector available; query device support and, where necessary, `Native.responds_to` before invoking an optional API. Private selectors are not guaranteed to exist on any OS version.

The two examples check:

- `examples/compute`: compiles a shader, transforms 257 integers, blits and checks every result, verifies a completion callback, round-trips a UTF-8 label, and checks malformed-source/missing-function errors.
- `examples/headless-render`: compiles vertex/fragment shaders, renders a triangle into a 32×32 texture, and checks the clear pixels, fragment color, coverage, and stored alpha after readback. It exercises by-value viewport/scissor inputs, nested `MTLRegion` arguments, and a four-double `MTLClearColor` argument/return. No window server, PNG dependency, or precompiled metallib is required.

These are executable examples, not ports of every upstream windowing/demo application. The binding API includes the surface needed by those applications; Cocoa event loops and window creation belong to their host application.

Verified on Apple M1 Max with macOS SDK 15.4 and the pinned Silk toolchain: all nine exported modules type-check, the native bridge and static library build, the 1,216-method inventory audit passes, and both GPU examples pass in debug and release modes. Ray-tracing, MPS, capture, window presentation, and private selectors are compiled but not exhaustively exercised by these examples.

The dev.17 migration also passed a temporary debug/release ABI smoke harness: direct SDK queries matched `MTLSize`, `MTLSizeAndAlign`, and `MTLResourceID` results; `CGSize` passed and returned both double fields; an `NSRange` GPU fill changed exactly the requested bytes without consuming the input value.

## Example

```silk
import metal.device { Device }
import metal.resources { Buffer }
import metal.runtime { Object, AutoreleasePool }
import metal.types { MTLResourceOptions_StorageModeShared }

pub fn main() -> i32 {
  unsafe {
    let pool = AutoreleasePool.new()
    let device = Object.adopt(Device.system_default())
    if Object.is_null(&device) { return 1 }
    let buffer = Object.adopt(Device.new_buffer(
      Object.get(&device), 4096, MTLResourceOptions_StorageModeShared
    ))
    if Object.is_null(&buffer) { return 2 }
    if Buffer.length(Object.get(&buffer)) != 4096 { return 3 }
  }
  return 0
}
```

A program that depends on this package needs no `[native]` section of its own: the bridge's
objects and the Foundation, Metal, QuartzCore and MetalPerformanceShaders frameworks reach it
through the dependency.

## Modules

| Import | Surface |
| --- | --- |
| `metal.runtime` | RAII `Object`, `AutoreleasePool`, retain/release, NSString/NSData/NSError access, selector availability |
| `metal.types` | Numeric enum/flag constants, C-layout records, value constructors |
| `metal.core` | Queues, command buffers, drawables/layers, arrays, URLs, events/fences, capture |
| `metal.device` | Device creation and queries, resource/pipeline factories, `FeatureSetCapabilities` |
| `metal.resources` | Buffers, textures, heaps, sampler/depth/stencil/vertex descriptors |
| `metal.encoder` | Command encoders, argument encoders, indirect command buffers/commands |
| `metal.pipeline` | Compute/render/mesh pipeline descriptors and states, reflection, render/compute/blit/acceleration passes |
| `metal.library` | Shader libraries/functions/constants, binary archives, argument reflection, counters |
| `metal.raytracing` | Acceleration structures, function tables, acceleration encoders, MPS |

Methods preserve upstream snake_case names. Namespace calls replace Rust receiver methods: `Buffer.length(raw)` instead of `buffer.length()`. Inherited operations use the base namespace: `Resource.set_label(buffer, ...)`, `CommandEncoder.end_encoding(encoder)`, or `RenderPassAttachmentDescriptor.set_texture(color_attachment, texture)`.

## ABI and ownership

| Rust surface | Silk surface |
| --- | --- |
| Objective-C owned/borrowed wrappers | `?*mut u8`; ownership documented at each operation |
| Owned result (`new`, `copy`) | +1 handle: release it or wrap it with `Object.adopt` |
| Borrowed result | +0 handle: keep its owner/autorelease pool alive, or use `Object.retain` |
| `Option<&Object>` | Nullable handle; nil retains its native meaning |
| `Result<T, String>` | Native result plus nullable `error: ?*mut ?*mut u8`; inspect NSError via `Native` |
| `&str` / paths | NUL-terminated UTF-8 pointer; returned text is borrowed |
| `Vec<Object>` / object arrays | NSArray handle, or an explicitly documented pointer/count form |
| Input slices | Raw pointer and element count; parallel arrays must have matching extents |
| Enum / bitflags | Underlying integer and module constants such as `MTLPixelFormat_RGBA8Unorm` |
| C record argument/result | Copy record passed/returned directly by value, using the target C ABI |
| Closure/block | Non-suspending C callback plus context pointer; the caller owns context lifetime |

`Object` is move-only, releases its retain when dropped, and can explicitly clone a retain. It does not dynamically validate the Metal object type. Nil is allowed. `Object.into_raw(&mut owner)` transfers ownership and clears the owner.

Create and drain autorelease pools on the same thread, in last-in-first-out order. Do not move an active pool between threads. Declare the pool before objects whose borrowed results use it. Queue command buffers and command encoders are **borrowed**, despite upstream names such as `new_command_buffer` and `new_compute_command_encoder`; use `Object.retain`, not `Object.adopt`, to own them.

String getters expose NSString's UTF-8 storage without the upstream UTF-16-length slicing bug. C-string APIs cannot carry embedded NULs; `Native.string_new(bytes, length)` supports arbitrary UTF-8 bytes for raw NSString interoperability. Returned NSString/NSData/NSError storage remains owned by its object, not by the pointer.

Callback context must remain valid until Metal finishes using it, including asynchronous notification and buffer deallocation. Callbacks can run on Metal/dispatch threads. Do not suspend, unwind, or perform unsynchronized access to shared storage. An unretained-reference command buffer additionally requires every referenced resource to outlive GPU execution.

[Migrating from the dev.14 port](docs/migrating-from-dev14.md) covers the record-by-value ABI change.

## Deliberate corrections and adaptations

- `VertexAmplificationViewMapping` uses SDK field order: viewport offset first, render-target offset second. The upstream Rust declaration reverses them. Native compile-time assertions check record sizes, alignments, and field offsets.
- Raster-order-group and mesh/object-threadgroup getters use the actual SDK getter selectors rather than upstream spellings that would send an unrecognized message.
- `VisibleFunctionTable.set_functions` takes **function handles**, not shader functions, plus a count, and invokes `setFunctions:withRange:`. Obtain handles from the relevant pipeline state. Upstream sends a nonexistent selector and cannot convert functions to pipeline-specific handles itself.
- `Buffer.gpu_resource_id` is retained as an explicitly non-SDK selector. Check `Native.responds_to` before calling it; use `Buffer.gpu_address` for public buffer addressing. No fabricated resource ID is returned.
- Constructors whose SDK factory is autoreleased retain their result so the Silk constructor consistently returns +1. Encoding factories documented as borrowed remain +0.
- `CaptureDescriptor.set_output_url` uses a filesystem URL, preserving spaces and other path characters correctly.
- `FeatureSetCapabilities` preserves the historical tables; it is not a live-device limit query. Inputs must be defined `MTLFeatureSet_*` constants. Optional numeric results use boolean presence plus an output pointer.
- The MPS acceleration-structure namespace is `MPSAccelerationStructure`, avoiding a collision with Metal's `AccelerationStructure`. The MPS intersection record with two float coordinates preserves upstream's array layout rather than imposing SIMD alignment.

## Maintaining the port

`api/upstream.json` records the independently extracted method inventory and upstream revision. Per-module JSON maps upstream declarations to Silk declarations and records adaptations. `tools/coverage.py` checks that all inventory methods have mappings and their target declarations exist; it does not prove ABI or GPU behavior.

For the generated slices:

```sh
git clone https://github.com/gfx-rs/metal-rs.git /path/to/metal-rs
git -C /path/to/metal-rs checkout e47bc888d3837f85ea8b01c9e66ccf73236293b9
python3 tools/generate.py /path/to/metal-rs
python3 tools/coverage.py --upstream /path/to/metal-rs
silk check && silk run --manifest-path examples/compute/silk.toml && silk run --manifest-path examples/headless-render/silk.toml
```

Edit `tools/core.py`, `device.py`, `encoder.py`, `library.py`, or `pipeline.py` for generated code. The resource, raytracing, runtime, and value-type files are maintained directly. Generated files are checked in, so normal builds require neither Python nor the upstream checkout.

## License

MIT OR Apache-2.0, following metal-rs. Original GFX contributor attribution is retained in the source; both upstream license texts are included. This is a Silk port, not the upstream Rust project.
