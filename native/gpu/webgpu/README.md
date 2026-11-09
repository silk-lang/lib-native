# gpu-webgpu

WebGPU for Silk, natively through [Dawn](https://dawn.googlesource.com/dawn) and in the browser
through `navigator.gpu`, with one API for both. The bindings follow the stable
[`webgpu.h`](https://github.com/webgpu-native/webgpu-headers) schema, so the same code runs on
Vulkan, Metal and D3D and as WebAssembly in a page.

There are two layers:

- `gpu-webgpu-sys` is the raw C layer, generated from the pinned header schema. All 92 records
  are checked against Dawn's own layouts, and all 202 functions are linked.
- `gpu-webgpu` is the safe layer built on it:
  - owned handles that release on drop;
  - descriptors that borrow instead of allocating;
  - typed `GpuError` failures with error scopes;
  - asynchronous requests that can be polled or awaited;
  - mapped buffers with checked copies;
  - capability snapshots;
  - host-owned surfaces.

## Example

```silk
import webgpu.runtime { GpuError, Instance, RequestAdapterOptions }
import webgpu.descriptors { DeviceDescriptor }
import webgpu.capabilities { adapterInfo }
import webgpu_sys.generated_webgpu as sys
import silk.mem.allocator { OutOfMemoryError }

/// Opens the default adapter and a device on it, and reports whether the adapter is a CPU
/// (software) implementation.
pub effect fn openDevice() -> bool ! GpuError | OutOfMemoryError {
  let instance = run Instance.create()
  let adapter = run instance.requestAdapter(&RequestAdapterOptions.default())
  let info = run adapterInfo(&adapter)
  let software = info.adapterType() == sys.WGPUAdapterType_CPU
  let device = run adapter.requestDevice(&DeviceDescriptor.default())
  run device.checkErrors()
  return software
}
```

The blocking `requestAdapter` and `requestDevice` are native conveniences. Each has a
`…Start` form that returns a request to `poll()` and `finish()`, which is what browser code must
use. `examples/src/session.silk` drives a whole sample that way, on both targets.

Native programs link Dawn's static library and a small callback helper
(`sys/native/callbacks.c`); `sys/silk.toml` pins Dawn and declares the native link
configuration, which reaches every program that depends on the packages.
Dependents import the packages as `webgpu` and `webgpu_sys`.

## Packages

| Path | Package | What it is |
| --- | --- | --- |
| `native/gpu/webgpu` | `gpu-webgpu` | The safe API |
| `native/gpu/webgpu/sys` | `gpu-webgpu-sys` | The generated raw bindings and ABI probes |
| `native/gpu/webgpu/examples` | `gpu-webgpu-examples` | Six headless samples (clear, triangle, textured quad, instanced rectangles, compute, MSAA), native and wasm |
| `native/gpu/webgpu/examples-window` | `gpu-webgpu-examples-window` | A GLFW window sample with a real surface and resizing |
| `native/gpu/webgpu/tests/native` | `gpu-webgpu-tests-native` | Native lifetime, readback and validation scenarios |
| `native/gpu/webgpu/bench` | `gpu-webgpu-bench` | The same CPU-side workloads in Silk, C on Dawn, and Rust on wgpu |

`browser/` holds the JavaScript host for the wasm build, and `tests/pixels/` holds the samples'
expected images.

## Platforms

Dawn's pinned release (`v20261027.184652`) runs natively on macOS arm64, Linux x86_64
and Windows x86_64. Tests use Metal on macOS, Mesa lavapipe on Linux, and D3D12 on
Windows. Browsers use Chromium's WebGPU.

Windows builds require LLVM 22 and an x64 Visual Studio 2026 developer shell with
the Windows SDK and current Visual C++ Redistributable. Run
`./scripts/prepare-windows.ps1` from the repository root, then use `silkc build`
or `silkc run` directly. Preparation wraps Dawn in `nonstd_dawn.dll` so its C++
runtime initializes. `-RuntimeDirectory <executable-directory>` stages Dawn, DXC
and GLFW DLLs beside an executable. Keep those DLLs when moving programs.

## Tests

From the repository root:

```sh
silkc check --workspace
silkc run --manifest-path native/gpu/webgpu/sys/abi/silk.toml
silkc run --manifest-path native/gpu/webgpu/tests/native/silk.toml
silkc build --manifest-path native/gpu/webgpu/examples/silk.toml --target wasm32-unknown-unknown
node --test native/gpu/webgpu/browser/host.test.mjs
```

The standalone [native workflow](../../../.github/workflows/native.yml) runs direct Silk
commands, native pixel comparisons, and actual browser wasm/canvas tests. See the
[repository guide](../../../README.md#verification) for prerequisites and coverage.
Historical Python generators and specialized harnesses remain optional maintenance tools;
they are not normal build hooks or the standalone CI gate.

## Performance

On the CPU-side workloads (encoding and submitting 10,000 draws, buffer writes or bind-group
switches, or one instanced draw of 10k–100k rectangles), Silk on Dawn runs between 11% faster and
5% slower than the same program in C on the same Dawn binary (median wall time, macOS arm64). [The reference](docs/reference.md#cpu-side-benchmarks)
has the tables, the instruction-count method and the Rust/wgpu comparison.

## More

- [Reference](docs/reference.md): implementation selection, header compatibility, toolchain
  prerequisites, CI, callback ownership, surfaces, samples, the browser ABI and benchmarks.
- [LIMITATIONS.md](LIMITATIONS.md): workarounds for compiler and standard-library gaps.
- [GPU scene renderer](https://github.com/silk-lang/lib-graphics/tree/main/graphics/gpu_scene): downstream 2D vector drawing.
