# lib-native

Standalone native GPU and window libraries for [Silk](https://github.com/silk-lang/silk).
Extracted from `silk-lang/nonstd` commit
`7d6c3693a9ad1fec24127985626ab4224f9f1995`; package names are retained under the `modules/` root.
The compiler pin is **0.1.0-dev.64+8214c402307b** (`toolchains.lock.json`).

## Products and internal support

| Path | Product |
| --- | --- |
| `modules/gpu/webgpu` | `gpu-webgpu`: owned WebGPU API, Dawn natively and browser WebGPU |
| `modules/gpu/webgpu/sys` | `gpu-webgpu-sys`: pinned raw WebGPU C ABI |
| `modules/gpu/metal` | `gpu-metal`: direct Apple Metal bindings |
| `modules/platform/window` | `platform-window`: AppKit, Win32, Wayland/X11, browser canvas and headless windows |

The WebGPU and window `browser/` runtime files are **supported product payloads**, not
build-tool implementation details. C/Objective-C bridges and generated declarations are
also required package inputs. `examples`, `tests`, `sys/abi`, pixel fixtures and `bench`
are distinct internal verification/demo packages, not additional public libraries.
Root `src/tasks.silk` and package task programs are internal maintainer tools.
No graphics, SVG or UI libraries are included; downstream rendering belongs to
[lib-graphics](https://github.com/silk-lang/lib-graphics).

Use an exact Git commit revision when depending on this repository, selecting the package
subdirectory above. Do not depend on sibling checkout paths. No nonstd checkout is required.

## Building

Install the pinned Silk release and LLVM 22, with the `silk` launcher on `PATH` and
`SILK_CLANG` pointing at LLVM's `clang`, with a matching LLVM 22 linker on `PATH`.
The launcher selects the pinned compiler.
Normal Unix builds use Silk's `[native]` source and checksum-pinned archive declarations:
Silk downloads Dawn into `$SILK_HOME/archives` (default `~/.silk/archives`) and compiles the
C/Objective-C bridges. **No Python, installer script, or generator is required to build.**
Generated inputs are committed; run `silk generate` only when intentionally refreshing them.

```sh
silk check --workspace
silk build --manifest-path modules/gpu/webgpu/tests/native/silk.toml
silk build --manifest-path modules/gpu/webgpu/examples/silk.toml --target wasm32-unknown-unknown
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
  and current Visual C++ Redistributable. Run `silk task prepare-windows` once to
  prepare the SDK bridge, Dawn DLL and GLFW archive overrides. Run it with
  `--runtime-directory <executable-directory>` to stage Dawn, DXC and GLFW runtime DLLs
  beside an executable; keep those DLLs with distributed programs. This preparation
  exists because the pinned toolchain's native SDK/archive integration needs it.
  Preparation is repeatable, selects the first matching tool on `PATH`, and preserves
  unrelated entries in `$SILK_HOME/native-overrides.toml`, including other repositories' archives.
- **Browser:** LLVM's `wasm-ld`, an actual WebGPU-capable browser and a secure context
  (HTTPS or localhost). The window host's suspending interface requires JSPI;
  a non-JSPI application must explicitly poll rather than block.

Archive availability is not a claim of tested runtime support on every architecture.
See the package [WebGPU prerequisites](modules/gpu/webgpu/docs/reference.md#toolchain-and-platform-prerequisites).

## Browser artifact contract

Consume these files from the **same exact lib-native Git revision as the Silk dependency**:

- `modules/gpu/webgpu/browser/host.ts` and `generated_schema.ts`, retaining their relative
  paths. They are browser-loadable ECMAScript despite the `.ts` extension: serve with a
  JavaScript MIME type, or bundle them together. `runner.ts` and `canvas-test.ts` are
  sample/test drivers, not required application hosts.
- `modules/gpu/webgpu/browser/generated_webgpu_exports.silk` is the root-application
  forwarding template. Copy it into the consuming application's source tree and import
  that module from the application root. The root must depend on the packages under
  aliases `webgpu` and `webgpu_sys`. Dependency-only symbols are not exported from wasm.
- `modules/platform/window/browser/window-host.js`, `imports.js` and `keys.js`, retaining
  their relative imports, provide the browser window/input host.

The WebGPU module forwards ten `webgpu_callback_*` entrypoints plus `webgpu_alloc` and
`webgpu_free`. Enter wasm through `WebGPUHost.call`; promises enqueue results, and only
explicit pumping invokes callbacks. Keep callback userdata pinned until completion or
cancellation; no outstanding loans may survive a pump. See the
[browser ABI](modules/gpu/webgpu/docs/reference.md#browser-embedding-abi).

Consumers can resolve their pinned checkout at `$SILK_HOME/git/<revision>` (default
`~/.silk/git/<revision>`) after `silk build` or `silk check` has fetched the dependency.
Copying the checked-in template needs neither Python nor schema regeneration.
For intentional maintenance, `silk task --manifest-path modules/gpu/webgpu/silk.toml
generate-browser --exports-output /absolute/consumer/src/generated_webgpu_exports.silk`
explicitly writes one additional consumer file; `--check` compares without writing.
With no destination, this generator touches only native-owned outputs. `embed-shaders`
likewise only embeds native sample shaders, never graphics or UI siblings.

## Verification

From the repository root, with the native prerequisites installed:

```sh
silk task --manifest-path modules/platform/window/silk.toml test-headless
silk run --manifest-path modules/gpu/webgpu/sys/abi/silk.toml
silk run --manifest-path modules/gpu/webgpu/tests/native/silk.toml
# macOS:
silk run --manifest-path modules/gpu/metal/examples/compute/silk.toml
silk run --manifest-path modules/gpu/metal/examples/headless-render/silk.toml
silk run --manifest-path modules/platform/window/tests/macos/silk.toml
# Linux, with a display:
silk run --manifest-path modules/platform/window/tests/linux/silk.toml
# Windows, after runtime DLL staging:
silk run --manifest-path modules/platform/window/tests/windows/silk.toml
```

The [native workflow](.github/workflows/native.yml) bootstraps Clang 22 and the pinned Silk
release before dispatching Silk tasks. A task itself is compiled code, so LLVM provisioning
cannot be its first compiler prerequisite. The small CI-only Windows bootstrap checks the
same locked LLVM archive checksum/size; macOS uses Homebrew `llvm@22` and `lld@22`. The tasks cover
headless window tests, WebGPU ABI/lifetimes, six native pixel/compute samples, Metal/AppKit,
Win32 fiber and caller-stack modes, X11/Weston, and real compiled wasm canvas execution.
JavaScript host contract tests remain JavaScript test inputs for the JavaScript runtime.

```sh
silk task                                  # list root tasks
silk task install-llvm                     # print selected LLVM 22 clang
silk task setup-ci --linux-gpu              # Ubuntu runner pins
silk task setup-ci --browser                # select existing Chromium
silk task check-workspace
silk task native-pixels
silk task --manifest-path modules/platform/window/silk.toml linux smoke
silk task test-browser
```

`setup-ci` writes `GITHUB_PATH`/`GITHUB_ENV`; locally it only prints settings, never
changes the calling shell. It uses the already-bootstrapped Silk compiler, accepts an installed
LLVM 22, and otherwise installs the complete checksum-pinned LLVM distribution. On Windows
it prepares the SDK/archive overrides and appends Dawn, DXC and GLFW runtime directories
to `GITHUB_PATH`. `prepare-windows` also exports those directories and can stage DLLs beside
an executable with `--runtime-directory`. Linux CI checks the recorded runner LLVM version
and Mesa snapshot. `install-llvm` does not replace the first working compiler bootstrap: a task
is compiled code, so CI provides Clang 22 and the macOS `SDKROOT` before any task. Installers require
native `curl`, `tar`, and a streaming digest
tool (`openssl` on Unix, `certutil` on Windows).
After bootstrap, Silk tasks own download, checksum, cache and platform-selection logic.

`artifact NAME TARGET [PACKAGE-PATH] [--release]` only finds an already-built artifact;
`check-pixels DIRECTORY` only compares existing outputs. Its directory and Windows
preparation's explicit paths are relative to the invocation directory; the artifact
package path is relative to this repository. Absolute paths are accepted.
`install-dxc` prints its `bin/x64` directory;
`install-glfw` and `install-webgpu` print their installation roots. Windows preparation
accepts `--clang PATH`, `--runtime-directory PATH`, and `--json`, preserves unrelated
native overrides, and requires the existing x64 Visual Studio developer environment.

Tasks are not dependency build hooks. Another repository can use these task commands
only from a revision that contains them, or an explicitly selected migrated working tree;
an older manifest-pinned checkout does not acquire new tasks automatically.
Package READMEs describe generators, benchmarks and platform harnesses. The Windows
managed-process limitation is tracked in [silk#3034](https://github.com/silk-lang/silk/issues/3034).
Affected timed Windows/browser harnesses report this unsupported prerequisite explicitly.
Historical measured results below and in package docs describe their original runs,
not verification of this task migration.

Local extraction verification on macOS arm64 passed the 16-package workspace check,
seven headless window tests, 92 C/Silk ABI record layouts and 202 native symbols,
native WebGPU lifetime/validation checks, all six native and browser pixel/compute
fixtures, Metal compute/headless rendering, and browser window/IME/input smoke checks.
Linux and Windows execution use the CI gates above; they were not run on this host.
