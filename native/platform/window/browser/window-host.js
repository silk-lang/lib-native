// The browser backend of native/platform/window: every `window#silk_window_*` import of
// native/bridge.h, over canvases in the page.
//
//   const host = new WindowHost({ container: document.body });
//   const instance = await WebAssembly.instantiate(module, { window: host.imports, ... });
//   host.attach(instance);
//
// A window is a canvas. `open` takes the next canvas given to `addCanvas`, or makes one in
// the container. Its `RawSurface.Canvas` handle is the window id, and the canvas has the id
// `silk-window-<handle>` (unless it had one), which is what `SurfaceTarget.canvasId` takes.
//
// Waiting: a browser cannot block. `nextEvent` returns what is queued and never waits,
// unless the page calls the Silk entry point through JSPI (`host.promising(name)`), in
// which case a wait suspends the wasm stack until an event arrives or the time is up.
// Without JSPI, call the app's poll export from `host.onWake` and `requestAnimationFrame`.
//
// Keys and text go through one hidden textarea that follows the focused canvas and sits
// at the caret, so input methods compose there and their candidate window opens next to
// the text. Frames are paced by requestAnimationFrame.
import { IMPORTS } from './imports.js';
import { CODES, NAMED, CODE_TO_NAMED } from './keys.js';

const EVENT = {
  REQUEST_FRAME: 1, RESIZED: 2, SCALE_FACTOR_CHANGED: 3, MOVED: 4, FOCUSED: 5, OCCLUDED: 6,
  CLOSE_REQUESTED: 7, FULL_SCREEN_CHANGED: 8, KEY_DOWN: 9, KEY_UP: 10, MODIFIERS_CHANGED: 11,
  TEXT_INPUT: 12, IME_PREEDIT: 13, MOUSE_MOVED: 14, MOUSE_DOWN: 15, MOUSE_UP: 16,
  MOUSE_ENTERED: 17, MOUSE_EXITED: 18, SCROLL: 19, PINCH: 20, MENU_ACTION: 21,
  DIALOG_CLOSED: 22, APPEARANCE_CHANGED: 23, QUIT_REQUESTED: 24,
};
const FLAG = { REPEAT: 1, IME_HANDLED: 2, ON: 4, PRECISE: 8 };
const MOD = { SHIFT: 1, CONTROL: 2, ALT: 4, META: 8, CAPS_LOCK: 16 };
const WINDOW = { RESIZABLE: 1, VISIBLE: 2, TRANSPARENT: 4 };
const MENU = { BEGIN: 1, END: 2, ACTION: 3, SEPARATOR: 4, SYSTEM: 5 };
const ITEM = { QUIT: 5, CLOSE_WINDOW: 6, TOGGLE_FULL_SCREEN: 9 };
const BACKEND_BROWSER = 4;
const SURFACE_CANVAS = 5;
const EVENT_SIZE = 88;
const CURSORS = ['default', 'text', 'pointer', 'crosshair', 'grabbing', 'grab', 'not-allowed',
  'ew-resize', 'ns-resize', 'w-resize', 'e-resize', 'n-resize', 's-resize', 'nwse-resize',
  'nesw-resize', 'col-resize', 'row-resize', 'vertical-text', 'copy', 'alias', 'context-menu',
  'none'];
// DOM MouseEvent.button to event.MouseButton.
const BUTTONS = [0, 2, 1, 3, 4];
const CODE_INDEX = new Map(CODES.map((name, index) => [name, index]));
const NAMED_INDEX = new Map(NAMED.map((name, index) => [name, index]));
const encoder = new TextEncoder();
const decoder = new TextDecoder();
const apple = /Mac|iPhone|iPad/.test(globalThis.navigator?.platform ?? '');

function event(kind, window = 0) {
  return { kind, window, code: 0, named: 0, modifiers: 0, flags: 0, count: 0, phase: 0,
    x: 0, y: 0, dx: 0, dy: 0, value: 0, start: 0n, end: 0n, texts: [] };
}

function modifiersOf(e) {
  let bits = 0;
  if (e.shiftKey) bits |= MOD.SHIFT;
  if (e.ctrlKey) bits |= MOD.CONTROL;
  if (e.altKey) bits |= MOD.ALT;
  if (e.metaKey) bits |= MOD.META;
  if (e.getModifierState?.('CapsLock')) bits |= MOD.CAPS_LOCK;
  return bits;
}

function singleCharacter(text) {
  if (!text) return false;
  const first = text.codePointAt(0);
  return String.fromCodePoint(first).length === text.length && first >= 0x20 && first !== 0x7f;
}

/**
 * Hosts native/platform/window's browser backend for one wasm instance.
 */
export class WindowHost {
  constructor({ container } = {}) {
    this.container = container ?? globalThis.document?.body;
    this.spare = [];
    this.windows = new Map();
    this.nextId = 1;
    this.queue = [];
    this.current = [];
    this.menu = [];
    this.pendingMenu = null;
    this.clipboard = '';
    this.dialogs = new Map();
    this.nextDialog = 1;
    this.started = false;
    this.focused = 0;
    this.waiter = null;
    /** Called whenever an event is queued; a page without JSPI polls from here. */
    this.onWake = undefined;
    this.imports = Object.fromEntries(IMPORTS.map(name => [name, (...args) => this[name](...args)]));
    // Without JSPI, waiting returns at once; with it, `promising` swaps this in.
    this.blocking = false;
  }

  /** Gives `open` a canvas to use instead of making one. */
  addCanvas(canvas) { this.spare.push(canvas); }

  attach(instance) {
    this.exports = instance.exports;
    if (!(this.exports.memory instanceof WebAssembly.Memory)) throw new TypeError('Silk wasm must export memory');
  }

  /**
   * Imports with `silk_window_next_event` able to suspend, for a module whose entry point
   * is called through `promising`. Needs JSPI (WebAssembly.Suspending).
   */
  get suspendingImports() {
    if (typeof WebAssembly.Suspending !== 'function') throw new Error('This browser has no JSPI');
    this.blocking = true;
    return { ...this.imports, silk_window_next_event: new WebAssembly.Suspending((t, p) => this.waitEvent(t, p)) };
  }

  /** Wraps an export so that it may suspend in `nextEvent`; returns an async function. */
  promising(name) { return WebAssembly.promising(this.exports[name]); }

  /** The files a dialog returned, as File objects; the Silk side gets their names. */
  dialogFiles(dialog) { return this.dialogs.get(dialog) ?? []; }

  // ---------------------------------------------------------------------------------
  // Memory.

  view() { return new DataView(this.exports.memory.buffer); }
  bytes(ptr, length) { return new Uint8Array(this.exports.memory.buffer, ptr, length); }
  text(ptr, length) { return length ? decoder.decode(this.bytes(ptr, length)) : ''; }
  f64(ptr, value) { if (ptr) this.view().setFloat64(ptr, value, true); }

  // ---------------------------------------------------------------------------------
  // The queue.

  push(record) {
    this.queue.push(record);
    if (this.waiter) {
      const wake = this.waiter;
      this.waiter = null;
      wake();
    }
    this.onWake?.();
  }

  pushText(kind, window, text) {
    const record = event(kind, window);
    record.texts = [text];
    this.push(record);
    return record;
  }

  write(ptr, record) {
    const view = this.view();
    const fields = ['kind', 'window', 'code', 'named', 'modifiers', 'flags', 'count', 'phase'];
    fields.forEach((field, i) => view.setUint32(ptr + i * 4, record[field] >>> 0, true));
    ['x', 'y', 'dx', 'dy', 'value'].forEach((field, i) => view.setFloat64(ptr + 32 + i * 8, record[field], true));
    view.setBigUint64(ptr + 72, BigInt(record.start), true);
    view.setBigUint64(ptr + 80, BigInt(record.end), true);
    this.current = record.texts.map(text => encoder.encode(text));
  }

  pop(ptr) {
    const record = this.queue.shift();
    if (!record) return 0;
    this.write(ptr, record);
    return 1;
  }

  waitEvent(timeout, ptr) {
    if (this.queue.length || timeout === 0) return this.pop(ptr);
    return new Promise(resolve => {
      let timer;
      const finish = () => { clearTimeout(timer); this.waiter = null; resolve(this.pop(ptr)); };
      this.waiter = finish;
      if (timeout > 0) timer = setTimeout(finish, timeout * 1000);
    });
  }

  // ---------------------------------------------------------------------------------
  // Start and windows.

  silk_window_start() {
    if (this.started) return 1;
    const document = globalThis.document;
    if (!document) return 3;
    this.started = true;
    this.input = document.createElement('textarea');
    Object.assign(this.input.style, {
      position: 'fixed', left: '0px', top: '0px', width: '1px', height: '1px', opacity: '0',
      padding: '0', border: '0', margin: '0', resize: 'none', overflow: 'hidden',
      outline: 'none', caretColor: 'transparent', zIndex: '-1', whiteSpace: 'pre',
    });
    this.input.setAttribute('autocomplete', 'off');
    this.input.setAttribute('autocorrect', 'off');
    this.input.setAttribute('autocapitalize', 'off');
    this.input.setAttribute('spellcheck', 'false');
    this.input.setAttribute('aria-hidden', 'true');
    this.input.tabIndex = -1;
    document.body.appendChild(this.input);
    this.listenInput();
    this.dark = globalThis.matchMedia?.('(prefers-color-scheme: dark)');
    this.dark?.addEventListener?.('change', () => {
      const record = event(EVENT.APPEARANCE_CHANGED);
      record.code = this.silk_window_appearance();
      this.push(record);
    });
    document.addEventListener('visibilitychange', () => {
      for (const window of this.windows.values()) {
        const record = event(EVENT.OCCLUDED, window.id);
        if (document.hidden) record.flags |= FLAG.ON;
        this.push(record);
      }
    });
    document.addEventListener('fullscreenchange', () => {
      for (const window of this.windows.values()) {
        const now = document.fullscreenElement === window.canvas;
        if (now === window.fullScreen) continue;
        window.fullScreen = now;
        const record = event(EVENT.FULL_SCREEN_CHANGED, window.id);
        if (now) record.flags |= FLAG.ON;
        this.push(record);
      }
    });
    document.addEventListener('paste', e => {
      const text = e.clipboardData?.getData('text/plain');
      if (text) this.clipboard = text;
    });
    this.watchScale();
    return 0;
  }

  silk_window_backend() { return this.started ? BACKEND_BROWSER : 0; }

  silk_window_appearance() { return this.dark?.matches ? 1 : 0; }

  watchScale() {
    const query = globalThis.matchMedia?.(`(resolution: ${globalThis.devicePixelRatio}dppx)`);
    query?.addEventListener?.('change', () => {
      for (const window of this.windows.values()) {
        const record = event(EVENT.SCALE_FACTOR_CHANGED, window.id);
        record.value = this.scale();
        this.push(record);
        this.resized(window, true);
      }
      this.watchScale();
    }, { once: true });
  }

  scale() { return Math.max(globalThis.devicePixelRatio || 1, 1); }

  silk_window_open(titlePtr, titleLength, width, height, minWidth, minHeight, flags) {
    if (!this.started) return 0;
    const document = globalThis.document;
    const id = this.nextId++;
    let canvas = this.spare.shift();
    const owned = !canvas;
    if (owned) {
      canvas = document.createElement('canvas');
      canvas.style.display = 'block';
      canvas.style.width = `${width}px`;
      canvas.style.height = `${height}px`;
      this.container.appendChild(canvas);
    }
    if (!canvas.id) canvas.id = `silk-window-${id}`;
    if (minWidth > 0) canvas.style.minWidth = `${minWidth}px`;
    if (minHeight > 0) canvas.style.minHeight = `${minHeight}px`;
    canvas.style.touchAction = 'none';
    const window = { id, canvas, owned, width: 0, height: 0, visible: true, frame: 0,
      pendingFrame: false, fullScreen: false, ime: { enabled: false, x: 0, y: 0, width: 0, height: 0 },
      clicks: { count: 0, time: 0, x: 0, y: 0, button: -1 }, inside: false };
    this.windows.set(id, window);
    const title = this.text(titlePtr, titleLength);
    if (title && this.windows.size === 1) document.title = title;
    if (!(flags & WINDOW.VISIBLE)) this.silk_window_set_visible(id, 0);
    this.listenCanvas(window);
    window.observer = new ResizeObserver(() => this.resized(window, false));
    window.observer.observe(canvas);
    this.resized(window, true);
    return id;
  }

  silk_window_close(id) {
    const window = this.windows.get(id);
    if (!window) return;
    cancelAnimationFrame(window.frame);
    window.observer.disconnect();
    window.abort.abort();
    if (window.owned) window.canvas.remove();
    this.windows.delete(id);
    this.queue = this.queue.filter(record => record.window !== id);
    if (this.focused === id) this.focused = 0;
  }

  // Sends Resized when the canvas's size changed, and a frame with it.
  resized(window, force) {
    if (!window.visible) return;
    const rect = window.canvas.getBoundingClientRect();
    const width = window.canvas.clientWidth || rect.width;
    const height = window.canvas.clientHeight || rect.height;
    if (!force && width === window.width && height === window.height) return;
    window.width = width;
    window.height = height;
    const record = event(EVENT.RESIZED, window.id);
    record.x = width;
    record.y = height;
    record.value = this.scale();
    this.push(record);
    this.frame(window);
  }

  silk_window_next_event(timeout, ptr) { return this.pop(ptr); }

  silk_window_event_text(index, ptr, capacity) {
    const bytes = this.current[index];
    if (!bytes) return 0;
    if (ptr && capacity) this.bytes(ptr, Math.min(capacity, bytes.length)).set(bytes.subarray(0, capacity));
    return bytes.length;
  }

  frame(window) {
    if (window.pendingFrame) return;
    window.pendingFrame = true;
    window.frame = requestAnimationFrame(time => {
      window.pendingFrame = false;
      if (!this.windows.has(window.id) || !window.visible) return;
      const record = event(EVENT.REQUEST_FRAME, window.id);
      record.value = (time + 1000 / 60) / 1000;
      this.push(record);
    });
  }

  // The browser loop is single-threaded: nothing waits on another thread to wake it.
  silk_window_wake() {}

  silk_window_request_frame(id) {
    const window = this.windows.get(id);
    if (window) this.frame(window);
  }

  silk_window_content_size(id, widthPtr, heightPtr) {
    const window = this.windows.get(id);
    this.f64(widthPtr, window ? window.width : 0);
    this.f64(heightPtr, window ? window.height : 0);
  }

  silk_window_scale_factor(id) { return this.scale(); }

  silk_window_raw_surface(id, firstPtr, secondPtr, idPtr) {
    const view = this.view();
    if (firstPtr) view.setUint32(firstPtr, 0, true);
    if (secondPtr) view.setUint32(secondPtr, 0, true);
    if (idPtr) view.setBigUint64(idPtr, BigInt(this.windows.has(id) ? id : 0), true);
    return this.windows.has(id) ? SURFACE_CANVAS : 0;
  }

  silk_window_set_title(id, ptr, length) {
    const first = this.windows.keys().next().value;
    if (id === first) globalThis.document.title = this.text(ptr, length);
  }

  silk_window_set_content_size(id, width, height) {
    const window = this.windows.get(id);
    if (!window) return;
    window.canvas.style.width = `${width}px`;
    window.canvas.style.height = `${height}px`;
    this.resized(window, false);
  }

  silk_window_set_min_content_size(id, width, height) {
    const window = this.windows.get(id);
    if (!window) return;
    window.canvas.style.minWidth = `${width}px`;
    window.canvas.style.minHeight = `${height}px`;
  }

  silk_window_set_visible(id, visible) {
    const window = this.windows.get(id);
    if (!window) return;
    window.visible = !!visible;
    window.canvas.style.visibility = visible ? '' : 'hidden';
    if (visible) this.resized(window, true);
  }

  silk_window_focus(id) {
    if (this.windows.has(id)) this.focusInput(id);
  }

  silk_window_minimize(id) {}

  silk_window_toggle_full_screen(id) {
    const window = this.windows.get(id);
    const document = globalThis.document;
    if (!window) return;
    if (document.fullscreenElement === window.canvas) document.exitFullscreen?.().catch(() => {});
    else window.canvas.requestFullscreen?.().catch(() => {});
  }

  silk_window_start_window_drag(id) {}

  silk_window_set_cursor(id, cursor) {
    const window = this.windows.get(id);
    if (window) window.canvas.style.cursor = CURSORS[cursor] ?? 'default';
  }

  silk_window_set_ime(id, enabled, x, y, width, height) {
    const window = this.windows.get(id);
    if (!window) return;
    window.ime = { enabled: !!enabled, x, y, width, height };
    if (this.focused === id) this.placeInput(window);
  }

  // ---------------------------------------------------------------------------------
  // Keyboard, text and input methods, through the hidden textarea.

  focusInput(id) {
    const window = this.windows.get(id);
    if (!window) return;
    if (this.focused !== id && this.focused) this.focusChanged(this.focused, false);
    this.focused = id;
    this.placeInput(window);
    if (globalThis.document.activeElement !== this.input) this.input.focus({ preventScroll: true });
  }

  placeInput(window) {
    const rect = window.canvas.getBoundingClientRect();
    const ime = window.ime;
    const style = this.input.style;
    style.left = `${rect.left + ime.x}px`;
    style.top = `${rect.top + ime.y}px`;
    style.width = `${Math.max(ime.width, 1)}px`;
    style.height = `${Math.max(ime.height, 1)}px`;
    style.fontSize = `${Math.max(ime.height, 1)}px`;
    style.lineHeight = style.height;
  }

  focusChanged(id, focused) {
    const record = event(EVENT.FOCUSED, id);
    if (focused) record.flags |= FLAG.ON;
    this.push(record);
  }

  listenInput() {
    const input = this.input;
    input.addEventListener('focus', () => { if (this.focused) this.focusChanged(this.focused, true); });
    input.addEventListener('blur', () => { if (this.focused) this.focusChanged(this.focused, false); });
    input.addEventListener('keydown', e => this.keyDown(e));
    input.addEventListener('keyup', e => this.keyUp(e));
    input.addEventListener('compositionstart', () => { this.composing = true; });
    input.addEventListener('compositionupdate', e => this.preedit(e.data ?? ''));
    input.addEventListener('compositionend', e => {
      this.composing = false;
      this.preedit('');
      if (e.data) this.pushText(EVENT.TEXT_INPUT, this.focused, e.data);
      input.value = '';
    });
    input.addEventListener('input', e => {
      if (this.composing || e.isComposing) return;
      if (e.inputType === 'insertText' || e.inputType === 'insertReplacementText' || e.inputType === 'insertFromPaste') {
        const text = e.data ?? input.value;
        if (text) this.pushText(EVENT.TEXT_INPUT, this.focused, text);
      }
      input.value = '';
    });
  }

  preedit(text) {
    if (!this.focused) return;
    const record = this.pushText(EVENT.IME_PREEDIT, this.focused, text);
    const caret = BigInt(encoder.encode(text).length);
    record.start = caret;
    record.end = caret;
  }

  keyRecord(kind, e) {
    const record = event(kind, this.focused);
    record.code = CODE_INDEX.get(e.code) ?? 0;
    const named = NAMED_INDEX.get(e.key) ?? NAMED_INDEX.get(CODE_TO_NAMED[e.code] ?? '') ?? 0;
    const character = !named && singleCharacter(e.key);
    record.named = character ? 0 : named;
    record.modifiers = modifiersOf(e);
    if (character) record.texts = [e.key];
    return record;
  }

  keyDown(e) {
    if (!this.focused) return;
    if (e.isComposing || e.keyCode === 229) {
      const record = this.keyRecord(EVENT.KEY_DOWN, e);
      record.flags |= FLAG.IME_HANDLED;
      this.push(record);
      return;
    }
    const record = this.keyRecord(EVENT.KEY_DOWN, e);
    if (this.shortcut(record, e)) { e.preventDefault(); return; }
    if (e.repeat) record.flags |= FLAG.REPEAT;
    this.push(record);
    // Keys the page would otherwise act on: focus moves, scrolling, browser shortcuts.
    if (record.named && e.key !== 'Unidentified' && !['Shift', 'Control', 'Alt', 'Meta', 'CapsLock'].includes(e.key)) e.preventDefault();
  }

  keyUp(e) {
    if (!this.focused) return;
    this.push(this.keyRecord(EVENT.KEY_UP, e));
  }

  // Menu shortcuts, as silk_menu_match in native/common/core.c. On Apple systems the
  // Command key stands in for Control, as people expect there.
  shortcut(record, e) {
    let modifiers = record.modifiers & (MOD.SHIFT | MOD.CONTROL | MOD.ALT | MOD.META);
    if (apple && (modifiers & MOD.META)) modifiers = (modifiers & ~MOD.META) | MOD.CONTROL;
    const key = record.texts[0] ?? (e.key.length === 1 ? e.key : '');
    const character = key ? key.toLowerCase().codePointAt(0) : 0;
    for (const entry of this.menu) {
      if (!entry.keyKind || !entry.enabled) continue;
      if (entry.type !== MENU.ACTION && entry.type !== MENU.SYSTEM) continue;
      if ((entry.modifiers & (MOD.SHIFT | MOD.CONTROL | MOD.ALT | MOD.META)) !== modifiers) continue;
      const hit = entry.keyKind === 1 ? character && entry.keyValue === character : record.named && entry.keyValue === record.named;
      if (!hit) continue;
      if (entry.type === MENU.ACTION) {
        const action = event(EVENT.MENU_ACTION);
        action.code = entry.action;
        this.push(action);
      } else {
        this.systemItem(entry.item);
      }
      return true;
    }
    return false;
  }

  systemItem(item) {
    if (item === ITEM.QUIT) this.push(event(EVENT.QUIT_REQUESTED));
    else if (item === ITEM.CLOSE_WINDOW && this.focused) this.push(event(EVENT.CLOSE_REQUESTED, this.focused));
    else if (item === ITEM.TOGGLE_FULL_SCREEN && this.focused) this.silk_window_toggle_full_screen(this.focused);
  }

  // ---------------------------------------------------------------------------------
  // Pointer.

  listenCanvas(window) {
    const canvas = window.canvas;
    window.abort = new AbortController();
    const options = { signal: window.abort.signal };
    const at = (record, e) => {
      const rect = canvas.getBoundingClientRect();
      record.x = e.clientX - rect.left;
      record.y = e.clientY - rect.top;
      record.modifiers = modifiersOf(e);
      return record;
    };
    canvas.addEventListener('pointermove', e => {
      this.push(at(event(EVENT.MOUSE_MOVED, window.id), e));
    }, options);
    canvas.addEventListener('pointerdown', e => {
      this.focusInput(window.id);
      e.preventDefault();
      if (e.pointerId !== undefined) try { canvas.setPointerCapture(e.pointerId); } catch {}
      const record = at(event(EVENT.MOUSE_DOWN, window.id), e);
      record.code = BUTTONS[e.button] ?? 5;
      const clicks = window.clicks;
      const now = performance.now();
      const near = Math.abs(record.x - clicks.x) < 4 && Math.abs(record.y - clicks.y) < 4;
      clicks.count = near && clicks.button === e.button && now - clicks.time < 500 ? clicks.count + 1 : 1;
      Object.assign(clicks, { time: now, x: record.x, y: record.y, button: e.button });
      record.count = clicks.count;
      this.push(record);
    }, options);
    canvas.addEventListener('pointerup', e => {
      const record = at(event(EVENT.MOUSE_UP, window.id), e);
      record.code = BUTTONS[e.button] ?? 5;
      this.push(record);
    }, options);
    canvas.addEventListener('pointerenter', e => {
      this.push(at(event(EVENT.MOUSE_ENTERED, window.id), e));
    }, options);
    canvas.addEventListener('pointerleave', () => this.push(event(EVENT.MOUSE_EXITED, window.id)), options);
    canvas.addEventListener('contextmenu', e => e.preventDefault(), options);
    canvas.addEventListener('wheel', e => {
      e.preventDefault();
      const record = at(event(EVENT.SCROLL, window.id), e);
      // Positive is up and left, as on the other backends.
      const scale = e.deltaMode === 2 ? window.height || 1 : 1;
      record.dx = -e.deltaX * scale;
      record.dy = -e.deltaY * scale;
      if (e.deltaMode === 0) record.flags |= FLAG.PRECISE;
      if (e.ctrlKey && e.deltaMode === 0) {
        // Trackpad pinches arrive as wheel events with Control held.
        const pinch = at(event(EVENT.PINCH, window.id), e);
        pinch.dx = -e.deltaY / 100;
        this.push(pinch);
        return;
      }
      this.push(record);
    }, { ...options, passive: false });
  }

  // ---------------------------------------------------------------------------------
  // Menus: no menu bar in a page, but the shortcuts work.

  silk_window_menu_begin_bar() { this.pendingMenu = []; }
  silk_window_menu_begin(ptr, length) { this.pendingMenu?.push({ type: MENU.BEGIN, title: this.text(ptr, length) }); }
  silk_window_menu_end() { this.pendingMenu?.push({ type: MENU.END }); }
  silk_window_menu_action(ptr, length, action, keyKind, keyValue, modifiers, enabled, checked) {
    this.pendingMenu?.push({ type: MENU.ACTION, title: this.text(ptr, length), action, keyKind,
      keyValue: keyKind === 1 && keyValue < 0x80 ? String.fromCodePoint(keyValue).toLowerCase().codePointAt(0) : keyValue,
      modifiers, enabled: !!enabled, checked: !!checked });
  }
  silk_window_menu_separator() { this.pendingMenu?.push({ type: MENU.SEPARATOR }); }
  silk_window_menu_system(item) {
    const entry = { type: MENU.SYSTEM, item, keyKind: 0, keyValue: 0, modifiers: 0, enabled: true };
    if (item === ITEM.QUIT) Object.assign(entry, { keyKind: 1, keyValue: 0x71, modifiers: MOD.CONTROL });
    if (item === ITEM.CLOSE_WINDOW) Object.assign(entry, { keyKind: 1, keyValue: 0x77, modifiers: MOD.CONTROL });
    if (item === ITEM.TOGGLE_FULL_SCREEN) Object.assign(entry, { keyKind: 2, keyValue: NAMED_INDEX.get('F11') });
    this.pendingMenu?.push(entry);
  }
  silk_window_menu_commit() {
    if (this.pendingMenu) this.menu = this.pendingMenu;
    this.pendingMenu = null;
  }

  // ---------------------------------------------------------------------------------
  // Dialogs: an open dialog is a file input, and needs a user gesture to show. Its paths
  // are the file names; dialogFiles(id) has the File objects. Save dialogs are not offered.

  silk_window_open_dialog(titlePtr, titleLength, dirPtr, dirLength, extPtr, extLength, flags, parent) {
    const document = globalThis.document;
    if (!this.started || !document) return 0;
    const id = this.nextDialog++;
    const input = document.createElement('input');
    input.type = 'file';
    input.style.display = 'none';
    const extensions = this.text(extPtr, extLength).split(',').map(s => s.trim()).filter(Boolean);
    if (extensions.length) input.accept = extensions.map(e => `.${e}`).join(',');
    if (flags & 4) input.multiple = true;
    if (flags & 2) input.webkitdirectory = true;
    let done = false;
    const finish = files => {
      if (done) return;
      done = true;
      input.remove();
      this.dialogs.set(id, files);
      const record = event(EVENT.DIALOG_CLOSED);
      record.code = id;
      record.count = files.length;
      record.texts = files.map(file => file.webkitRelativePath || file.name);
      this.push(record);
    };
    input.addEventListener('change', () => finish([...(input.files ?? [])]));
    input.addEventListener('cancel', () => finish([]));
    document.body.appendChild(input);
    try { input.showPicker ? input.showPicker() : input.click(); }
    catch { input.remove(); return 0; }
    return id;
  }

  silk_window_save_dialog() { return 0; }

  // ---------------------------------------------------------------------------------
  // Clipboard: reads are synchronous, so they return what the app last wrote or what was
  // last pasted into the page; writes also go to the system clipboard when allowed.

  silk_window_clipboard_read(ptr, capacity) {
    const bytes = encoder.encode(this.clipboard);
    if (ptr && capacity) this.bytes(ptr, Math.min(capacity, bytes.length)).set(bytes.subarray(0, capacity));
    return BigInt(bytes.length);
  }

  silk_window_clipboard_write(ptr, length) {
    this.clipboard = this.text(ptr, length);
    globalThis.navigator?.clipboard?.writeText?.(this.clipboard).catch(() => {});
  }

  // ---------------------------------------------------------------------------------
  // Test hooks: they dispatch DOM events to the same listeners real input reaches.

  silk_window_test_key(id, code, ptr, length, modifiers) {
    if (!this.windows.has(id)) return 0;
    this.focusInput(id);
    const text = this.text(ptr, length);
    const codeName = CODES[code] ?? 'Unidentified';
    const key = singleCharacter(text) ? text : (CODE_TO_NAMED[codeName] ?? 'Unidentified');
    const init = { code: codeName, key, bubbles: true, cancelable: true,
      shiftKey: !!(modifiers & MOD.SHIFT), ctrlKey: !!(modifiers & MOD.CONTROL),
      altKey: !!(modifiers & MOD.ALT), metaKey: !!(modifiers & MOD.META) };
    const down = new KeyboardEvent('keydown', init);
    this.input.dispatchEvent(down);
    if (!down.defaultPrevented && singleCharacter(text) && !init.ctrlKey && !init.metaKey) {
      this.input.value = text;
      this.input.dispatchEvent(new InputEvent('input', { data: text, inputType: 'insertText', bubbles: true }));
    }
    this.input.dispatchEvent(new KeyboardEvent('keyup', init));
    return 1;
  }

  silk_window_test_mouse(id, kind, button, x, y, dx, dy) {
    const window = this.windows.get(id);
    if (!window) return 0;
    const rect = window.canvas.getBoundingClientRect();
    const init = { clientX: rect.left + x, clientY: rect.top + y, bubbles: true, cancelable: true,
      button: [0, 2, 1, 3, 4][button] ?? 0, pointerId: 1, pointerType: 'mouse' };
    const target = window.canvas;
    if (kind === 0) target.dispatchEvent(new PointerEvent('pointermove', init));
    else if (kind === 1) target.dispatchEvent(new PointerEvent('pointerdown', init));
    else if (kind === 2) target.dispatchEvent(new PointerEvent('pointerup', init));
    else target.dispatchEvent(new WheelEvent('wheel', { ...init, deltaX: -dx, deltaY: -dy, deltaMode: 1 }));
    return 1;
  }

  silk_window_test_preedit(id, ptr, length, start, end) {
    if (!this.windows.has(id)) return 0;
    this.focusInput(id);
    const data = this.text(ptr, length);
    if (!this.composing) this.input.dispatchEvent(new CompositionEvent('compositionstart', { data: '' }));
    this.input.dispatchEvent(new CompositionEvent('compositionupdate', { data }));
    return 1;
  }

  silk_window_test_commit(id, ptr, length) {
    if (!this.windows.has(id)) return 0;
    this.focusInput(id);
    this.input.dispatchEvent(new CompositionEvent('compositionend', { data: this.text(ptr, length) }));
    return 1;
  }

  silk_window_test_resize(id, width, height) {
    if (!this.windows.has(id)) return 0;
    this.silk_window_set_content_size(id, width, height);
    return 1;
  }

  silk_window_test_ime_rect(id, xPtr, yPtr, widthPtr, heightPtr) {
    const ime = this.windows.get(id)?.ime ?? { x: 0, y: 0, width: 0, height: 0 };
    this.f64(xPtr, ime.x);
    this.f64(yPtr, ime.y);
    this.f64(widthPtr, ime.width);
    this.f64(heightPtr, ime.height);
  }
}
