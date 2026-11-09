# lib-native

Standalone native GPU and window libraries for [Silk](https://github.com/silk-lang/silk).
Extracted from `silk-lang/nonstd` commit
`7d6c3693a9ad1fec24127985626ab4224f9f1995`; package names and source layout are retained.
The compiler pin is **0.1.0-dev.61+91f335268900** (`toolchains.lock.json`).

## Products and internal support

| Path | Product |
| --- | --- |
| `native/gpu/webgpu` | `gpu-webgpu`: owned WebGPU API, Dawn natively and browser WebGPU |
| `native/gpu/webgpu/sys` | `gpu-webgpu-sys`: pinned raw WebGPU C ABI |
| `native/gpu/metal` | `gpu-metal`: direct Apple Metal bindings |
| `native/platform/window` | `platform-window`: AppKit, Win32, Wayland/X11, browser canvas and headless windows |

The WebGPU and window `browser/` runtime files are **supported product payloads**, not
build-tool implementation details. C/Objective-C bridges and generated declarations are
also required package inputs. `examples`, `tests`, `sys/abi`, pixel fixtures and `bench`
are distinct internal verification/demo packages, not additional public libraries.
Root `scripts/` and package generators/harnesses are internal maintainer tools.
No graphics, SVG or UI libraries are included; downstream rendering belongs to
[lib-graphics](https://github.com/silk-lang/lib-graphics).

Use an exact Git commit revision when depending on this repository, selecting the package
subdirectory above. Do not depend on sibling checkout paths. No nonstd checkout is required.

## Building

Install the pinned Silk release and LLVM 22, with `silkc` on `PATH` and `SILK_CLANG`
pointing at LLVM's `clang`. The commands below use the release's `silkc` executable.
Normal Unix builds use Silk's `[native]` source and checksum-pinned archive declarations:
Silk downloads Dawn into `$SILK_HOME/archives` (default `~/.silk/archives`) and compiles the
C/Objective-C bridges. **No Python, installer script, or generator is required to build.**
Generated inputs are committed; run `silk generate` only when intentionally refreshing them.

```sh
silkc check --workspace
silkc build --manifest-path native/gpu/webgpu/tests/native/silk.toml
silkc build --manifest-path native/gpu/webgpu/examples/silk.toml --target wasm32-unknown-unknown
```

- **macOS arm64:** macOS 26+ for the pinned Dawn binary, Apple SDK/Command Line Tools,
  LLVM 22. Export `SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"` before native builds.
  Metal links system frameworks. The GLFW example additionally needs GLFW.
- **Linux x86_64:** LLVM 22, C++ runtime, Vulkan loader and a GPU driver (Mesa lavapipe
  works headlessly). Window builds require `libx11-dev libxkbcommon-dev libwayland-dev`;
  the GLFW example additionally requires `libglfw3-dev`. X11 smoke tests need Xvfb/xauth;
  Wayland smoke tests need Weston. Committed Wayland protocol sources need no scanner
  during ordinary builds.
- **Windows x86_64:** LLVM 22, an x64 Visual Studio 2026 developer shell, Windows SDK
  and current Visual C++ Redistributable. Run `./scripts/prepare-windows.ps1` once to
  prepare the SDK bridge, Dawn DLL and GLFW archive overrides. Run it with
  `-RuntimeDirectory <executable-directory>` to stage Dawn, DXC and GLFW runtime DLLs
  beside an executable; keep those DLLs with distributed programs. This preparation
  exists because the pinned toolchain's native SDK/archive integration needs it.
  Preparation selects the first matching tool on `PATH` and preserves unrelated
  entries in `$SILK_HOME/native-overrides.toml`, including other repositories' archives.
- **Browser:** LLVM's `wasm-ld`, an actual WebGPU-capable browser and a secure context
  (HTTPS or localhost). The window host's suspending interface requires JSPI;
  a non-JSPI application must explicitly poll rather than block.

Archive availability is not a claim of tested runtime support on every architecture.
See the package [WebGPU prerequisites](native/gpu/webgpu/docs/reference.md#toolchain-and-platform-prerequisites).

## Browser artifact contract

Consume these files from the **same exact lib-native Git revision as the Silk dependency**:

- `native/gpu/webgpu/browser/host.ts` and `generated_schema.ts`, retaining their relative
  paths. They are browser-loadable ECMAScript despite the `.ts` extension: serve with a
  JavaScript MIME type, or bundle them together. `runner.ts` and `canvas-test.ts` are
  sample/test drivers, not required application hosts.
- `native/gpu/webgpu/browser/generated_webgpu_exports.silk` is the root-application
  forwarding template. Copy it into the consuming application's source tree and import
  that module from the application root. The root must depend on the packages under
  aliases `webgpu` and `webgpu_sys`. Dependency-only symbols are not exported from wasm.
- `native/platform/window/browser/window-host.js`, `imports.js` and `keys.js`, retaining
  their relative imports, provide the browser window/input host.

The WebGPU module forwards ten `webgpu_callback_*` entrypoints plus `webgpu_alloc` and
`webgpu_free`. Enter wasm through `WebGPUHost.call`; promises enqueue results, and only
explicit pumping invokes callbacks. Keep callback userdata pinned until completion or
cancellation; no outstanding loans may survive a pump. See the
[browser ABI](native/gpu/webgpu/docs/reference.md#browser-embedding-abi).

Consumers can resolve their pinned checkout at `$SILK_HOME/git/<revision>` (default
`~/.silk/git/<revision>`) after `silkc build` or `silkc check` has fetched the dependency.
Copying the checked-in template needs neither Python nor schema regeneration.
For intentional maintenance, `python3 native/gpu/webgpu/scripts/generate-browser
--exports-output /absolute/consumer/src/generated_webgpu_exports.silk` explicitly writes
one additional consumer file; `--check` compares without writing. With no destination,
this generator touches only native-owned outputs. `embed-shaders` likewise only embeds
the native sample shaders, never graphics or UI siblings.

## Verification

From the repository root, with the native prerequisites installed:

```sh
(cd native/platform/window && silkc test --root src/tests.silk)
silkc run --manifest-path native/gpu/webgpu/sys/abi/silk.toml
silkc run --manifest-path native/gpu/webgpu/tests/native/silk.toml
# macOS:
silkc run --manifest-path native/gpu/metal/examples/compute/silk.toml
silkc run --manifest-path native/gpu/metal/examples/headless-render/silk.toml
silkc run --manifest-path native/platform/window/tests/macos/silk.toml
# Linux, with a display:
silkc run --manifest-path native/platform/window/tests/linux/silk.toml
# Windows, after runtime DLL staging:
silkc run --manifest-path native/platform/window/tests/windows/silk.toml
```

The [native workflow](.github/workflows/native.yml) invokes Silk directly. It checks the
workspace, headless window tests, C/Silk WebGPU ABI, lifetime/validation scenarios, all six
native pixel/compute samples, Metal/AppKit, Win32 (fiber and caller-stack modes), X11 and
headless Weston, and compiled wasm WebGPU/window canvas execution. GLFW is build-covered;
its interactive presentation proof remains the optional historical `scripts/test-window`
harness. Browser tests use the existing installed Chromium plus checksum-pinned Playwright
JavaScript; unavailable adapters, compilers or browsers fail instead of silently skipping.

Internal `node scripts/setup-ci.mjs [--linux-gpu] [--browser]` provisions hosted runners
and writes `GITHUB_PATH`/`GITHUB_ENV` (locally it prints paths; it does not modify your shell).
It validates the native compiler seed, accepts installed LLVM 22, and otherwise downloads
the pinned LLVM distribution. Linux CI checks the recorded runner LLVM version and pins
Mesa from the recorded Ubuntu snapshot. `scripts/artifact.mjs` only finds already-built
artifacts; `scripts/check-pixels.mjs` only compares already-produced native pixels.
These are not dependency build hooks.

Historical Python generators, declaration coverage tools, analytic fixture regeneration,
C/Rust benchmarking, and specialized presentation harnesses remain opt-in maintainer
utilities. Their package-relative and repository-root paths resolve inside this extraction;
none is imported by a downstream repository or required by normal builds/CI. Their old
all-in-one `scripts/check` is not the standalone CI gate. Historical measured results in
package docs describe their original runs, not verification of this extraction.

Local extraction verification on macOS arm64 passed the 16-package workspace check,
seven headless window tests, 92 C/Silk ABI record layouts and 202 native symbols,
native WebGPU lifetime/validation checks, all six native and browser pixel/compute
fixtures, Metal compute/headless rendering, and browser window/IME/input smoke checks.
Linux and Windows execution use the CI gates above; they were not run on this host.
