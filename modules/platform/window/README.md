# platform-window

Native windows, input and frame pacing for Silk applications.
It opens windows, delivers keyboard, pointer, IME, menu and dialog events, paces
frames by the display, and hands out the native objects a `gpu-webgpu` surface is created
from. It draws nothing itself.

| Backend | Module | State |
| --- | --- | --- |
| macOS (AppKit, `CAMetalLayer`, `CADisplayLink`, `NSTextInputClient`) | `macos` | First backend |
| Windows (Win32, IMM32, DwmFlush pacing) | `bridge` | Complete |
| Linux (Wayland with text-input-v3, else X11 with XIM) | `bridge` | Complete |
| Browser canvas (hidden textarea for input methods) | `bridge` | Complete |
| Headless, for tests | `headless` | Complete |

`native.NativePlatform` names the backend for the compilation target: `MacPlatform` on
macOS, `BridgePlatform` on Windows, Linux and wasm32, and `HeadlessPlatform` elsewhere.

## The model

```silk,ignore
import window.native { NativePlatform }
import window.platform { Platform }
import window.event { Event }
import window.types { Wait, WindowOptions }

let mut platform = run NativePlatform.start()
let window = run platform.openWindow(&WindowOptions.make("Hello", 800.0, 600.0))
while true {
  match move run platform.nextEvent(Wait.Forever) {
    Option<Event>.Some { value } => {
      match &value {
        Event.Resized { window, size, scaleFactor } => { resizeSurface(size, scaleFactor) }
        Event.RequestFrame { window, targetTime } => { draw(targetTime) }
        Event.MouseMoved { window, position, modifiers } => { platform.requestFrame(window) }
        Event.QuitRequested => { break }
        _ => {}
      }
    }
    _ => {}
  }
}
```

- **One interface, static dispatch.** `platform.Platform` is a Silk interface, and Silk has
  no runtime dispatch, so code that should run on every backend is generic over
  `P: Platform`. Windows are named by `WindowId`, so one value owns all native state.
- **The app owns the loop.** `nextEvent(wait)` returns the next event or `None` when the wait
  ends. There are no callbacks into Silk.
- **Frames are events, and only come when asked.** `requestFrame(window)` asks for one
  `RequestFrame`, delivered at the display's next refresh; requests in between coalesce.
  Opening, showing, resizing and a scale change deliver a frame on their own. An idle window
  costs nothing, and an animation requests again from each frame. `targetTime` is when the
  frame reaches the display.
- **Logical points.** Positions and sizes are points with the origin at the top left of the
  content area. The surface is `contentSize * scaleFactor` device pixels.
- **Text input** arrives as `TextInput` for committed text (typed characters or an input
  method's commit) and `ImePreedit` for composition. `KeyDown.imeHandled` marks keys the input
  method consumed. `setIme` turns the input method on for a text field and tells it where the
  caret is, so the candidate window follows it.
- **Menus and dialogs** never block. A `menu.MenuBar` is built once and installed with
  `setMenuBar`; choosing an item delivers `MenuAction`. File dialogs return a `DialogId` at
  once and deliver `DialogClosed` with the chosen paths.

## How the macOS backend works

`native/macos/bridge.m` is the Objective-C side, behind the C ABI in `bridge.h`; `src/macos.silk`
decodes its events. Each window is an `NSWindow` whose content view is layer-backed by a
`CAMetalLayer` (`RawSurface.MetalLayer`).

AppKit runs loops of its own: during a live resize, while a menu is open, while a window is
dragged. A pull-model API (like GLFW's) gets no control back until they end, so the window
freezes during a resize. Here AppKit instead runs on a second stack of the main thread. When
an event is queued, wherever AppKit is, the bridge switches back to Silk's stack, and
`nextEvent` returns it. Silk handles it, draws if it was a frame, and calls `nextEvent` again,
which resumes AppKit where it stopped. So a resize delivers `Resized` and `RequestFrame` from
inside `setFrameSize:` and the new frame is drawn before AppKit shows the new size. The switch
saves and restores the callee-saved registers (arm64 and x86-64) and costs nanoseconds; no
second thread is involved, so every AppKit call stays on the main thread.

Two rules follow. Closing a window hides it at once and releases it after AppKit's current
dispatch returns. `startWindowDrag` runs AppKit's drag loop on the caller's stack, so events
queue until the drag ends.

`SILK_WINDOW_FIBER=0` turns the second stack off and pumps AppKit on the caller's stack, for
diagnosing problems. Live resizing then shows stale frames.

Frames are paced by the view's `CADisplayLink` (macOS 14+), which runs on the main run loop
in every mode, including during a live resize, and is paused while no frame is requested.
`CVDisplayLink`, which the design doc names, is deprecated and calls back on another thread.

## How the Windows, Linux and browser backends work

They share one C ABI, `native/bridge.h`, and one Silk side, `src/bridge.silk`
(`BridgePlatform`), which decodes the same event record as the macOS bridge. The Silk
declarations (`src/bridge_sys.silk`) and the browser's import list (`browser/imports.js`) are
generated from one table by `scripts/generate-bridge`. `BridgePlatform.backend()` says which
window system is in use, and its `simulate*` methods feed input through the backend for
tests.

**Windows** (`native/windows/bridge.c`). Windows has modal loops of its own like AppKit:
live resize, window moves, menus. The message pump runs on a fiber, and the window procedure
switches back to Silk as soon as an event is queued, so frames keep coming during a resize. A
timer keeps the pump waking inside those loops. A helper thread waits on `DwmFlush` and
signals each composition while a window wants a frame. Per-monitor DPI v2, dark title bars,
`HMENU` menu bars (the client size is kept when a menu is added), IMM32 composition with the
candidate window at the caret, `IFileDialog` run from the pump so `nextEvent` keeps
returning, and the Unicode clipboard. `SILK_WINDOW_FIBER=0` pumps on the caller's stack.

**Linux** (`native/linux/`). Wayland when `WAYLAND_DISPLAY` is set and the compositor has
xdg-shell, else X11; `SILK_WINDOW_BACKEND=x11|wayland` forces one. Wayland: xdg-decoration,
or a drawn title bar where the compositor wants client-side decorations; fractional scale
with viewporter; frame callbacks for pacing (with a timer until the first one arrives, since
hidden surfaces get none); xkbcommon with compose and key repeat; text-input-v3;
`wl_data_device` for the clipboard; cursor-shape when offered. X11: XIM with preedit
callbacks, the `CLIPBOARD` selection, `_NET_WM` hints. File dialogs run `zenity` or
`kdialog` as a child process (`SILK_WINDOW_DIALOG` names another command).

**Browser** (`browser/window-host.js`). A window is a canvas; `RawSurface.Canvas` carries its
handle and the canvas has the id `silk-window-<handle>` for `SurfaceTarget.canvasId`. Keys,
text and composition go through one hidden textarea placed at the caret, so input methods
work and place their candidate window by the text. `requestAnimationFrame` paces frames. A
browser cannot block: `nextEvent` returns what is queued, unless the page calls the module
through JSPI (`host.suspendingImports` and `host.promising`), in which case a wait suspends
the wasm stack until an event arrives. Without JSPI, drive the app from `requestAnimationFrame`
and `host.onWake` with `Wait.Poll`.

Menus outside macOS and Windows have no visible bar, but their shortcuts work and system
items (Quit, Close Window, Toggle Full Screen) get the usual ones.

## Build and test

Unit tests run anywhere, on the headless backend:

```sh
(cd modules/platform/window && silkc test --root src/tests.silk)
```

On macOS 14+ with the Xcode or Command Line Tools SDK:

```sh
silkc run --manifest-path modules/platform/window/tests/macos/silk.toml
silkc run --manifest-path modules/platform/window/examples/demo/silk.toml
```

Native smoke tests share the checks in `tests/smoke`. Linux needs a running X11 or Wayland
display; Windows needs the preparation and DLL staging described in the root README:

```sh
silkc run --manifest-path modules/platform/window/tests/linux/silk.toml
silkc run --manifest-path modules/platform/window/tests/windows/silk.toml
silkc build --manifest-path modules/platform/window/tests/browser/silk.toml --target wasm32-unknown-unknown
```

`silk.toml` declares each bridge, and its libraries reach every program that depends on the
package, so a program using the window needs no `[native]` section:

- macOS compiles `native/macos/bridge.m` and links `Cocoa`, `QuartzCore` and
  `UniformTypeIdentifiers`.
- Linux compiles `native/common` and `native/linux` and links libX11, libxkbcommon and
  libwayland, so it needs their development packages. The Wayland protocol glue in
  `native/linux/protocols` is committed; `silk generate` rewrites it with `wayland-scanner`.
- Windows needs the SDK headers, which Silk does not pass to Clang, so the root
  `scripts/prepare-windows.ps1` builds the bridge and names it as the `win32-bridge`
  archive's override.

The standalone [workflow](../../../.github/workflows/native.yml) provides Xvfb and
headless Weston, runs native tests on their own hosts and invokes `browser/test.mjs`
with the compiled wasm artifact. The older Python wrappers remain optional harnesses.

The demo is the manual part of the exit test: drag a window edge and watch frames keep
coming; move the window between a Retina and a non-Retina display; leave it idle and see that
no frames are logged; type with the Japanese input method and watch the preedit and commit
lines, with the candidate window at the mouse; use File > Open… and File > Save….

The key tables in `src/keys.silk` and `native/macos/keys.h` are generated by
`scripts/generate-keys`; run its `--check` mode when maintaining generated tables.

## Not yet

- A cross-thread waker for `nextEvent`.
- File drag and drop, context menus, accessibility, and custom title bar hit testing beyond
  `startWindowDrag`.
- X11: frames are paced at 60 Hz (or `SILK_WINDOW_REFRESH_HZ`) rather than read from
  XRandR, and clipboard transfers do not use INCR, so very large texts are refused.
- Wayland: `focus` cannot raise a window (the protocol leaves that to the compositor), and
  the drawn title bar has no resize edges.
- Windows: Text Services Framework; IMM32 covers the input methods people use, and TSF would
  add reconversion.
- Browser: save dialogs, and clipboard reads beyond what the app wrote or the user pasted
  (the async clipboard API needs a permission and cannot answer synchronously).
- Surrounding-text queries for the input method (reconversion); the input method sees the
  composition only.
