// The AppKit backend of modules/platform/window. See README.md, "How the macOS backend works".
//
// Silk owns the event loop and pulls events with silk_macos_next_event. AppKit, though,
// runs loops of its own during a live resize, while a menu is open and in other tracking
// modes, and only returns to its caller when they end. To keep handing events and frames to
// Silk from inside those loops, AppKit runs on a second stack of the main thread (a fiber).
// silk_macos_next_event switches to that stack; the fiber switches back as soon as an event
// is queued, from wherever AppKit happens to be. Silk handles the event, draws if it was a
// frame, and calls silk_macos_next_event again, which resumes AppKit where it stopped.
//
// Set SILK_WINDOW_FIBER=0 to pump AppKit on the caller's stack instead. Events and frames
// then wait until AppKit's own loops end, so live resizing shows stale frames.
//
// Built with ARC.
#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <QuartzCore/QuartzCore.h>
#import <IOSurface/IOSurface.h>
#import <UserNotifications/UserNotifications.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <objc/message.h>
#include "../common/cursors.h"
#include <math.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "bridge.h"
#include "keys.h"

#if !__has_feature(objc_arc)
#error "bridge.m must be compiled with -fobjc-arc"
#endif

// ---------------------------------------------------------------------------------------
// Stack switching.
//
// silk_macos_switch(save, load) saves the callee-saved registers on the current stack,
// stores the stack pointer in *save, loads `load` as the stack pointer, and restores the
// registers saved there. A new fiber's stack is prepared so that the first switch "returns"
// into silk_macos_fiber_entry, which calls the fiber's function.

void silk_macos_switch(void **save, void *load);
void silk_macos_fiber_entry(void);

#if defined(__aarch64__)
__asm__(
    ".text\n"
    ".globl _silk_macos_switch\n"
    ".p2align 2\n"
    "_silk_macos_switch:\n"
    "  sub sp, sp, #160\n"
    "  stp x19, x20, [sp, #0]\n"
    "  stp x21, x22, [sp, #16]\n"
    "  stp x23, x24, [sp, #32]\n"
    "  stp x25, x26, [sp, #48]\n"
    "  stp x27, x28, [sp, #64]\n"
    "  stp x29, x30, [sp, #80]\n"
    "  stp d8, d9, [sp, #96]\n"
    "  stp d10, d11, [sp, #112]\n"
    "  stp d12, d13, [sp, #128]\n"
    "  stp d14, d15, [sp, #144]\n"
    "  mov x9, sp\n"
    "  str x9, [x0]\n"
    "  mov sp, x1\n"
    "  ldp x19, x20, [sp, #0]\n"
    "  ldp x21, x22, [sp, #16]\n"
    "  ldp x23, x24, [sp, #32]\n"
    "  ldp x25, x26, [sp, #48]\n"
    "  ldp x27, x28, [sp, #64]\n"
    "  ldp x29, x30, [sp, #80]\n"
    "  ldp d8, d9, [sp, #96]\n"
    "  ldp d10, d11, [sp, #112]\n"
    "  ldp d12, d13, [sp, #128]\n"
    "  ldp d14, d15, [sp, #144]\n"
    "  add sp, sp, #160\n"
    "  ret\n"
    ".globl _silk_macos_fiber_entry\n"
    ".p2align 2\n"
    "_silk_macos_fiber_entry:\n"
    "  mov x0, x19\n"
    "  blr x20\n"
    "  brk #1\n");

enum { SWITCH_FRAME = 160 };

static void *prepare_stack(char *top, void (*function)(void *), void *argument) {
    uint64_t *frame = (uint64_t *)(top - SWITCH_FRAME);
    memset(frame, 0, SWITCH_FRAME);
    frame[0] = (uint64_t)argument;               // x19
    frame[1] = (uint64_t)function;               // x20
    frame[10] = 0;                               // x29, the end of the frame chain
    frame[11] = (uint64_t)silk_macos_fiber_entry; // x30
    return frame;
}
#elif defined(__x86_64__)
__asm__(
    ".text\n"
    ".globl _silk_macos_switch\n"
    ".p2align 4\n"
    "_silk_macos_switch:\n"
    "  pushq %rbp\n"
    "  pushq %rbx\n"
    "  pushq %r12\n"
    "  pushq %r13\n"
    "  pushq %r14\n"
    "  pushq %r15\n"
    "  subq $8, %rsp\n"
    "  stmxcsr (%rsp)\n"
    "  fnstcw 4(%rsp)\n"
    "  movq %rsp, (%rdi)\n"
    "  movq %rsi, %rsp\n"
    "  ldmxcsr (%rsp)\n"
    "  fldcw 4(%rsp)\n"
    "  addq $8, %rsp\n"
    "  popq %r15\n"
    "  popq %r14\n"
    "  popq %r13\n"
    "  popq %r12\n"
    "  popq %rbx\n"
    "  popq %rbp\n"
    "  ret\n"
    ".globl _silk_macos_fiber_entry\n"
    ".p2align 4\n"
    "_silk_macos_fiber_entry:\n"
    "  movq %r12, %rdi\n"
    "  callq *%r13\n"
    "  ud2\n");

static void *prepare_stack(char *top, void (*function)(void *), void *argument) {
    // From the saved stack pointer up: mxcsr and x87 control word, r15, r14, r13, r12, rbx,
    // rbp, return address. After `ret` the stack is 16-byte aligned, so the call in
    // silk_macos_fiber_entry enters `function` with the alignment the ABI expects.
    uint64_t *frame = (uint64_t *)(top - 16 - 64);
    memset(frame, 0, 64);
    uint32_t *control = (uint32_t *)frame;
    control[0] = 0x1F80;                         // mxcsr default
    control[1] = 0x037F;                         // x87 control word default
    frame[3] = (uint64_t)function;               // r13
    frame[4] = (uint64_t)argument;               // r12
    frame[7] = (uint64_t)silk_macos_fiber_entry; // return address
    return frame;
}
#else
#error "unsupported architecture"
#endif

enum { FIBER_STACK = 8 * 1024 * 1024 };

// ---------------------------------------------------------------------------------------
// State. Everything lives for the rest of the process once started.

@class SilkWindow;

@interface SilkQueued : NSObject {
@public
    SilkWindowEvent event;
}
@property(nonatomic, strong) NSArray<NSData *> *texts;
@end

@implementation SilkQueued
@end

static BOOL started;
static BOOL fiberMode;
static BOOL inFiber;
static BOOL topLevelWait;
static BOOL pollWait;
static BOOL timedOut;
// Set by silk_macos_wake from any thread; the loop returns to Silk when it sees it.
static atomic_int woken;
static void *userStack;
static void *appStack;
static NSMutableArray<SilkQueued *> *queue;
static NSArray<NSData *> *currentTexts;
static NSMutableDictionary<NSNumber *, SilkWindow *> *windows;
static NSMutableArray<NSWindow *> *closing;
static uint32_t nextWindow = 1;
static uint32_t nextDialog = 1;
static CFRunLoopTimerRef deadlineTimer;
static double deadline;
static NSData *clipboardText;
static NSMutableArray<NSMenu *> *menuStack;
static NSMenu *pendingBar;
static id menuTarget;
// Between tray_begin and tray_commit, items go straight into the tray's menu.
static BOOL trayBuilding;
static uint32_t lastAppearance = 2;

static SilkWindowEvent blank_event(uint32_t kind, uint32_t window) {
    SilkWindowEvent event;
    memset(&event, 0, sizeof event);
    event.kind = kind;
    event.window = window;
    return event;
}

static NSData *utf8(NSString *string) {
    NSData *data = [string dataUsingEncoding:NSUTF8StringEncoding];
    return data ? data : [NSData data];
}

static NSString *string_from(const uint8_t *bytes, size_t length) {
    if (!bytes || length == 0) return @"";
    NSString *string = [[NSString alloc] initWithBytes:bytes length:length
                                              encoding:NSUTF8StringEncoding];
    return string ? string : @"";
}

static NSUInteger push_texts(SilkWindowEvent event, NSArray<NSData *> *texts) {
    SilkQueued *queued = [SilkQueued new];
    queued->event = event;
    queued->event.count = (uint32_t)texts.count;
    queued.texts = texts;
    [queue addObject:queued];
    return queue.count - 1;
}

static NSUInteger push(SilkWindowEvent event) { return push_texts(event, @[]); }

// Returns to Silk when events are waiting, if AppKit is running on its fiber. Called at the
// end of every handler, wherever in AppKit's loops it runs.
static void flush(void) {
    if (!fiberMode || !inFiber || queue.count == 0) return;
    inFiber = NO;
    silk_macos_switch(&appStack, userStack);
    inFiber = YES;
}

static uint32_t modifier_bits(NSEventModifierFlags flags) {
    uint32_t bits = 0;
    if (flags & NSEventModifierFlagShift) bits |= 1;
    if (flags & NSEventModifierFlagControl) bits |= 2;
    if (flags & NSEventModifierFlagOption) bits |= 4;
    if (flags & NSEventModifierFlagCommand) bits |= 8;
    if (flags & NSEventModifierFlagCapsLock) bits |= 16;
    if (flags & NSEventModifierFlagFunction) bits |= 32;
    return bits;
}

static NSEventModifierFlags modifier_flags(uint32_t bits) {
    NSEventModifierFlags flags = 0;
    if (bits & 1) flags |= NSEventModifierFlagShift;
    if (bits & 2) flags |= NSEventModifierFlagControl;
    if (bits & 4) flags |= NSEventModifierFlagOption;
    if (bits & 8) flags |= NSEventModifierFlagCommand;
    return flags;
}

static uint32_t phase_value(NSEventPhase phase, NSEventPhase momentum) {
    // event.Phase: None, Began, Changed, Ended, Cancelled, Momentum, MomentumEnded.
    if (momentum != NSEventPhaseNone) {
        return (momentum & (NSEventPhaseEnded | NSEventPhaseCancelled)) ? 6 : 5;
    }
    if (phase & (NSEventPhaseBegan | NSEventPhaseMayBegin)) return 1;
    if (phase & NSEventPhaseChanged) return 2;
    if (phase & NSEventPhaseEnded) return 3;
    if (phase & NSEventPhaseCancelled) return 4;
    return 0;
}

static BOOL is_text(NSString *text) {
    if (text.length == 0) return NO;
    unichar first = [text characterAtIndex:0];
    if (first < 0x20 || first == 0x7F) return NO;
    if (first >= 0xF700 && first <= 0xF8FF) return NO; // AppKit function-key characters
    return YES;
}

static uint32_t current_appearance(void) {
    NSAppearanceName name = [NSApp.effectiveAppearance
        bestMatchFromAppearancesWithNames:@[ NSAppearanceNameAqua, NSAppearanceNameDarkAqua ]];
    return [name isEqualToString:NSAppearanceNameDarkAqua] ? 1 : 0;
}

static void check_appearance(void) {
    uint32_t appearance = current_appearance();
    if (appearance == lastAppearance) return;
    lastAppearance = appearance;
    SilkWindowEvent event = blank_event(SILK_EVENT_APPEARANCE_CHANGED, 0);
    event.code = appearance;
    push(event);
}

// ---------------------------------------------------------------------------------------
// Windows.

@class SilkAXNode;
@interface SilkView : NSView <NSTextInputClient, NSDraggingDestination>
@property(nonatomic) uint32_t windowId;
@property(nonatomic) BOOL imeEnabled;
@property(nonatomic) NSRect imeArea;
@property(nonatomic) BOOL frameRequested;
@property(nonatomic, strong) NSCursor *cursor;
@property(nonatomic, strong) NSMutableAttributedString *marked;
@property(nonatomic) NSRange markedSelection;
@property(nonatomic) BOOL imeTouched;
@property(nonatomic, strong) CADisplayLink *displayLink;
@property(nonatomic, strong) NSTrackingArea *tracking;
@property(nonatomic) BOOL handlingKey;
@property(nonatomic, strong) NSEvent *pressed;
@property(nonatomic, strong) NSMutableDictionary<NSNumber *, SilkAXNode *> *axNodes;
@property(nonatomic, strong) NSMutableSet<NSNumber *> *axSeen;
@property(nonatomic, strong) NSMutableArray<NSNumber *> *axOrder;
@property(nonatomic, strong) NSDictionary<NSNumber *, NSArray<SilkAXNode *> *> *axChildren;
@property(nonatomic, strong) NSArray<SilkAXNode *> *axRoots;
@property(nonatomic, strong) NSMutableArray *axActions;
@property(nonatomic, strong) NSDictionary *axAction;
@property(nonatomic) uint32_t axFocused;
@property(nonatomic) uint32_t axPreviousFocus;
@property(nonatomic, strong) NSMutableDictionary<id, NSNumber *> *touchIds;
@property(nonatomic) uint32_t nextTouchId;
@end

@interface SilkNSWindow : NSWindow
@end

@interface SilkWindow : NSObject <NSWindowDelegate>
@property(nonatomic) uint32_t windowId;
@property(nonatomic, strong) SilkNSWindow *window;
@property(nonatomic, strong) SilkView *view;
// The height of the app's own bar the window controls are centred in, 0 for AppKit's place.
@property(nonatomic) double controlsHeight;
// The controls' gaps from the left and top edges; 0 for the centred place.
@property(nonatomic) double controlsLeft;
@property(nonatomic) double controlsTop;
@end

static SilkWindow *find_window(uint32_t window) { return windows[@(window)]; }

// Retained accessibility objects keep their identities across layout updates.
@interface SilkAXNode : NSAccessibilityElement
@property(nonatomic, weak) SilkView *owner;
@property(nonatomic) uint32_t nodeId, parentId, roleCode, flags;
@property(nonatomic) BOOL hasRange;
@property(nonatomic) double rangeMin, rangeMax, rangeValue;
@property(nonatomic) NSRect localFrame;
@property(nonatomic, copy) NSString *labelText, *valueText;
@end

static void ax_action(SilkAXNode *node, uint32_t kind, NSString *value) {
    SilkView *view = node.owner;
    if (!view || (node.flags & 1)) return;
    if (!view.axActions) view.axActions = [NSMutableArray new];
    [view.axActions addObject:@{@"kind": @(kind), @"target": @(node.nodeId), @"text": value ?: @""}];
    push(blank_event(SILK_EVENT_REQUEST_FRAME, view.windowId));
}
static NSString *ax_role(uint32_t role) {
    switch (role) {
    case 2: return NSAccessibilityButtonRole;
    case 3: return NSAccessibilityLinkRole;
    case 4: case 24: return NSAccessibilityCheckBoxRole;
    case 5: return NSAccessibilityRadioButtonRole;
    case 6: return NSAccessibilityTextFieldRole;
    case 7: if (@available(macOS 26.0, *)) return NSAccessibilityHeadingRole;
        return NSAccessibilityStaticTextRole;
    case 8: return NSAccessibilityStaticTextRole;
    case 9: case 25: return NSAccessibilityListRole;
    case 10: case 26: return NSAccessibilityRowRole;
    case 11: return NSAccessibilityImageRole;
    case 13: return NSAccessibilityMenuRole;
    case 14: return NSAccessibilityMenuItemRole;
    case 15: return NSAccessibilityRadioButtonRole;
    case 16: return NSAccessibilityTabGroupRole;
    case 18: return NSAccessibilityToolbarRole;
    case 23: return NSAccessibilitySliderRole;
    default: return NSAccessibilityGroupRole;
    }
}
@implementation SilkAXNode
- (BOOL)isAccessibilityElement { return self.roleCode != 0 && self.roleCode != 1; }
- (NSString *)accessibilityRole { return ax_role(self.roleCode); }
- (NSString *)accessibilitySubrole { return self.roleCode == 12 ? NSAccessibilityDialogSubrole : nil; }
- (id)accessibilityMinValue { return self.hasRange ? @(self.rangeMin) : nil; }
- (id)accessibilityMaxValue { return self.hasRange ? @(self.rangeMax) : nil; }
- (NSString *)accessibilityLabel { return self.labelText; }
- (NSString *)accessibilityIdentifier { return [NSString stringWithFormat:@"silk-%u", self.nodeId]; }
- (id)accessibilityParent { return self.owner.axNodes[@(self.parentId)] ?: self.owner; }
- (NSArray *)accessibilityChildren {
    return NSAccessibilityUnignoredChildren(self.owner.axChildren[@(self.nodeId)] ?: @[]);
}
- (NSRect)accessibilityFrame {
    SilkView *view = self.owner;
    return [view.window convertRectToScreen:[view convertRect:self.localFrame toView:nil]];
}
- (BOOL)isAccessibilityEnabled { return !(self.flags & 1); }
- (BOOL)isAccessibilityFocused { return (self.flags & 2) != 0; }
- (BOOL)isAccessibilitySelected { return (self.flags & 8) != 0; }
- (BOOL)isAccessibilityExpanded { return (self.flags & 16) != 0; }
- (id)accessibilityValue {
    if (self.roleCode == 4 || self.roleCode == 5 || self.roleCode == 24) return @((self.flags & 4) != 0);
    return self.hasRange ? @(self.rangeValue) : self.valueText;
}
- (void)setAccessibilityFocused:(BOOL)focused { if (focused) ax_action(self, 2, nil); }
- (void)setAccessibilityValue:(id)value {
    if (self.roleCode == 6 && !(self.flags & 32)) ax_action(self, 3, [value description]);
}
- (BOOL)accessibilityPerformPress { ax_action(self, 1, nil); return !(self.flags & 1); }
- (BOOL)accessibilityPerformIncrement { ax_action(self, 4, nil); return !(self.flags & 1); }
- (BOOL)accessibilityPerformDecrement { ax_action(self, 5, nil); return !(self.flags & 1); }
- (BOOL)isAccessibilitySelectorAllowed:(SEL)selector {
    if (selector == @selector(accessibilityPerformPress))
        return !(self.flags & 1) && (self.flags & 64);
    if (selector == @selector(setAccessibilityValue:)) return self.roleCode == 6 && !(self.flags & 33);
    if (selector == @selector(accessibilityPerformIncrement) || selector == @selector(accessibilityPerformDecrement))
        return self.roleCode == 23 && !(self.flags & 1);
    return [super isAccessibilitySelectorAllowed:selector];
}
@end

@implementation SilkNSWindow
- (BOOL)canBecomeKeyWindow { return YES; }
- (BOOL)canBecomeMainWindow { return YES; }
@end

static void push_frame(SilkView *view, double targetTime) {
    view.frameRequested = NO;
    view.displayLink.paused = YES;
    SilkWindowEvent event = blank_event(SILK_EVENT_REQUEST_FRAME, view.windowId);
    event.value = targetTime;
    push(event);
}

static void push_resize(SilkView *view) {
    NSSize size = view.bounds.size;
    double scale = view.window ? view.window.backingScaleFactor : 1.0;
    CAMetalLayer *layer = (CAMetalLayer *)view.layer;
    if ([layer isKindOfClass:[CAMetalLayer class]]) {
        layer.contentsScale = scale;
        layer.drawableSize = CGSizeMake(size.width * scale, size.height * scale);
    }
    SilkWindowEvent event = blank_event(SILK_EVENT_RESIZED, view.windowId);
    event.x = size.width;
    event.y = size.height;
    event.value = scale;
    push(event);
    push_frame(view, CACurrentMediaTime());
}

@implementation SilkView
- (BOOL)isAccessibilityElement { return NO; }
- (NSArray *)accessibilityChildren {
    return NSAccessibilityUnignoredChildren(self.axRoots ?: @[]);
}
- (id)accessibilityFocusedUIElement { return self.axNodes[@(self.axFocused)] ?: self; }
- (id)accessibilityHitTest:(NSPoint)point {
    NSArray *keys = self.axOrder;
    for (NSNumber *key in [keys reverseObjectEnumerator]) { SilkAXNode *node = self.axNodes[key];
        if ([node isAccessibilityElement] && NSPointInRect(point, [node accessibilityFrame])) return node; }
    return self;
}


- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.wantsLayer = YES;
        self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawDuringViewResize;
        self.cursor = [NSCursor arrowCursor];
        self.marked = [NSMutableAttributedString new];
        self.markedSelection = NSMakeRange(0, 0);
        // Fingers on a touchscreen arrive as direct touches; the trackpad's stay gestures.
        self.allowedTouchTypes = NSTouchTypeMaskDirect;
        self.touchIds = [NSMutableDictionary new];
        self.nextTouchId = 1;
    }
    return self;
}

- (CALayer *)makeBackingLayer {
    CAMetalLayer *layer = [CAMetalLayer layer];
    layer.contentsGravity = kCAGravityTopLeft;
    layer.needsDisplayOnBoundsChange = YES;
    return layer;
}

- (BOOL)wantsUpdateLayer { return YES; }
- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent *)event { return YES; }
- (BOOL)mouseDownCanMoveWindow { return NO; }

- (void)updateLayer {
    // AppKit asks for contents, for example when the window is first shown. This runs
    // inside Core Animation's commit, so the frame waits for the next switch to Silk
    // rather than being drawn from here.
    if (!self.frameRequested) push_frame(self, CACurrentMediaTime());
}

- (void)viewDidMoveToWindow {
    [super viewDidMoveToWindow];
    if (!self.window) return;
    if (!self.displayLink) {
        self.displayLink = [self displayLinkWithTarget:self selector:@selector(displayLinkFired:)];
        self.displayLink.paused = YES;
        [self.displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
    }
}

// SILK_WINDOW_IGNORE_OCCLUSION=1 keeps frames flowing while the system reports the window as
// covered, as a browser does with occlusion throttling turned off. macOS can report a window on
// screen as not visible for a few seconds; a benchmark that drives the window sets this so those
// seconds do not count as dropped frames.
static BOOL ignore_occlusion(void) {
    static int ignore = -1;
    if (ignore < 0) {
        const char *value = getenv("SILK_WINDOW_IGNORE_OCCLUSION");
        ignore = value && value[0] && strcmp(value, "0") != 0;
    }
    return ignore;
}

static BOOL window_visible(NSWindow *window) {
    if (!window) return NO;
    if (ignore_occlusion()) return window.isVisible && !window.isMiniaturized;
    return (window.occlusionState & NSWindowOcclusionStateVisible) != 0;
}

- (void)displayLinkFired:(CADisplayLink *)link {
    // A covered or off-screen window keeps its request and draws when it shows again
    // (windowDidChangeOcclusionState: resumes the link).
    if (!self.frameRequested || !window_visible(self.window)) {
        link.paused = YES;
        return;
    }
    push_frame(self, link.targetTimestamp);
    flush();
}

- (void)setFrameSize:(NSSize)size {
    BOOL changed = !NSEqualSizes(size, self.frame.size);
    [super setFrameSize:size];
    if (!changed || !self.window) return;
    push_resize(self);
    flush();
}

- (void)viewDidChangeBackingProperties {
    [super viewDidChangeBackingProperties];
    if (!self.window) return;
    SilkWindowEvent event = blank_event(SILK_EVENT_SCALE_FACTOR_CHANGED, self.windowId);
    event.value = self.window.backingScaleFactor;
    push(event);
    push_resize(self);
    flush();
}

- (void)viewDidChangeEffectiveAppearance {
    [super viewDidChangeEffectiveAppearance];
    check_appearance();
    flush();
}

- (void)updateTrackingAreas {
    if (self.tracking) [self removeTrackingArea:self.tracking];
    self.tracking = [[NSTrackingArea alloc]
        initWithRect:NSZeroRect
             options:NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
                     NSTrackingCursorUpdate | NSTrackingActiveAlways | NSTrackingInVisibleRect
               owner:self
            userInfo:nil];
    [self addTrackingArea:self.tracking];
    [super updateTrackingAreas];
}

// Whether `windowPoint` hits a native subview (such as a WKWebView) rather than the Silk view.
static BOOL pointer_over_subview(SilkView *view, NSPoint windowPoint) {
    if (!view || view.subviews.count == 0) return NO;
    NSPoint inSuper = view.superview ? [view.superview convertPoint:windowPoint fromView:nil] : windowPoint;
    NSView *hit = [view hitTest:inSuper];
    return hit != nil && hit != view;
}

- (void)cursorUpdate:(NSEvent *)event {
    if (pointer_over_subview(self, event.locationInWindow)) return;
    [self.cursor set];
}

// Files dragged in from other apps.

static NSArray<NSData *> *dragged_paths(id<NSDraggingInfo> info) {
    NSArray<NSURL *> *urls =
        [info.draggingPasteboard readObjectsForClasses:@[ NSURL.class ]
                                               options:@{NSPasteboardURLReadingFileURLsOnlyKey : @YES}];
    NSMutableArray<NSData *> *paths = [NSMutableArray array];
    for (NSURL *url in urls)
        if (url.fileURL && url.path) [paths addObject:utf8(url.path)];
    return paths;
}

- (void)pushDrag:(uint32_t)kind info:(id<NSDraggingInfo>)info paths:(NSArray<NSData *> *)paths {
    SilkWindowEvent event = blank_event(kind, self.windowId);
    if (info) {
        NSPoint position = [self convertPoint:info.draggingLocation fromView:nil];
        event.x = position.x;
        event.y = position.y;
    }
    push_texts(event, paths ?: @[]);
    flush();
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info {
    NSArray<NSData *> *paths = dragged_paths(info);
    if (paths.count == 0) return NSDragOperationNone;
    [self pushDrag:SILK_EVENT_FILE_DRAG_ENTERED info:info paths:paths];
    return NSDragOperationCopy;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)info {
    [self pushDrag:SILK_EVENT_FILE_DRAG_MOVED info:info paths:nil];
    return NSDragOperationCopy;
}

- (void)draggingExited:(id<NSDraggingInfo>)info {
    [self pushDrag:SILK_EVENT_FILE_DRAG_LEFT info:nil paths:nil];
}

- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)info { return YES; }

- (BOOL)performDragOperation:(id<NSDraggingInfo>)info {
    NSArray<NSData *> *paths = dragged_paths(info);
    if (paths.count == 0) return NO;
    [self pushDrag:SILK_EVENT_FILE_DROPPED info:info paths:paths];
    return YES;
}

// Pointer.

- (NSPoint)positionOf:(NSEvent *)event {
    return [self convertPoint:event.locationInWindow fromView:nil];
}

- (void)pushMouse:(uint32_t)kind event:(NSEvent *)event button:(uint32_t)button {
    NSPoint position = [self positionOf:event];
    SilkWindowEvent out = blank_event(kind, self.windowId);
    out.code = button;
    out.x = position.x;
    out.y = position.y;
    out.modifiers = modifier_bits(event.modifierFlags);
    if (kind == SILK_EVENT_MOUSE_DOWN) out.count = (uint32_t)MAX(event.clickCount, 1);
    SilkQueued *queued = [SilkQueued new];
    queued->event = out;
    queued.texts = @[];
    [queue addObject:queued];
    flush();
}

static uint32_t other_button(NSEvent *event) {
    // event.MouseButton: Left, Right, Middle, Back, Forward, Other.
    switch (event.buttonNumber) {
    case 2: return 2;
    case 3: return 3;
    case 4: return 4;
    default: return 5;
    }
}

// Touchscreen fingers. The tree makes pointer and touch events from them, so the mouse events
// AppKit makes from the same fingers are dropped while one is down.

- (BOOL)fingerMouse:(NSEvent *)e {
    return self.touchIds.count > 0 && e.subtype == NSEventSubtypeTouch;
}

- (void)pushTouches:(NSEvent *)e phase:(NSTouchPhase)phase value:(uint32_t)value {
    for (NSTouch *touch in [e touchesMatchingPhase:phase inView:self]) {
        if (touch.type != NSTouchTypeDirect) continue;
        NSNumber *known = self.touchIds[touch.identity];
        if (!known) {
            if (value != 1) continue;
            known = @(self.nextTouchId);
            self.nextTouchId = self.nextTouchId == UINT32_MAX ? 1 : self.nextTouchId + 1;
            self.touchIds[touch.identity] = known;
        }
        NSPoint position = [touch locationInView:self];
        SilkWindowEvent out = blank_event(SILK_EVENT_TOUCH, self.windowId);
        out.code = known.unsignedIntValue;
        out.phase = value;
        out.x = position.x;
        out.y = position.y;
        out.modifiers = modifier_bits(e.modifierFlags);
        push(out);
        if (value == 3 || value == 4) [self.touchIds removeObjectForKey:touch.identity];
    }
    flush();
}

- (void)touchesBeganWithEvent:(NSEvent *)e { [self pushTouches:e phase:NSTouchPhaseBegan value:1]; }
- (void)touchesMovedWithEvent:(NSEvent *)e { [self pushTouches:e phase:NSTouchPhaseMoved value:2]; }
- (void)touchesEndedWithEvent:(NSEvent *)e { [self pushTouches:e phase:NSTouchPhaseEnded value:3]; }
- (void)touchesCancelledWithEvent:(NSEvent *)e { [self pushTouches:e phase:NSTouchPhaseCancelled value:4]; }

- (void)mouseDown:(NSEvent *)e {
    if ([self fingerMouse:e]) return;
    self.pressed = e;
    [self pushMouse:SILK_EVENT_MOUSE_DOWN event:e button:0];
}
- (void)mouseUp:(NSEvent *)e {
    if ([self fingerMouse:e]) return;
    self.pressed = nil;
    [self pushMouse:SILK_EVENT_MOUSE_UP event:e button:0];
}
- (void)rightMouseDown:(NSEvent *)e { [self pushMouse:SILK_EVENT_MOUSE_DOWN event:e button:1]; }
- (void)rightMouseUp:(NSEvent *)e { [self pushMouse:SILK_EVENT_MOUSE_UP event:e button:1]; }
- (void)otherMouseDown:(NSEvent *)e {
    [self pushMouse:SILK_EVENT_MOUSE_DOWN event:e button:other_button(e)];
}
- (void)otherMouseUp:(NSEvent *)e {
    [self pushMouse:SILK_EVENT_MOUSE_UP event:e button:other_button(e)];
}
- (void)mouseMoved:(NSEvent *)e {
    // The tracking area is active even while another app is in front, so a window shows
    // hover like other Mac windows do. AppKit also sends the moves while another app's
    // window covers this one; those are dropped unless this window is the one under the
    // pointer. Moves over a native child view (e.g. WKWebView) belong to that view.
    NSInteger top = [NSWindow windowNumberAtPoint:[NSEvent mouseLocation] belowWindowWithWindowNumber:0];
    if (top != self.window.windowNumber) return;
    if (pointer_over_subview(self, e.locationInWindow)) return;
    [self pushMouse:SILK_EVENT_MOUSE_MOVED event:e button:0];
}
- (void)mouseDragged:(NSEvent *)e {
    if ([self fingerMouse:e]) return;
    [self pushMouse:SILK_EVENT_MOUSE_MOVED event:e button:0];
}
- (void)rightMouseDragged:(NSEvent *)e { [self pushMouse:SILK_EVENT_MOUSE_MOVED event:e button:0]; }
- (void)otherMouseDragged:(NSEvent *)e { [self pushMouse:SILK_EVENT_MOUSE_MOVED event:e button:0]; }
- (void)mouseEntered:(NSEvent *)e { [self pushMouse:SILK_EVENT_MOUSE_ENTERED event:e button:0]; }
- (void)mouseExited:(NSEvent *)e { [self pushMouse:SILK_EVENT_MOUSE_EXITED event:e button:0]; }

- (void)scrollWheel:(NSEvent *)e {
    NSPoint position = [self positionOf:e];
    SilkWindowEvent out = blank_event(SILK_EVENT_SCROLL, self.windowId);
    out.x = position.x;
    out.y = position.y;
    out.dx = e.scrollingDeltaX;
    out.dy = e.scrollingDeltaY;
    out.flags = e.hasPreciseScrollingDeltas ? SILK_FLAG_PRECISE : 0;
    out.phase = phase_value(e.phase, e.momentumPhase);
    out.modifiers = modifier_bits(e.modifierFlags);
    push(out);
    flush();
}

- (void)magnifyWithEvent:(NSEvent *)e {
    NSPoint position = [self positionOf:e];
    SilkWindowEvent out = blank_event(SILK_EVENT_PINCH, self.windowId);
    out.x = position.x;
    out.y = position.y;
    out.dx = e.magnification;
    out.phase = phase_value(e.phase, NSEventPhaseNone);
    push(out);
    flush();
}

// Keyboard.

static void fill_key(SilkWindowEvent *out, NSEvent *e, NSMutableArray<NSData *> *texts) {
    uint16_t vk = e.keyCode;
    uint32_t code = vk < 128 ? silk_macos_key_codes[vk] : SILK_CODE_Unidentified;
    out->code = code;
    out->named = silk_code_named_keys[code];
    out->modifiers = modifier_bits(e.modifierFlags);
    NSString *text = @"";
    if (out->named == SILK_NAMED_Unidentified) {
        NSString *characters = e.charactersIgnoringModifiers;
        if (is_text(characters)) text = characters;
    }
    [texts addObject:utf8(text)];
}

// The Edit menu's items (menu.SystemItem Undo to SelectAll) reach the app as edit commands
// when chosen with the mouse. Their shortcuts come here first and go on as KeyDown, as on the
// other systems, so the app sees the key before the command it stands for.
- (void)sendEditCommand:(uint32_t)command {
    SilkWindowEvent out = blank_event(SILK_EVENT_EDIT_COMMAND, self.windowId);
    out.code = command;
    push(out);
    flush();
}
- (void)undo:(id)sender { [self sendEditCommand:0]; }
- (void)redo:(id)sender { [self sendEditCommand:1]; }
- (void)cut:(id)sender { [self sendEditCommand:2]; }
- (void)copy:(id)sender { [self sendEditCommand:3]; }
- (void)paste:(id)sender { [self sendEditCommand:4]; }
- (void)selectAll:(id)sender { [self sendEditCommand:5]; }

- (BOOL)performKeyEquivalent:(NSEvent *)e {
    NSEventModifierFlags held = e.modifierFlags & (NSEventModifierFlagCommand | NSEventModifierFlagControl
        | NSEventModifierFlagOption | NSEventModifierFlagShift);
    NSString *key = e.charactersIgnoringModifiers.lowercaseString;
    BOOL editing = self.window.firstResponder == self && (held & NSEventModifierFlagCommand)
        && !(held & (NSEventModifierFlagControl | NSEventModifierFlagOption))
        && ([key isEqualToString:@"z"] || [key isEqualToString:@"x"] || [key isEqualToString:@"c"]
            || [key isEqualToString:@"v"] || [key isEqualToString:@"a"]);
    if (!editing) return [super performKeyEquivalent:e];
    [self keyDown:e];
    return YES;
}

- (void)keyDown:(NSEvent *)e {
    SilkWindowEvent out = blank_event(SILK_EVENT_KEY_DOWN, self.windowId);
    NSMutableArray<NSData *> *texts = [NSMutableArray array];
    fill_key(&out, e, texts);
    if (e.isARepeat) out.flags |= SILK_FLAG_REPEAT;
    NSUInteger index = push_texts(out, texts);
    BOOL hadMarked = self.marked.length > 0;
    BOOL shortcut = (e.modifierFlags & (NSEventModifierFlagCommand | NSEventModifierFlagControl)) != 0;
    self.imeTouched = NO;
    self.handlingKey = YES;
    if (self.imeEnabled && (!shortcut || hadMarked)) {
        [self.inputContext handleEvent:e];
    } else if (!shortcut && is_text(e.characters)) {
        SilkWindowEvent input = blank_event(SILK_EVENT_TEXT_INPUT, self.windowId);
        push_texts(input, @[ utf8(e.characters) ]);
    }
    self.handlingKey = NO;
    if (hadMarked || self.imeTouched || self.marked.length > 0) {
        queue[index]->event.flags |= SILK_FLAG_IME_HANDLED;
    }
    flush();
}

- (void)keyUp:(NSEvent *)e {
    SilkWindowEvent out = blank_event(SILK_EVENT_KEY_UP, self.windowId);
    NSMutableArray<NSData *> *texts = [NSMutableArray array];
    fill_key(&out, e, texts);
    push_texts(out, texts);
    flush();
}

- (void)flagsChanged:(NSEvent *)e {
    SilkWindowEvent out = blank_event(SILK_EVENT_MODIFIERS_CHANGED, self.windowId);
    out.modifiers = modifier_bits(e.modifierFlags);
    push(out);
    flush();
}

// NSTextInputClient. Silk keeps the text; the view keeps only the composition and the
// caret rectangle the app last reported, which is all the input method needs.

static NSString *plain(id string) {
    return [string isKindOfClass:[NSAttributedString class]] ? [string string] : (NSString *)string;
}

// The UTF-8 byte offset of a UTF-16 offset, moved back to the start of the character it
// falls inside so it never splits a surrogate pair.
static uint64_t utf8_offset(NSString *text, NSUInteger utf16) {
    if (utf16 == NSNotFound || utf16 >= text.length) {
        return [text lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    }
    NSRange composed = [text rangeOfComposedCharacterSequenceAtIndex:utf16];
    if (composed.location < utf16) utf16 = composed.location;
    return [[text substringToIndex:utf16] lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
}

- (void)pushPreedit {
    NSString *text = self.marked.string;
    SilkWindowEvent out = blank_event(SILK_EVENT_IME_PREEDIT, self.windowId);
    NSRange selection = self.markedSelection;
    out.start = utf8_offset(text, selection.location);
    out.end = utf8_offset(text, NSMaxRange(selection));
    push_texts(out, @[ utf8(text) ]);
}

- (BOOL)hasMarkedText { return self.marked.length > 0; }

- (NSRange)markedRange {
    return self.marked.length > 0 ? NSMakeRange(0, self.marked.length) : NSMakeRange(NSNotFound, 0);
}

- (NSRange)selectedRange {
    return self.marked.length > 0 ? self.markedSelection : NSMakeRange(NSNotFound, 0);
}

- (void)setMarkedText:(id)string
        selectedRange:(NSRange)selectedRange
     replacementRange:(NSRange)replacementRange {
    NSString *text = plain(string);
    self.marked = [[NSMutableAttributedString alloc] initWithString:text];
    self.markedSelection = selectedRange;
    self.imeTouched = YES;
    [self pushPreedit];
    if (!self.handlingKey) flush();
}

- (void)unmarkText {
    if (self.marked.length == 0) return;
    self.marked = [NSMutableAttributedString new];
    self.markedSelection = NSMakeRange(0, 0);
    [self pushPreedit];
    [self.inputContext discardMarkedText];
    if (!self.handlingKey) flush();
}

- (NSArray<NSAttributedStringKey> *)validAttributesForMarkedText { return @[]; }

- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range
                                                actualRange:(NSRangePointer)actualRange {
    return nil;
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
    NSString *text = plain(string);
    if (self.marked.length > 0) {
        self.marked = [NSMutableAttributedString new];
        self.markedSelection = NSMakeRange(0, 0);
        [self pushPreedit];
        self.imeTouched = YES;
    }
    if (is_text(text)) {
        SilkWindowEvent input = blank_event(SILK_EVENT_TEXT_INPUT, self.windowId);
        push_texts(input, @[ utf8(text) ]);
    }
    if (!self.handlingKey) flush();
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point { return NSNotFound; }

- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange {
    if (actualRange) *actualRange = range;
    if (!self.window) return NSZeroRect;
    NSRect inWindow = [self convertRect:self.imeArea toView:nil];
    return [self.window convertRectToScreen:inWindow];
}

- (void)doCommandBySelector:(SEL)selector {
    // Editing commands reach the app as KeyDown; swallowing them here avoids the beep.
}

@end

static NSPoint top_left(NSWindow *window) {
    NSRect frame = window.frame;
    CGFloat top = NSMaxY(NSScreen.screens.firstObject.frame);
    return NSMakePoint(frame.origin.x, top - NSMaxY(frame));
}

// Places the window controls in the app's bar, centred unless the app gave gaps. AppKit lays the title bar out again whenever
// the window changes size or leaves full screen, so this runs after each.
static void place_controls(SilkWindow *record) {
    double height = record.controlsHeight;
    NSWindow *window = record.window;
    if (height <= 0 || (window.styleMask & NSWindowStyleMaskFullScreen)) return;
    NSButton *close = [window standardWindowButton:NSWindowCloseButton];
    NSButton *minimize = [window standardWindowButton:NSWindowMiniaturizeButton];
    NSButton *zoom = [window standardWindowButton:NSWindowZoomButton];
    NSView *container = close.superview.superview;
    if (!close || !minimize || !zoom || !container) return;
    // The controls are clipped to the title bar's container, which grows to the bar.
    NSRect bar = container.frame;
    bar.size.height = height;
    bar.origin.y = window.frame.size.height - height;
    container.frame = bar;
    double pitch = minimize.frame.origin.x - close.frame.origin.x;
    double top = record.controlsTop > 0 ? record.controlsTop : (height - close.frame.size.height) / 2;
    double left = record.controlsLeft > 0 ? record.controlsLeft : top;
    // The container's origin is at its bottom.
    double y = height - top - close.frame.size.height;
    NSArray<NSButton *> *controls = @[ close, minimize, zoom ];
    for (NSUInteger i = 0; i < controls.count; i++) {
        [controls[i] setFrameOrigin:NSMakePoint(left + pitch * i, y)];
    }
}

@implementation SilkWindow

- (void)windowDidResize:(NSNotification *)notification {
    place_controls(self);
}

- (BOOL)windowShouldClose:(NSWindow *)sender {
    push(blank_event(SILK_EVENT_CLOSE_REQUESTED, self.windowId));
    flush();
    return NO;
}

- (void)windowDidMove:(NSNotification *)notification {
    NSPoint position = top_left(self.window);
    SilkWindowEvent event = blank_event(SILK_EVENT_MOVED, self.windowId);
    event.x = position.x;
    event.y = position.y;
    push(event);
    flush();
}

- (void)pushFlag:(uint32_t)kind on:(BOOL)on {
    SilkWindowEvent event = blank_event(kind, self.windowId);
    if (on) event.flags |= SILK_FLAG_ON;
    push(event);
    flush();
}

- (void)windowDidBecomeKey:(NSNotification *)n { [self pushFlag:SILK_EVENT_FOCUSED on:YES]; }
- (void)windowDidResignKey:(NSNotification *)n { [self pushFlag:SILK_EVENT_FOCUSED on:NO]; }
- (void)windowDidEnterFullScreen:(NSNotification *)n {
    [self pushFlag:SILK_EVENT_FULL_SCREEN_CHANGED on:YES];
}
- (void)windowDidExitFullScreen:(NSNotification *)n {
    place_controls(self);
    [self pushFlag:SILK_EVENT_FULL_SCREEN_CHANGED on:NO];
}
- (void)windowDidChangeOcclusionState:(NSNotification *)n {
    BOOL visible = window_visible(self.window);
    if (visible && self.view.frameRequested) self.view.displayLink.paused = NO;
    [self pushFlag:SILK_EVENT_OCCLUDED on:!visible];
}

@end

// ---------------------------------------------------------------------------------------
// The app, menus and the run loop.

@interface SilkAppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation SilkAppDelegate
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    push(blank_event(SILK_EVENT_QUIT_REQUESTED, 0));
    flush();
    return NSTerminateCancel;
}
@end

@interface SilkMenuTarget : NSObject
- (void)menuAction:(NSMenuItem *)item;
@end

@implementation SilkMenuTarget
- (void)menuAction:(NSMenuItem *)item {
    SilkWindowEvent event = blank_event(SILK_EVENT_MENU_ACTION, 0);
    event.code = (uint32_t)item.tag;
    push(event);
    flush();
}
@end

static void close_pending(void) {
    if (closing.count == 0) return;
    NSArray<NSWindow *> *windowsToClose = [closing copy];
    [closing removeAllObjects];
    for (NSWindow *window in windowsToClose) {
        window.delegate = nil;
        [window close];
    }
}

static void deadline_fired(CFRunLoopTimerRef timer, void *info) {
    timedOut = YES;
    // At the top level nextEventMatchingMask returns on its own at the same deadline.
    if (!topLevelWait && fiberMode && inFiber) {
        inFiber = NO;
        silk_macos_switch(&appStack, userStack);
        inFiber = YES;
    }
}

// On the main thread, in any run loop mode: ends the wait as the deadline does.
static void wake_fired(void) {
    timedOut = YES;
    if (!topLevelWait && fiberMode && inFiber) {
        inFiber = NO;
        silk_macos_switch(&appStack, userStack);
        inFiber = YES;
    }
}

void silk_macos_wake(void) {
    if (!started) return;
    if (atomic_exchange(&woken, 1)) return;
    // A block for nested loops (live resize, menus) and an event for the top-level wait.
    CFRunLoopRef loop = CFRunLoopGetMain();
    CFRunLoopPerformBlock(loop, kCFRunLoopCommonModes, ^{
        if (atomic_load(&woken)) wake_fired();
    });
    CFRunLoopWakeUp(loop);
    @autoreleasepool {
        NSEvent *event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                            location:NSZeroPoint
                                       modifierFlags:0
                                           timestamp:0
                                        windowNumber:0
                                             context:nil
                                             subtype:0x5157
                                               data1:0
                                               data2:0];
        [NSApp postEvent:event atStart:NO];
    }
}

static void arm_deadline(double timeout) {
    timedOut = NO;
    pollWait = timeout == 0;
    if (timeout < 0) {
        deadline = INFINITY;
        CFRunLoopTimerSetNextFireDate(deadlineTimer, CFAbsoluteTimeGetCurrent() + 1.0e10);
        return;
    }
    deadline = CFAbsoluteTimeGetCurrent() + timeout;
    CFRunLoopTimerSetNextFireDate(deadlineTimer, deadline);
}

static NSDate *wait_until(void) {
    if (pollWait) return [NSDate distantPast];
    if (isinf(deadline)) return [NSDate distantFuture];
    return [NSDate dateWithTimeIntervalSinceReferenceDate:deadline];
}

// One turn of the top-level loop: wait for an AppKit event and dispatch it.
static BOOL pump_one(NSDate *until) {
    topLevelWait = YES;
    NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                        untilDate:until
                                           inMode:NSDefaultRunLoopMode
                                          dequeue:YES];
    topLevelWait = NO;
    if (event) {
        [NSApp sendEvent:event];
        [NSApp updateWindows];
    }
    close_pending();
    return event != nil;
}

static void appkit_main(void *unused) {
    (void)unused;
    for (;;) {
        @autoreleasepool {
            @try {
                BOOL got = pump_one(wait_until());
                BOOL expired = (!got && (pollWait || timedOut ||
                                         CFAbsoluteTimeGetCurrent() >= deadline)) ||
                               atomic_load(&woken);
                if (queue.count > 0 || expired) {
                    inFiber = NO;
                    silk_macos_switch(&appStack, userStack);
                    inFiber = YES;
                }
            } @catch (NSException *exception) {
                NSLog(@"native/platform/window: uncaught exception in AppKit: %@", exception);
            }
        }
    }
}

// Without a fiber: dispatch what AppKit has until an event for Silk is queued.
static void pump_on_caller(void) {
    @autoreleasepool {
        NSDate *until = wait_until();
        while (queue.count == 0) {
            if (!pump_one(until) || atomic_load(&woken)) break;
            until = [NSDate distantPast];
            if (queue.count == 0 && !pollWait && !isinf(deadline) &&
                CFAbsoluteTimeGetCurrent() >= deadline) {
                break;
            }
            if (queue.count == 0 && !pollWait) until = wait_until();
        }
    }
}

static NSMenuItem *system_item(uint32_t item) {
    NSString *app = NSProcessInfo.processInfo.processName;
    NSMenuItem *menuItem = nil;
    // menu.SystemItem: About, Services, Hide, HideOthers, ShowAll, Quit, CloseWindow,
    // Minimize, Zoom, ToggleFullScreen, BringAllToFront, Undo, Redo, Cut, Copy, Paste,
    // SelectAll.
    switch (item) {
    case 0:
        menuItem = [[NSMenuItem alloc] initWithTitle:[@"About " stringByAppendingString:app]
                                              action:@selector(orderFrontStandardAboutPanel:)
                                       keyEquivalent:@""];
        break;
    case 1: {
        menuItem = [[NSMenuItem alloc] initWithTitle:@"Services" action:nil keyEquivalent:@""];
        NSMenu *services = [[NSMenu alloc] initWithTitle:@"Services"];
        menuItem.submenu = services;
        NSApp.servicesMenu = services;
        break;
    }
    case 2:
        menuItem = [[NSMenuItem alloc] initWithTitle:[@"Hide " stringByAppendingString:app]
                                              action:@selector(hide:)
                                       keyEquivalent:@"h"];
        break;
    case 3:
        menuItem = [[NSMenuItem alloc] initWithTitle:@"Hide Others"
                                              action:@selector(hideOtherApplications:)
                                       keyEquivalent:@"h"];
        menuItem.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagOption;
        break;
    case 4:
        menuItem = [[NSMenuItem alloc] initWithTitle:@"Show All"
                                              action:@selector(unhideAllApplications:)
                                       keyEquivalent:@""];
        break;
    case 5:
        menuItem = [[NSMenuItem alloc] initWithTitle:[@"Quit " stringByAppendingString:app]
                                              action:@selector(terminate:)
                                       keyEquivalent:@"q"];
        break;
    case 6:
        menuItem = [[NSMenuItem alloc] initWithTitle:@"Close Window"
                                              action:@selector(performClose:)
                                       keyEquivalent:@"w"];
        break;
    case 7:
        menuItem = [[NSMenuItem alloc] initWithTitle:@"Minimize"
                                              action:@selector(performMiniaturize:)
                                       keyEquivalent:@"m"];
        break;
    case 8:
        menuItem = [[NSMenuItem alloc] initWithTitle:@"Zoom"
                                              action:@selector(performZoom:)
                                       keyEquivalent:@""];
        break;
    case 9:
        menuItem = [[NSMenuItem alloc] initWithTitle:@"Enter Full Screen"
                                              action:@selector(toggleFullScreen:)
                                       keyEquivalent:@"f"];
        menuItem.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagControl;
        break;
    case 10:
        menuItem = [[NSMenuItem alloc] initWithTitle:@"Bring All to Front"
                                              action:@selector(arrangeInFront:)
                                       keyEquivalent:@""];
        break;
    case 11:
        menuItem = [[NSMenuItem alloc] initWithTitle:@"Undo" action:@selector(undo:) keyEquivalent:@"z"];
        break;
    case 12:
        menuItem = [[NSMenuItem alloc] initWithTitle:@"Redo" action:@selector(redo:) keyEquivalent:@"z"];
        menuItem.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
        break;
    case 13:
        menuItem = [[NSMenuItem alloc] initWithTitle:@"Cut" action:@selector(cut:) keyEquivalent:@"x"];
        break;
    case 14:
        menuItem = [[NSMenuItem alloc] initWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@"c"];
        break;
    case 15:
        menuItem = [[NSMenuItem alloc] initWithTitle:@"Paste" action:@selector(paste:) keyEquivalent:@"v"];
        break;
    case 16:
        menuItem = [[NSMenuItem alloc] initWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"];
        break;
    default:
        return nil;
    }
    return menuItem;
}

static void install_default_menu(void) {
    NSMenu *bar = [[NSMenu alloc] initWithTitle:@""];
    NSMenuItem *appItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    NSMenu *appMenu = [[NSMenu alloc] initWithTitle:@""];
    [appMenu addItem:system_item(5)];
    appItem.submenu = appMenu;
    [bar addItem:appItem];

    // Edit, so that the system's own text services and the menu commands reach the app.
    NSMenuItem *editItem = [[NSMenuItem alloc] initWithTitle:@"Edit" action:nil keyEquivalent:@""];
    NSMenu *editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
    [editMenu addItem:system_item(11)];
    [editMenu addItem:system_item(12)];
    [editMenu addItem:[NSMenuItem separatorItem]];
    for (uint32_t item = 13; item <= 16; item++) [editMenu addItem:system_item(item)];
    editItem.submenu = editMenu;
    [bar addItem:editItem];
    NSApp.mainMenu = bar;
}

int32_t silk_macos_start(void) {
    if (started) return 1;
    if (!pthread_main_np()) return 2;
    @autoreleasepool {
        [NSApplication sharedApplication];
        if (!NSApp) return 3;
        static SilkAppDelegate *delegate;
        delegate = [SilkAppDelegate new];
        NSApp.delegate = delegate;
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        queue = [NSMutableArray array];
        windows = [NSMutableDictionary dictionary];
        closing = [NSMutableArray array];
        menuStack = [NSMutableArray array];
        menuTarget = [SilkMenuTarget new];
        currentTexts = @[];
        install_default_menu();
        [NSApp finishLaunching];
        if ([NSApp respondsToSelector:@selector(activate)]) {
            [NSApp activate];
        } else {
            [NSApp activateIgnoringOtherApps:YES];
        }
        lastAppearance = current_appearance();
        deadlineTimer = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + 1.0e10, 1.0e10,
                                             0, 0, deadline_fired, NULL);
        CFRunLoopAddTimer(CFRunLoopGetMain(), deadlineTimer, kCFRunLoopCommonModes);
        const char *setting = getenv("SILK_WINDOW_FIBER");
        fiberMode = !(setting && strcmp(setting, "0") == 0);
        if (fiberMode) {
            size_t page = (size_t)getpagesize();
            char *stack = mmap(NULL, FIBER_STACK, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (stack == MAP_FAILED) {
                fiberMode = NO;
            } else {
                mprotect(stack, page, PROT_NONE); // guard page below the stack
                appStack = prepare_stack(stack + FIBER_STACK, appkit_main, NULL);
            }
        }
        started = YES;
    }
    return 0;
}

uint8_t silk_macos_next_event(double timeout, SilkWindowEvent *out) {
    if (!started) return 0;
    if (queue.count == 0) {
        arm_deadline(timeout);
        if (fiberMode) {
            inFiber = YES;
            silk_macos_switch(&userStack, appStack);
            inFiber = NO;
        } else {
            pump_on_caller();
        }
        CFRunLoopTimerSetNextFireDate(deadlineTimer, CFAbsoluteTimeGetCurrent() + 1.0e10);
    }
    atomic_store(&woken, 0);
    if (queue.count == 0) {
        currentTexts = @[];
        return 0;
    }
    SilkQueued *queued = queue.firstObject;
    [queue removeObjectAtIndex:0];
    *out = queued->event;
    currentTexts = queued.texts;
    return 1;
}

const uint8_t *silk_macos_event_text(size_t index, size_t *length) {
    if (index >= currentTexts.count) {
        *length = 0;
        return NULL;
    }
    NSData *data = currentTexts[index];
    *length = data.length;
    return data.length ? data.bytes : (const uint8_t *)"";
}

uint32_t silk_macos_open_window(const uint8_t *title, size_t titleLength, double width,
                                double height, double minWidth, double minHeight,
                                uint32_t flags) {
    if (!started) return 0;
    @autoreleasepool {
        NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                  NSWindowStyleMaskMiniaturizable;
        if (flags & SILK_WINDOW_RESIZABLE) style |= NSWindowStyleMaskResizable;
        if (flags & (SILK_WINDOW_TITLE_TRANSPARENT | SILK_WINDOW_TITLE_HIDDEN)) {
            style |= NSWindowStyleMaskFullSizeContentView;
        }
        SilkNSWindow *window =
            [[SilkNSWindow alloc] initWithContentRect:NSMakeRect(0, 0, width, height)
                                            styleMask:style
                                              backing:NSBackingStoreBuffered
                                                defer:NO];
        if (!window) return 0;
        window.releasedWhenClosed = NO;
        window.acceptsMouseMovedEvents = YES;
        window.title = string_from(title, titleLength);
        if (minWidth > 0 || minHeight > 0) window.contentMinSize = NSMakeSize(minWidth, minHeight);
        if (flags & (SILK_WINDOW_TITLE_TRANSPARENT | SILK_WINDOW_TITLE_HIDDEN)) {
            window.titlebarAppearsTransparent = YES;
            window.titleVisibility = NSWindowTitleHidden;
        }
        if (flags & SILK_WINDOW_TITLE_HIDDEN) {
            [window standardWindowButton:NSWindowCloseButton].hidden = YES;
            [window standardWindowButton:NSWindowMiniaturizeButton].hidden = YES;
            [window standardWindowButton:NSWindowZoomButton].hidden = YES;
        }
        uint32_t id = nextWindow++;
        SilkView *view = [[SilkView alloc] initWithFrame:NSMakeRect(0, 0, width, height)];
        view.windowId = id;
        BOOL transparent = (flags & SILK_WINDOW_TRANSPARENT) != 0;
        if (transparent) {
            window.opaque = NO;
            window.backgroundColor = [NSColor clearColor];
        }
        window.contentView = view;
        [view registerForDraggedTypes:@[ NSPasteboardTypeFileURL ]];
        view.layer.opaque = !transparent;
        SilkWindow *record = [SilkWindow new];
        record.windowId = id;
        record.window = window;
        record.view = view;
        window.delegate = record;
        windows[@(id)] = record;
        [window center];
        // The first size and frame, before the window is shown.
        push_resize(view);
        if (flags & SILK_WINDOW_VISIBLE) [window makeKeyAndOrderFront:nil];
        [window makeFirstResponder:view];
        return id;
    }
}

void silk_macos_close_window(uint32_t id) {
    SilkWindow *record = find_window(id);
    if (!record) return;
    [windows removeObjectForKey:@(id)];
    [record.view.displayLink invalidate];
    record.view.displayLink = nil;
    [record.window orderOut:nil];
    // AppKit may be in the middle of dispatching an event to this window on the fiber, so
    // the window is closed and released when that dispatch has returned.
    [closing addObject:record.window];
}

void silk_macos_request_frame(uint32_t id) {
    SilkView *view = find_window(id).view;
    if (!view || view.frameRequested) return;
    view.frameRequested = YES;
    view.displayLink.paused = NO;
}

void silk_macos_content_size(uint32_t id, double *width, double *height) {
    SilkView *view = find_window(id).view;
    NSSize size = view ? view.bounds.size : NSZeroSize;
    *width = size.width;
    *height = size.height;
}

double silk_macos_scale_factor(uint32_t id) {
    NSWindow *window = find_window(id).window;
    return window ? window.backingScaleFactor : 1.0;
}

void *silk_macos_metal_layer(uint32_t id) {
    SilkView *view = find_window(id).view;
    return view ? (__bridge void *)view.layer : NULL;
}

void silk_macos_set_title(uint32_t id, const uint8_t *title, size_t length) {
    @autoreleasepool {
        find_window(id).window.title = string_from(title, length);
    }
}

void silk_macos_set_content_size(uint32_t id, double width, double height) {
    @autoreleasepool {
        [find_window(id).window setContentSize:NSMakeSize(width, height)];
    }
}

void silk_macos_set_min_content_size(uint32_t id, double width, double height) {
    find_window(id).window.contentMinSize = NSMakeSize(width, height);
}

void silk_macos_set_visible(uint32_t id, uint8_t visible) {
    @autoreleasepool {
        NSWindow *window = find_window(id).window;
        if (visible) {
            [window orderFront:nil];
        } else {
            [window orderOut:nil];
        }
    }
}

void silk_macos_focus(uint32_t id) {
    @autoreleasepool {
        if ([NSApp respondsToSelector:@selector(activate)]) {
            [NSApp activate];
        } else {
            [NSApp activateIgnoringOtherApps:YES];
        }
        [find_window(id).window makeKeyAndOrderFront:nil];
    }
}

void silk_macos_minimize(uint32_t id) {
    @autoreleasepool {
        [find_window(id).window miniaturize:nil];
    }
}

void silk_macos_toggle_full_screen(uint32_t id) {
    @autoreleasepool {
        [find_window(id).window toggleFullScreen:nil];
    }
}

void silk_macos_set_controls(uint32_t id, double height, double left, double top) {
    SilkWindow *record = find_window(id);
    if (!record) return;
    record.controlsHeight = height;
    record.controlsLeft = left;
    record.controlsTop = top;
    place_controls(record);
}

void silk_macos_start_window_drag(uint32_t id) {
    @autoreleasepool {
        // The app asks after it has seen the press, often from another thread, so the press
        // is no longer AppKit's current event: the view keeps it while the button is down.
        SilkWindow *record = find_window(id);
        NSEvent *event = record.view.pressed;
        if (!event || ([NSEvent pressedMouseButtons] & 1) == 0) return;
        if (event.clickCount == 2) {
            // A double click on a title bar does what the user chose in System Settings.
            NSString *action =
                [NSUserDefaults.standardUserDefaults stringForKey:@"AppleActionOnDoubleClick"];
            if ([action isEqualToString:@"Minimize"]) {
                [record.window miniaturize:nil];
            } else if (![action isEqualToString:@"None"]) {
                [record.window zoom:nil];
            }
            return;
        }
        [record.window performWindowDragWithEvent:event];
    }
}

static NSCursor *private_cursor(NSString *name, NSCursor *fallback) {
    SEL selector = NSSelectorFromString(name);
    if ([NSCursor respondsToSelector:selector]) {
        NSCursor *(*send)(id, SEL) = (NSCursor * (*)(id, SEL)) objc_msgSend;
        NSCursor *cursor = send([NSCursor class], selector);
        if (cursor) return cursor;
    }
    return fallback;
}

static NSCursor *hidden_cursor(void) {
    static NSCursor *cursor;
    if (!cursor) {
        NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(1, 1)];
        cursor = [[NSCursor alloc] initWithImage:image hotSpot:NSZeroPoint];
    }
    return cursor;
}

// A cursor from `pixels` (width * height RGBA, not premultiplied), `scale` pixels per point.
static NSCursor *pixel_cursor(const uint8_t *pixels, uint32_t width, uint32_t height,
                              uint32_t hotX, uint32_t hotY, double scale) {
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL
                      pixelsWide:(NSInteger)width
                      pixelsHigh:(NSInteger)height
                   bitsPerSample:8
                 samplesPerPixel:4
                        hasAlpha:YES
                        isPlanar:NO
                  colorSpaceName:NSDeviceRGBColorSpace
                    bitmapFormat:NSBitmapFormatAlphaNonpremultiplied
                     bytesPerRow:(NSInteger)width * 4
                    bitsPerPixel:32];
    if (!rep) return nil;
    memcpy(rep.bitmapData, pixels, (size_t)width * height * 4);
    if (scale <= 0) scale = 1;
    NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(width / scale, height / scale)];
    [image addRepresentation:rep];
    return [[NSCursor alloc] initWithImage:image hotSpot:NSMakePoint(hotX / scale, hotY / scale)];
}

// The bundled image of `cursor`, drawn for a 2x display, or `fallback`.
static NSCursor *bundled_cursor(uint32_t cursor, NSCursor *fallback) {
    static NSCursor *made[40];
    if (cursor < 40 && made[cursor]) return made[cursor];
    uint32_t size = SILK_CURSOR_ART * 2, hotX, hotY;
    uint8_t *rgba = malloc((size_t)size * size * 4);
    NSCursor *result = nil;
    if (rgba && silk_cursor_bundled(cursor, 2, rgba, &hotX, &hotY))
        result = pixel_cursor(rgba, size, size, hotX, hotY, 2.0);
    free(rgba);
    if (!result) return fallback;
    if (cursor < 40) made[cursor] = result;
    return result;
}

// macOS 15's frame resize cursors: `position` and `directions` are
// NSCursorFrameResizePosition and NSCursorFrameResizeDirections bits.
static NSCursor *frame_cursor(NSUInteger position, NSUInteger directions, NSCursor *fallback) {
    SEL selector = NSSelectorFromString(@"frameResizeCursorFromPosition:inDirections:");
    if ([NSCursor respondsToSelector:selector]) {
        NSCursor *(*send)(id, SEL, NSUInteger, NSUInteger) =
            (NSCursor * (*)(id, SEL, NSUInteger, NSUInteger)) objc_msgSend;
        NSCursor *cursor = send([NSCursor class], selector, position, directions);
        if (cursor) return cursor;
    }
    return fallback;
}

// macOS 15's column and row resize cursors, both directions.
static NSCursor *split_cursor(NSString *name, NSString *older, NSCursor *fallback) {
    SEL selector = NSSelectorFromString(name);
    if ([NSCursor respondsToSelector:selector]) {
        NSCursor *(*send)(id, SEL, NSUInteger) = (NSCursor * (*)(id, SEL, NSUInteger)) objc_msgSend;
        NSCursor *cursor = send([NSCursor class], selector, 3);
        if (cursor) return cursor;
    }
    return private_cursor(older, fallback);
}

// NSCursorFrameResizePosition and NSCursorFrameResizeDirections values.
enum { FRAME_TOP = 1, FRAME_LEFT = 2, FRAME_BOTTOM = 4, FRAME_RIGHT = 8 };
enum { FRAME_OUTWARD = 2, FRAME_BOTH = 3 };

static NSCursor *cursor_for(uint32_t cursor) {
    // types.Cursor, in declaration order.
    switch (cursor) {
    case 0: return [NSCursor arrowCursor];
    case 1: return [NSCursor IBeamCursor];
    case 2: return [NSCursor pointingHandCursor];
    case 3: return [NSCursor crosshairCursor];
    case 4: return [NSCursor closedHandCursor];
    case 5: return [NSCursor openHandCursor];
    case 6: return [NSCursor operationNotAllowedCursor];
    case 7: return [NSCursor resizeLeftRightCursor];
    case 8: return [NSCursor resizeUpDownCursor];
    case 9: return [NSCursor resizeLeftCursor];
    case 10: return [NSCursor resizeRightCursor];
    case 11: return [NSCursor resizeUpCursor];
    case 12: return [NSCursor resizeDownCursor];
    case 13:
        return frame_cursor(FRAME_TOP | FRAME_LEFT, FRAME_BOTH,
                            private_cursor(@"_windowResizeNorthWestSouthEastCursor", [NSCursor crosshairCursor]));
    case 14:
        return frame_cursor(FRAME_TOP | FRAME_RIGHT, FRAME_BOTH,
                            private_cursor(@"_windowResizeNorthEastSouthWestCursor", [NSCursor crosshairCursor]));
    case 15: return split_cursor(@"columnResizeCursorInDirections:", @"columnResizeCursor", [NSCursor resizeLeftRightCursor]);
    case 16: return split_cursor(@"rowResizeCursorInDirections:", @"rowResizeCursor", [NSCursor resizeUpDownCursor]);
    case 17: return [NSCursor IBeamCursorForVerticalLayout];
    case 18: return [NSCursor dragCopyCursor];
    case 19: return [NSCursor dragLinkCursor];
    case 20: return [NSCursor contextualMenuCursor];
    case 22: return private_cursor(@"busyButClickableCursor", [NSCursor arrowCursor]);
    case 23: return private_cursor(@"busyButClickableCursor", [NSCursor arrowCursor]);
    case 24: return private_cursor(@"_helpCursor", [NSCursor arrowCursor]);
    case 25: return private_cursor(@"zoomInCursor", bundled_cursor(25, [NSCursor arrowCursor]));
    case 26: return private_cursor(@"zoomOutCursor", bundled_cursor(26, [NSCursor arrowCursor]));
    case 27: return bundled_cursor(27, [NSCursor crosshairCursor]);
    case 28: return bundled_cursor(28, [NSCursor operationNotAllowedCursor]);
    // NSCursor answers to _moveCursor on macOS 26 but traps when called.
    case 29: return bundled_cursor(29, [NSCursor openHandCursor]);
    case 30: return bundled_cursor(29, [NSCursor openHandCursor]);
    case 31:
        return frame_cursor(FRAME_TOP | FRAME_LEFT, FRAME_OUTWARD,
                            private_cursor(@"_windowResizeNorthWestCursor", cursor_for(13)));
    case 32:
        return frame_cursor(FRAME_TOP | FRAME_RIGHT, FRAME_OUTWARD,
                            private_cursor(@"_windowResizeNorthEastCursor", cursor_for(14)));
    case 33:
        return frame_cursor(FRAME_BOTTOM | FRAME_LEFT, FRAME_OUTWARD,
                            private_cursor(@"_windowResizeSouthWestCursor", cursor_for(14)));
    case 34:
        return frame_cursor(FRAME_BOTTOM | FRAME_RIGHT, FRAME_OUTWARD,
                            private_cursor(@"_windowResizeSouthEastCursor", cursor_for(13)));
    default: return hidden_cursor();
    }
}

static void show_cursor(SilkWindow *record, NSCursor *cursor) {
    record.view.cursor = cursor;
    NSPoint mouse = [record.window mouseLocationOutsideOfEventStream];
    NSPoint local = [record.view convertPoint:mouse fromView:nil];
    if (NSPointInRect(local, record.view.bounds) && !pointer_over_subview(record.view, mouse)) {
        [record.view.cursor set];
    }
}

void silk_macos_set_cursor(uint32_t id, uint32_t cursor) {
    @autoreleasepool {
        SilkWindow *record = find_window(id);
        if (!record) return;
        show_cursor(record, cursor_for(cursor));
    }
}

uint8_t silk_macos_set_cursor_image(uint32_t id, const uint8_t *pixels, uint32_t width,
                                    uint32_t height, uint32_t hotX, uint32_t hotY, double scale) {
    @autoreleasepool {
        SilkWindow *record = find_window(id);
        if (!record) return 0;
        NSCursor *cursor = pixel_cursor(pixels, width, height, hotX, hotY, scale);
        if (!cursor) return 0;
        show_cursor(record, cursor);
        return 1;
    }
}

void silk_macos_set_ime(uint32_t id, uint8_t enabled, double x, double y, double width,
                        double height) {
    @autoreleasepool {
        SilkView *view = find_window(id).view;
        if (!view) return;
        if (!enabled && view.marked.length > 0) {
            view.marked = [NSMutableAttributedString new];
            [view.inputContext discardMarkedText];
            [view pushPreedit];
        }
        view.imeEnabled = enabled != 0;
        view.imeArea = NSMakeRect(x, y, width, height);
        if (enabled) [view.inputContext invalidateCharacterCoordinates];
    }
}

// Menus.

static unichar named_key_character(uint32_t named) {
    switch (named) {
    case SILK_NAMED_Enter: return '\r';
    case SILK_NAMED_Tab: return '\t';
    case SILK_NAMED_Backspace: return 0x08;
    case SILK_NAMED_Delete: return NSDeleteFunctionKey;
    case SILK_NAMED_Escape: return 0x1B;
    case SILK_NAMED_ArrowLeft: return NSLeftArrowFunctionKey;
    case SILK_NAMED_ArrowRight: return NSRightArrowFunctionKey;
    case SILK_NAMED_ArrowUp: return NSUpArrowFunctionKey;
    case SILK_NAMED_ArrowDown: return NSDownArrowFunctionKey;
    case SILK_NAMED_Home: return NSHomeFunctionKey;
    case SILK_NAMED_End: return NSEndFunctionKey;
    case SILK_NAMED_PageUp: return NSPageUpFunctionKey;
    case SILK_NAMED_PageDown: return NSPageDownFunctionKey;
    case SILK_NAMED_Help: return NSHelpFunctionKey;
    default:
        if (named >= SILK_NAMED_F1 && named <= SILK_NAMED_F24) {
            return (unichar)(NSF1FunctionKey + (named - SILK_NAMED_F1));
        }
        return 0;
    }
}

// How deep the menu stack must be for an item: inside a top-level menu of the bar, or
// anywhere in the tray's menu.
static NSUInteger item_depth(void) { return trayBuilding ? 1 : 2; }

void silk_macos_menu_begin_bar(void) {
    trayBuilding = NO;
    pendingBar = [[NSMenu alloc] initWithTitle:@""];
    [menuStack removeAllObjects];
    [menuStack addObject:pendingBar];
}

void silk_macos_menu_begin(const uint8_t *title, size_t length) {
    if (menuStack.count == 0) return;
    NSString *text = string_from(title, length);
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:text action:nil keyEquivalent:@""];
    NSMenu *menu = [[NSMenu alloc] initWithTitle:text];
    item.submenu = menu;
    [menuStack.lastObject addItem:item];
    [menuStack addObject:menu];
    if (!trayBuilding && menuStack.count == 2 && [text isEqualToString:@"Window"])
        NSApp.windowsMenu = menu;
}

void silk_macos_menu_end(void) {
    if (menuStack.count > 1) [menuStack removeLastObject];
}

void silk_macos_menu_action(const uint8_t *title, size_t length, uint32_t action,
                            uint32_t keyKind, uint32_t keyValue, uint32_t modifiers,
                            uint8_t enabled, uint8_t checked) {
    if (menuStack.count < item_depth()) return;
    NSString *key = @"";
    if (keyKind == 1 && keyValue != 0) {
        uint32_t scalar = keyValue;
        key = [[NSString alloc] initWithBytes:&scalar length:4 encoding:NSUTF32LittleEndianStringEncoding];
    } else if (keyKind == 2) {
        unichar character = named_key_character(keyValue);
        if (character) key = [NSString stringWithCharacters:&character length:1];
    }
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:string_from(title, length)
                                                  action:enabled ? @selector(menuAction:) : nil
                                           keyEquivalent:key ? key : @""];
    item.target = menuTarget;
    item.tag = (NSInteger)action;
    item.enabled = enabled != 0;
    item.state = checked ? NSControlStateValueOn : NSControlStateValueOff;
    if (key.length) item.keyEquivalentModifierMask = modifier_flags(modifiers);
    [menuStack.lastObject addItem:item];
}

void silk_macos_menu_separator(void) {
    if (menuStack.count < item_depth()) return;
    [menuStack.lastObject addItem:[NSMenuItem separatorItem]];
}

void silk_macos_menu_system(uint32_t item) {
    if (menuStack.count < item_depth()) return;
    NSMenuItem *menuItem = system_item(item);
    if (menuItem) [menuStack.lastObject addItem:menuItem];
}

void silk_macos_menu_commit(void) {
    if (!pendingBar || trayBuilding) return;
    // Every menu but the app menu is matched by key equivalents, so items are enabled
    // as built rather than through responder-chain validation.
    for (NSMenuItem *top in pendingBar.itemArray) top.submenu.autoenablesItems = NO;
    if (pendingBar.itemArray.count > 0) pendingBar.itemArray[0].submenu.autoenablesItems = YES;
    NSApp.mainMenu = pendingBar;
    pendingBar = nil;
    [menuStack removeAllObjects];
}

// Dialogs.

static NSArray<UTType *> *content_types(const uint8_t *extensions, size_t length) {
    NSMutableArray<UTType *> *types = [NSMutableArray array];
    for (NSString *part in [string_from(extensions, length) componentsSeparatedByString:@","]) {
        NSString *extension = [part stringByTrimmingCharactersInSet:
                                        [NSCharacterSet whitespaceCharacterSet]];
        if (extension.length == 0) continue;
        UTType *type = [UTType typeWithFilenameExtension:extension];
        if (type) [types addObject:type];
    }
    return types;
}

static void finish_dialog(uint32_t dialog, NSModalResponse response, NSArray<NSURL *> *urls) {
    NSMutableArray<NSData *> *paths = [NSMutableArray array];
    if (response == NSModalResponseOK) {
        for (NSURL *url in urls) {
            if (url.path) [paths addObject:utf8(url.path)];
        }
    }
    SilkWindowEvent event = blank_event(SILK_EVENT_DIALOG_CLOSED, 0);
    event.code = dialog;
    push_texts(event, paths);
    flush();
}

static void show_panel(NSSavePanel *panel, uint32_t parent, void (^done)(NSModalResponse)) {
    NSWindow *window = parent ? find_window(parent).window : nil;
    if (window) {
        [panel beginSheetModalForWindow:window completionHandler:done];
    } else {
        [panel beginWithCompletionHandler:done];
    }
}

uint32_t silk_macos_open_dialog(const uint8_t *title, size_t titleLength,
                                const uint8_t *directory, size_t directoryLength,
                                const uint8_t *extensions, size_t extensionsLength,
                                uint32_t flags, uint32_t parent) {
    @autoreleasepool {
        NSOpenPanel *panel = [NSOpenPanel openPanel];
        if (!panel) return 0;
        uint32_t dialog = nextDialog++;
        panel.message = string_from(title, titleLength);
        panel.canChooseFiles = (flags & 1) != 0;
        panel.canChooseDirectories = (flags & 2) != 0;
        panel.allowsMultipleSelection = (flags & 4) != 0;
        if (flags & 8) {
            panel.directoryURL = [NSURL fileURLWithPath:string_from(directory, directoryLength)
                                            isDirectory:YES];
        }
        NSArray<UTType *> *types = content_types(extensions, extensionsLength);
        if (types.count) panel.allowedContentTypes = types;
        // The handler keeps the panel alive until it runs; AppKit then releases it.
        show_panel(panel, parent, ^(NSModalResponse response) {
          finish_dialog(dialog, response, panel.URLs);
        });
        return dialog;
    }
}

uint32_t silk_macos_save_dialog(const uint8_t *title, size_t titleLength,
                                const uint8_t *directory, size_t directoryLength,
                                const uint8_t *name, size_t nameLength,
                                const uint8_t *extensions, size_t extensionsLength,
                                uint32_t flags, uint32_t parent) {
    @autoreleasepool {
        NSSavePanel *panel = [NSSavePanel savePanel];
        if (!panel) return 0;
        uint32_t dialog = nextDialog++;
        panel.message = string_from(title, titleLength);
        panel.nameFieldStringValue = string_from(name, nameLength);
        if (flags & 8) {
            panel.directoryURL = [NSURL fileURLWithPath:string_from(directory, directoryLength)
                                            isDirectory:YES];
        }
        NSArray<UTType *> *types = content_types(extensions, extensionsLength);
        if (types.count) panel.allowedContentTypes = types;
        show_panel(panel, parent, ^(NSModalResponse response) {
          NSURL *url = panel.URL;
          finish_dialog(dialog, response, url ? @[ url ] : @[]);
        });
        return dialog;
    }
}

// Clipboard and appearance.

const uint8_t *silk_macos_clipboard_read(size_t *length) {
    @autoreleasepool {
        NSString *text = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];
        if (!text) {
            *length = 0;
            return NULL;
        }
        clipboardText = utf8(text);
        *length = clipboardText.length;
        return clipboardText.length ? clipboardText.bytes : (const uint8_t *)"";
    }
}

void silk_macos_clipboard_write(const uint8_t *text, size_t length) {
    @autoreleasepool {
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
        [pasteboard setString:string_from(text, length) forType:NSPasteboardTypeString];
    }
}

static NSPasteboardType pasteboard_type(uint32_t kind) {
    return kind == 0 ? NSPasteboardTypeString : kind == 1 ? NSPasteboardTypeHTML : NSPasteboardTypePNG;
}

uint32_t silk_macos_clipboard_types(void) {
    @autoreleasepool {
        NSArray<NSPasteboardType> *types = [NSPasteboard generalPasteboard].types;
        uint32_t kinds = 0;
        if ([types containsObject:NSPasteboardTypeString]) kinds |= 1;
        if ([types containsObject:NSPasteboardTypeHTML]) kinds |= 2;
        if ([types containsObject:NSPasteboardTypePNG] || [types containsObject:NSPasteboardTypeTIFF]) kinds |= 4;
        return kinds;
    }
}

const uint8_t *silk_macos_clipboard_read_type(uint32_t kind, size_t *length) {
    @autoreleasepool {
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        NSData *data = nil;
        if (kind == 0) {
            NSString *text = [pasteboard stringForType:NSPasteboardTypeString];
            data = text ? utf8(text) : nil;
        } else {
            data = [pasteboard dataForType:pasteboard_type(kind)];
            if (!data && kind == 2) {
                NSData *tiff = [pasteboard dataForType:NSPasteboardTypeTIFF];
                NSBitmapImageRep *image = tiff ? [NSBitmapImageRep imageRepWithData:tiff] : nil;
                data = image ? [image representationUsingType:NSBitmapImageFileTypePNG properties:@{}] : nil;
            }
        }
        clipboardText = [data copy];
        *length = clipboardText.length;
        if (!clipboardText) return NULL;
        return clipboardText.length ? clipboardText.bytes : (const uint8_t *)"";
    }
}

void silk_macos_clipboard_begin(void) {
    @autoreleasepool {
        [[NSPasteboard generalPasteboard] clearContents];
    }
}

void silk_macos_clipboard_add(uint32_t kind, const uint8_t *data, size_t length) {
    @autoreleasepool {
        if (kind > 2) return;
        [[NSPasteboard generalPasteboard] setData:[NSData dataWithBytes:data length:length]
                                          forType:pasteboard_type(kind)];
    }
}

uint32_t silk_macos_appearance(void) {
    @autoreleasepool {
        return started ? current_appearance() : 0;
    }
}

// Test hooks.

void silk_macos_test_post_key(uint32_t id, uint16_t keyCode, const uint8_t *characters,
                              size_t length, uint32_t modifiers) {
    @autoreleasepool {
        NSWindow *window = find_window(id).window;
        if (!window) return;
        NSString *text = string_from(characters, length);
        NSEventModifierFlags flags = modifier_flags(modifiers);
        for (int up = 0; up < 2; up++) {
            NSEvent *event = [NSEvent keyEventWithType:up ? NSEventTypeKeyUp : NSEventTypeKeyDown
                                              location:NSZeroPoint
                                         modifierFlags:flags
                                             timestamp:NSProcessInfo.processInfo.systemUptime
                                          windowNumber:window.windowNumber
                                               context:nil
                                            characters:text
                           charactersIgnoringModifiers:text
                                             isARepeat:NO
                                               keyCode:keyCode];
            [NSApp postEvent:event atStart:NO];
        }
    }
}

void silk_macos_test_set_marked_text(uint32_t id, const uint8_t *text, size_t length,
                                     uint64_t selectionStart, uint64_t selectionLength) {
    @autoreleasepool {
        SilkView *view = find_window(id).view;
        [view setMarkedText:string_from(text, length)
              selectedRange:NSMakeRange((NSUInteger)selectionStart, (NSUInteger)selectionLength)
           replacementRange:NSMakeRange(NSNotFound, 0)];
    }
}

void silk_macos_test_insert_text(uint32_t id, const uint8_t *text, size_t length) {
    @autoreleasepool {
        [find_window(id).view insertText:string_from(text, length)
                        replacementRange:NSMakeRange(NSNotFound, 0)];
    }
}

void silk_macos_test_set_frame_size(uint32_t id, double width, double height) {
    @autoreleasepool {
        [find_window(id).window setContentSize:NSMakeSize(width, height)];
    }
}

void silk_macos_test_ime_rect(uint32_t id, double *x, double *y, double *width,
                              double *height) {
    @autoreleasepool {
        SilkView *view = find_window(id).view;
        NSRect rect = view ? [view firstRectForCharacterRange:NSMakeRange(0, 0) actualRange:NULL]
                           : NSZeroRect;
        *x = rect.origin.x;
        *y = rect.origin.y;
        *width = rect.size.width;
        *height = rect.size.height;
    }
}

uint8_t silk_macos_test_uses_fiber(void) { return fiberMode ? 1 : 0; }

// UI-tree accessibility snapshot and action queue. Strings are copied before returning.
void silk_macos_ax_begin(uint32_t id) {
    SilkView *view = find_window(id).view;
    if (!view.axNodes) view.axNodes = [NSMutableDictionary new];
    view.axSeen = [NSMutableSet new];
    view.axOrder = [NSMutableArray new];
    view.axPreviousFocus = view.axFocused;
    view.axFocused = 0;
}
void silk_macos_ax_node(uint32_t id, uint32_t nodeId, uint32_t parent, uint32_t role,
    uint32_t flags, double x, double y, double width, double height,
    const uint8_t *label, size_t labelLength, const uint8_t *value, size_t valueLength) {
    SilkView *view = find_window(id).view;
    SilkAXNode *node = view.axNodes[@(nodeId)];
    if (!node) { node = [SilkAXNode new]; node.owner = view; node.nodeId = nodeId;
        view.axNodes[@(nodeId)] = node; }
    NSString *nextValue = string_from(value, valueLength);
    BOOL changed = node.valueText && ![node.valueText isEqualToString:nextValue];
    node.parentId = parent; node.roleCode = role; node.flags = flags; node.hasRange = NO;
    node.localFrame = NSMakeRect(x, y, width, height);
    node.labelText = string_from(label, labelLength); node.valueText = nextValue;
    [view.axSeen addObject:@(nodeId)];
    [view.axOrder addObject:@(nodeId)];
    if (flags & 2) view.axFocused = nodeId;
    if (changed) NSAccessibilityPostNotification(node, NSAccessibilityValueChangedNotification);
}
void silk_macos_ax_end(uint32_t id) {
    SilkView *view = find_window(id).view;
    for (NSNumber *key in [view.axNodes allKeys]) if (![view.axSeen containsObject:key]) [view.axNodes removeObjectForKey:key];
    // Build child lists once per published snapshot. Scanning every node inside
    // accessibilityChildren made a full accessibility traversal quadratic.
    NSMutableDictionary<NSNumber *, NSMutableArray<SilkAXNode *> *> *children = [NSMutableDictionary new];
    NSMutableArray<SilkAXNode *> *roots = [NSMutableArray new];
    for (NSNumber *key in view.axOrder) {
        SilkAXNode *node = view.axNodes[key];
        NSNumber *parent = @(node.parentId);
        if (!view.axNodes[parent]) { [roots addObject:node]; continue; }
        NSMutableArray<SilkAXNode *> *siblings = children[parent];
        if (!siblings) { siblings = [NSMutableArray new]; children[parent] = siblings; }
        [siblings addObject:node];
    }
    view.axChildren = children;
    view.axRoots = roots;
    if (view.axFocused != view.axPreviousFocus)
        NSAccessibilityPostNotification(view.axNodes[@(view.axFocused)] ?: view, NSAccessibilityFocusedUIElementChangedNotification);
    NSAccessibilityPostNotification(view, NSAccessibilityLayoutChangedNotification);
}
void silk_macos_ax_range(uint32_t window, uint32_t id, double min, double max, double value) {
    SilkAXNode *node = find_window(window).view.axNodes[@(id)];
    BOOL changed = node.rangeValue != value;
    node.hasRange = YES; node.rangeMin = min; node.rangeMax = max; node.rangeValue = value;
    if (changed) NSAccessibilityPostNotification(node, NSAccessibilityValueChangedNotification);
}
uint32_t silk_macos_ax_poll(uint32_t id, uint32_t *target) {
    SilkView *view = find_window(id).view;
    if (!view.axActions.count) return 0;
    view.axAction = view.axActions[0]; [view.axActions removeObjectAtIndex:0];
    *target = [view.axAction[@"target"] unsignedIntValue];
    return [view.axAction[@"kind"] unsignedIntValue];
}
size_t silk_macos_ax_text(uint32_t id, uint8_t *out, size_t capacity) {
    NSData *data = [find_window(id).view.axAction[@"text"] dataUsingEncoding:NSUTF8StringEncoding];
    if (out && capacity >= data.length) memcpy(out, data.bytes, data.length);
    return data.length;
}

// ---------------------------------------------------------------------------------------
// The tray: an NSStatusItem in the menu bar.

static NSStatusItem *statusItem;
static id trayTarget;

@interface SilkTrayTarget : NSObject
- (void)clicked:(id)sender;
@end

@implementation SilkTrayTarget
- (void)clicked:(id)sender {
    SilkWindowEvent event = blank_event(SILK_EVENT_TRAY_CLICKED, 0);
    NSEventType type = NSApp.currentEvent.type;
    event.code = type == NSEventTypeRightMouseUp ? 1 : type == NSEventTypeOtherMouseUp ? 2 : 0;
    push(event);
    flush();
}
@end

void silk_macos_tray_begin(void) {
    trayBuilding = YES;
    pendingBar = [[NSMenu alloc] initWithTitle:@""];
    pendingBar.autoenablesItems = NO;
    [menuStack removeAllObjects];
    [menuStack addObject:pendingBar];
}

uint8_t silk_macos_tray_commit(const uint8_t *pixels, uint32_t width, uint32_t height,
                               const uint8_t *tooltip, size_t tooltipLength, uint32_t flags) {
    NSMenu *menu = trayBuilding ? pendingBar : nil;
    trayBuilding = NO;
    pendingBar = nil;
    [menuStack removeAllObjects];
    if (!started || !width || !height) return 0;
    @autoreleasepool {
        NSBitmapImageRep *rep = [[NSBitmapImageRep alloc]
            initWithBitmapDataPlanes:NULL
                          pixelsWide:(NSInteger)width
                          pixelsHigh:(NSInteger)height
                       bitsPerSample:8
                     samplesPerPixel:4
                            hasAlpha:YES
                            isPlanar:NO
                      colorSpaceName:NSDeviceRGBColorSpace
                        bitmapFormat:NSBitmapFormatAlphaNonpremultiplied
                         bytesPerRow:(NSInteger)width * 4
                        bitsPerPixel:32];
        if (!rep) return 0;
        memcpy(rep.bitmapData, pixels, (size_t)width * height * 4);
        // Menu bar icons are 18 points tall.
        NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(18.0 * width / height, 18.0)];
        [image addRepresentation:rep];
        image.template = (flags & SILK_TRAY_TEMPLATE) != 0;
        if (!statusItem) {
            statusItem = [NSStatusBar.systemStatusBar statusItemWithLength:NSSquareStatusItemLength];
            trayTarget = [SilkTrayTarget new];
        }
        if (!statusItem) return 0;
        statusItem.visible = YES;
        statusItem.button.image = image;
        statusItem.button.toolTip = string_from(tooltip, tooltipLength);
        if (menu.itemArray.count) {
            statusItem.menu = menu;
            statusItem.button.target = nil;
            statusItem.button.action = nil;
        } else {
            statusItem.menu = nil;
            statusItem.button.target = trayTarget;
            statusItem.button.action = @selector(clicked:);
            [statusItem.button sendActionOn:NSEventMaskLeftMouseUp | NSEventMaskRightMouseUp |
                                            NSEventMaskOtherMouseUp];
        }
    }
    return 1;
}

void silk_macos_tray_remove(void) {
    if (!statusItem) return;
    [NSStatusBar.systemStatusBar removeStatusItem:statusItem];
    statusItem = nil;
}

// Context menus share the tray's way of building a menu.
void silk_macos_context_menu_begin(void) { silk_macos_tray_begin(); }

uint8_t silk_macos_context_menu_commit(uint32_t window, double x, double y) {
    NSMenu *menu = trayBuilding ? pendingBar : nil;
    trayBuilding = NO;
    pendingBar = nil;
    [menuStack removeAllObjects];
    SilkView *view = find_window(window).view;
    if (!started || !menu || !view) return 0;
    // The menu tracks in a loop of its own, so it opens once the current event is done.
    dispatch_async(dispatch_get_main_queue(), ^{
      BOOL chosen = [menu popUpMenuPositioningItem:nil atLocation:NSMakePoint(x, y) inView:view];
      if (!chosen) {
          push(blank_event(SILK_EVENT_CONTEXT_MENU_CLOSED, window));
          flush();
      }
    });
    return 1;
}

static NSProgressIndicator *dockProgress;

void silk_macos_set_badge(const uint8_t *text, size_t length) {
    NSApp.dockTile.badgeLabel = length ? string_from(text, length) : nil;
}

void silk_macos_set_progress(double fraction) {
    NSDockTile *tile = NSApp.dockTile;
    if (fraction < 0) {
        if (!tile.contentView) return;
        tile.contentView = nil;
        dockProgress = nil;
        [tile display];
        return;
    }
    if (!dockProgress) {
        NSSize size = tile.size;
        NSImageView *icon = [NSImageView imageViewWithImage:NSApp.applicationIconImage];
        icon.frame = NSMakeRect(0, 0, size.width, size.height);
        dockProgress = [[NSProgressIndicator alloc]
            initWithFrame:NSMakeRect(size.width * 0.1, size.height * 0.06, size.width * 0.8, 14)];
        dockProgress.style = NSProgressIndicatorStyleBar;
        dockProgress.indeterminate = NO;
        dockProgress.minValue = 0;
        dockProgress.maxValue = 1;
        [icon addSubview:dockProgress];
        tile.contentView = icon;
    }
    dockProgress.doubleValue = fraction > 1 ? 1 : fraction;
    [tile display];
}

// ---------------------------------------------------------------------------------------
// URLs, Finder and dragging files out.

uint8_t silk_macos_open_url(const uint8_t *url, size_t length) {
    @autoreleasepool {
        NSURL *target = length ? [NSURL URLWithString:string_from(url, length)] : nil;
        if (!target || !target.scheme) return 0;
        return [NSWorkspace.sharedWorkspace openURL:target] ? 1 : 0;
    }
}

uint8_t silk_macos_reveal_path(const uint8_t *path, size_t length) {
    @autoreleasepool {
        if (!length) return 0;
        NSURL *target = [NSURL fileURLWithPath:string_from(path, length)];
        if (![NSFileManager.defaultManager fileExistsAtPath:target.path]) return 0;
        [NSWorkspace.sharedWorkspace activateFileViewerSelectingURLs:@[ target ]];
        return 1;
    }
}

@interface SilkView (FileDrag) <NSDraggingSource>
@end

@implementation SilkView (FileDrag)
- (NSDragOperation)draggingSession:(NSDraggingSession *)session
    sourceOperationMaskForDraggingContext:(NSDraggingContext)context {
    return context == NSDraggingContextOutsideApplication ? NSDragOperationCopy | NSDragOperationLink |
                                                                NSDragOperationGeneric
                                                          : NSDragOperationCopy;
}

// AppKit's drag loop takes the button's release, so the window would never see it.
- (void)draggingSession:(NSDraggingSession *)session
           endedAtPoint:(NSPoint)screenPoint
              operation:(NSDragOperation)operation {
    self.pressed = nil;
    NSPoint inWindow = [self.window convertPointFromScreen:screenPoint];
    NSPoint position = [self convertPoint:inWindow fromView:nil];
    SilkWindowEvent out = blank_event(SILK_EVENT_MOUSE_UP, self.windowId);
    out.x = position.x;
    out.y = position.y;
    out.modifiers = modifier_bits(NSEvent.modifierFlags);
    push(out);
    flush();
}
@end

uint8_t silk_macos_start_file_drag(uint32_t window, const uint8_t *paths, size_t length) {
    @autoreleasepool {
        SilkView *view = find_window(window).view;
        NSEvent *event = view.pressed;
        if (!view || !event || ([NSEvent pressedMouseButtons] & 1) == 0 || !length) return 0;
        NSPoint at = [view convertPoint:event.locationInWindow fromView:nil];
        NSMutableArray<NSDraggingItem *> *items = [NSMutableArray array];
        for (NSString *line in [string_from(paths, length) componentsSeparatedByString:@"\n"]) {
            if (line.length == 0) continue;
            NSURL *url = [NSURL fileURLWithPath:line];
            NSDraggingItem *item = [[NSDraggingItem alloc] initWithPasteboardWriter:url];
            NSImage *icon = [NSWorkspace.sharedWorkspace iconForFile:line];
            icon.size = NSMakeSize(32, 32);
            // Each icon is offset a little from the one before, as Finder stacks them.
            CGFloat offset = (CGFloat)MIN(items.count, 4) * 4;
            [item setDraggingFrame:NSMakeRect(at.x - 16 + offset, at.y - 16 + offset, 32, 32)
                          contents:icon];
            [items addObject:item];
        }
        if (items.count == 0) return 0;
        [view beginDraggingSessionWithItems:items event:event source:view];
        return 1;
    }
}

// ---------------------------------------------------------------------------------------
// Global shortcuts: Carbon hot keys, which need no accessibility permission.

typedef struct HotKey {
    uint32_t id;
    EventHotKeyRef ref;
} HotKey;

static HotKey hotKeys[64];
static size_t hotKeyCount;
static BOOL hotKeyHandler;

static OSStatus hot_key_pressed(EventHandlerCallRef next, EventRef carbonEvent, void *data) {
    EventHotKeyID hotKey;
    if (GetEventParameter(carbonEvent, kEventParamDirectObject, typeEventHotKeyID, NULL,
                          sizeof hotKey, NULL, &hotKey) != noErr)
        return eventNotHandledErr;
    SilkWindowEvent event = blank_event(SILK_EVENT_GLOBAL_SHORTCUT, 0);
    event.code = hotKey.id;
    push(event);
    flush();
    return noErr;
}

void silk_macos_shortcut_unregister(uint32_t id) {
    for (size_t i = 0; i < hotKeyCount; i++) {
        if (hotKeys[i].id != id) continue;
        UnregisterEventHotKey(hotKeys[i].ref);
        hotKeys[i] = hotKeys[--hotKeyCount];
        return;
    }
}

uint8_t silk_macos_shortcut_register(uint32_t id, uint32_t code, uint32_t modifiers) {
    if (!started || hotKeyCount == sizeof hotKeys / sizeof hotKeys[0]) return 0;
    silk_macos_shortcut_unregister(id);
    int keyCode = -1;
    for (int i = 0; i < 128 && code; i++)
        if (silk_macos_key_codes[i] == code) keyCode = i;
    if (keyCode < 0) return 0;
    if (!hotKeyHandler) {
        EventTypeSpec spec = {kEventClassKeyboard, kEventHotKeyPressed};
        if (InstallApplicationEventHandler(NewEventHandlerUPP(hot_key_pressed), 1, &spec, NULL, NULL) != noErr)
            return 0;
        hotKeyHandler = YES;
    }
    UInt32 carbon = 0;
    if (modifiers & 1) carbon |= shiftKey;
    if (modifiers & 2) carbon |= controlKey;
    if (modifiers & 4) carbon |= optionKey;
    if (modifiers & 8) carbon |= cmdKey;
    EventHotKeyID hotKey = {0x53494C4B, id}; // 'SILK'
    EventHotKeyRef ref = NULL;
    if (RegisterEventHotKey((UInt32)keyCode, carbon, hotKey, GetApplicationEventTarget(), 0, &ref) != noErr)
        return 0;
    hotKeys[hotKeyCount].id = id;
    hotKeys[hotKeyCount].ref = ref;
    hotKeyCount++;
    return 1;
}

// ---------------------------------------------------------------------------------------
// Notifications. UNUserNotificationCenter only works for an app bundle; a bare executable
// (a development build) shows them through osascript instead, without clicks.

static uint32_t nextNotification = 1;
static BOOL notificationsAsked;
static id notificationDelegate;

@interface SilkNotificationDelegate : NSObject <UNUserNotificationCenterDelegate>
@end

@implementation SilkNotificationDelegate
- (void)userNotificationCenter:(UNUserNotificationCenter *)center
       willPresentNotification:(UNNotification *)notification
         withCompletionHandler:(void (^)(UNNotificationPresentationOptions))completion {
    completion(UNNotificationPresentationOptionBanner | UNNotificationPresentationOptionList);
}

- (void)userNotificationCenter:(UNUserNotificationCenter *)center
    didReceiveNotificationResponse:(UNNotificationResponse *)response
             withCompletionHandler:(void (^)(void))completion {
    uint32_t id = (uint32_t)response.notification.request.identifier.longLongValue;
    BOOL clicked = [response.actionIdentifier isEqualToString:UNNotificationDefaultActionIdentifier];
    dispatch_async(dispatch_get_main_queue(), ^{
      if (clicked) {
          SilkWindowEvent event = blank_event(SILK_EVENT_NOTIFICATION_CLICKED, 0);
          event.code = id;
          push(event);
          flush();
      }
    });
    completion();
}
@end

static NSString *apple_script_string(NSString *text) {
    NSString *escaped = [text stringByReplacingOccurrencesOfString:@"\\" withString:@"\\\\"];
    escaped = [escaped stringByReplacingOccurrencesOfString:@"\"" withString:@"\\\""];
    return [NSString stringWithFormat:@"\"%@\"", escaped];
}

uint32_t silk_macos_notify(const uint8_t *title, size_t titleLength, const uint8_t *body,
                           size_t bodyLength) {
    if (!started) return 0;
    uint32_t id = nextNotification++;
    @autoreleasepool {
        NSString *titleText = string_from(title, titleLength);
        NSString *bodyText = string_from(body, bodyLength);
        if (!NSBundle.mainBundle.bundleIdentifier) {
            NSString *script = [NSString stringWithFormat:@"display notification %@ with title %@",
                                                          apple_script_string(bodyText),
                                                          apple_script_string(titleText)];
            NSTask *task = [NSTask new];
            task.executableURL = [NSURL fileURLWithPath:@"/usr/bin/osascript"];
            task.arguments = @[ @"-e", script ];
            NSError *error = nil;
            if (![task launchAndReturnError:&error]) return 0;
            return id;
        }
        UNUserNotificationCenter *center = UNUserNotificationCenter.currentNotificationCenter;
        if (!notificationsAsked) {
            notificationsAsked = YES;
            notificationDelegate = [SilkNotificationDelegate new];
            center.delegate = notificationDelegate;
            [center requestAuthorizationWithOptions:UNAuthorizationOptionAlert | UNAuthorizationOptionSound
                                  completionHandler:^(BOOL granted, NSError *error){
                                  }];
        }
        UNMutableNotificationContent *content = [UNMutableNotificationContent new];
        content.title = titleText;
        content.body = bodyText;
        NSString *identifier = [NSString stringWithFormat:@"%u", id];
        UNNotificationRequest *request = [UNNotificationRequest requestWithIdentifier:identifier
                                                                              content:content
                                                                              trigger:nil];
        [center addNotificationRequest:request withCompletionHandler:nil];
    }
    return id;
}

uint8_t silk_macos_test_drop(uint32_t window, uint32_t kind, double x, double y,
                             const uint8_t *paths, size_t length) {
    // AppKit drags come from another app's dragging session; there is nothing to post.
    return 0;
}

// ---------------------------------------------------------------------------------------------
// GPU canvas layers: a canvas whose frames are IOSurfaces shows in a layer of its own above the
// window's CAMetalLayer, so a canvas frame changes that layer's contents and nothing else. Each
// canvas is two layers: a clip layer (masksToBounds, with the clip's corner radius) holding the
// content layer, whose contents are the IOSurface. Every call may run on any thread: changes go
// in an explicit transaction with actions disabled (silk_ca_begin / silk_ca_commit), so nothing
// animates and nothing waits for the main thread's implicit transaction.

void silk_ca_begin(void) {
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
}

void silk_ca_commit(void) {
    [CATransaction commit];
    // A thread with no run loop has no implicit transaction to carry these to the window server.
    [CATransaction flush];
}

void *silk_ca_canvas_layer_create(void *parent) {
    if (!parent) return NULL;
    @autoreleasepool {
        CALayer *superlayer = (__bridge CALayer *)parent;
        CALayer *clip = [CALayer layer];
        clip.anchorPoint = CGPointZero;
        clip.masksToBounds = YES;
        clip.hidden = YES;
        CALayer *content = [CALayer layer];
        content.anchorPoint = CGPointZero;
        content.contentsGravity = kCAGravityResize;
        [clip addSublayer:content];
        silk_ca_begin();
        [superlayer addSublayer:clip];
        silk_ca_commit();
        return (__bridge_retained void *)clip;
    }
}

void silk_ca_canvas_layer_destroy(void *handle) {
    if (!handle) return;
    @autoreleasepool {
        CALayer *clip = (__bridge_transfer CALayer *)handle;
        silk_ca_begin();
        [clip removeFromSuperlayer];
        silk_ca_commit();
    }
}

// Rects are in the window's device pixels from its top-left corner; `scale` turns them into the
// superlayer's points.
static CGRect pointsRect(CALayer *superlayer, double x, double y, double w, double h, double scale) {
    double s = scale > 0.0 ? scale : 1.0;
    CGRect r = CGRectMake(x / s, y / s, w / s, h / s);
    if (superlayer && !superlayer.geometryFlipped) {
        r.origin.y = superlayer.bounds.size.height - r.origin.y - r.size.height;
    }
    return r;
}

void silk_ca_canvas_layer_set(void *handle, const SilkCanvasLayerState *state, void *io_surface) {
    if (!handle || !state) return;
    @autoreleasepool {
        CALayer *clip = (__bridge CALayer *)handle;
        CALayer *content = clip.sublayers.firstObject;
        // No scale given: the window's layer knows its own.
        double given = state->scale > 0.0 ? state->scale : clip.superlayer.contentsScale;
        double s = given > 0.0 ? given : 1.0;
        clip.frame = pointsRect(clip.superlayer, state->clip_x, state->clip_y, state->clip_width, state->clip_height, s);
        clip.cornerRadius = state->corner_radius / s;
        clip.hidden = state->hidden != 0;
        // The content layer sits at the canvas's place, relative to the clip's corner.
        CGRect canvas = CGRectMake((state->x - state->clip_x) / s, (state->y - state->clip_y) / s,
                                   state->width / s, state->height / s);
        if (clip.superlayer && !clip.superlayer.geometryFlipped) {
            canvas.origin.y = clip.bounds.size.height - canvas.origin.y - canvas.size.height;
        }
        content.frame = canvas;
        content.contentsScale = s;
        content.opaque = state->opaque != 0;
        content.opacity = (float)state->opacity;
        if (io_surface) {
            // The same IOSurface comes back with new pixels (a swap chain reuses its images): Core
            // Animation keeps what it read of a surface unless told the contents changed.
            SEL changed = NSSelectorFromString(@"setContentsChanged");
            if (content.contents == (__bridge id)io_surface && ![content respondsToSelector:changed]) {
                content.contents = nil;
            }
            content.contents = (__bridge id)io_surface;
            if ([content respondsToSelector:changed]) {
                ((void (*)(id, SEL))objc_msgSend)(content, changed);
            }
        }
    }
}

void silk_ca_canvas_layer_get(void *handle, SilkCanvasLayerState *out, void **io_surface) {
    if (!handle || !out) return;
    @autoreleasepool {
        CALayer *clip = (__bridge CALayer *)handle;
        CALayer *content = clip.sublayers.firstObject;
        double s = content.contentsScale > 0.0 ? content.contentsScale : 1.0;
        CGRect c = clip.frame;
        if (clip.superlayer && !clip.superlayer.geometryFlipped) {
            c.origin.y = clip.superlayer.bounds.size.height - c.origin.y - c.size.height;
        }
        CGRect r = content.frame;
        if (clip.superlayer && !clip.superlayer.geometryFlipped) {
            r.origin.y = clip.bounds.size.height - r.origin.y - r.size.height;
        }
        out->clip_x = c.origin.x * s;
        out->clip_y = c.origin.y * s;
        out->clip_width = c.size.width * s;
        out->clip_height = c.size.height * s;
        out->x = (c.origin.x + r.origin.x) * s;
        out->y = (c.origin.y + r.origin.y) * s;
        out->width = r.size.width * s;
        out->height = r.size.height * s;
        out->corner_radius = clip.cornerRadius * s;
        out->opacity = content.opacity;
        out->scale = s;
        out->opaque = content.opaque ? 1 : 0;
        out->hidden = clip.hidden ? 1 : 0;
        out->masks = clip.masksToBounds ? 1 : 0;
        if (io_surface) *io_surface = (__bridge void *)content.contents;
    }
}

int silk_ca_layer_index_of(void *parent, void *handle) {
    if (!parent || !handle) return -1;
    CALayer *superlayer = (__bridge CALayer *)parent;
    NSUInteger at = [superlayer.sublayers indexOfObjectIdenticalTo:(__bridge CALayer *)handle];
    return at == NSNotFound ? -1 : (int)at;
}

void silk_ca_metal_layer_presents_with_transaction(void *layer, int on) {
    if (!layer) return;
    CAMetalLayer *metal = (__bridge CAMetalLayer *)layer;
    if ([metal isKindOfClass:[CAMetalLayer class]]) metal.presentsWithTransaction = on != 0;
}

int silk_ca_metal_layer_presents_with_transaction_get(void *layer) {
    if (!layer) return 0;
    CAMetalLayer *metal = (__bridge CAMetalLayer *)layer;
    return [metal isKindOfClass:[CAMetalLayer class]] && metal.presentsWithTransaction ? 1 : 0;
}

// A layer with no window, for tests: a CAMetalLayer when `metal`, of `width` x `height` points.
void *silk_ca_detached_layer_create(double width, double height, int flipped, int metal) {
    @autoreleasepool {
        CALayer *layer = metal ? [CAMetalLayer layer] : [CALayer layer];
        layer.anchorPoint = CGPointZero;
        layer.bounds = CGRectMake(0, 0, width, height);
        layer.geometryFlipped = flipped != 0;
        return (__bridge_retained void *)layer;
    }
}

void silk_ca_layer_release(void *layer) {
    if (layer) CFRelease(layer);
}

// A small BGRA IOSurface, for tests of canvas layers.
void *silk_ca_test_iosurface_create(uint32_t width, uint32_t height) {
    NSDictionary *props = @{
        (id)kIOSurfaceWidth: @(width),
        (id)kIOSurfaceHeight: @(height),
        (id)kIOSurfaceBytesPerElement: @4,
        (id)kIOSurfacePixelFormat: @((uint32_t)'BGRA'),
    };
    return (void *)IOSurfaceCreate((__bridge CFDictionaryRef)props);
}

void silk_ca_layer_set_contents_scale(void *layer, double scale) {
    if (layer) ((__bridge CALayer *)layer).contentsScale = scale;
}

// What Core Animation holds for a canvas layer and its parent, as one line of text: for traces.
void silk_ca_canvas_layer_describe(void *handle, char *out, size_t capacity) {
    if (!out || capacity == 0) return;
    out[0] = 0;
    if (!handle) return;
    @autoreleasepool {
        CALayer *clip = (__bridge CALayer *)handle;
        CALayer *content = clip.sublayers.firstObject;
        CALayer *parent = clip.superlayer;
        NSUInteger index = parent ? [parent.sublayers indexOfObjectIdenticalTo:clip] : NSNotFound;
        snprintf(out, capacity,
                 "clip=(%.1f,%.1f %.1fx%.1f)pt hidden=%d masks=%d radius=%.1f content=(%.1f,%.1f %.1fx%.1f)pt"
                 " opacity=%.2f scale=%.1f contents=%s parent=%s parent-bounds=(%.1fx%.1f) parent-flipped=%d"
                 " parent-scale=%.1f parent-hidden=%d index=%ld of %lu window=%s",
                 clip.frame.origin.x, clip.frame.origin.y, clip.frame.size.width, clip.frame.size.height,
                 clip.hidden ? 1 : 0, clip.masksToBounds ? 1 : 0, clip.cornerRadius,
                 content.frame.origin.x, content.frame.origin.y, content.frame.size.width, content.frame.size.height,
                 content.opacity, content.contentsScale, content.contents ? "yes" : "no",
                 parent ? NSStringFromClass([parent class]).UTF8String : "none",
                 parent.bounds.size.width, parent.bounds.size.height, parent.geometryFlipped ? 1 : 0,
                 parent.contentsScale, parent.hidden ? 1 : 0,
                 index == NSNotFound ? -1L : (long)index, (unsigned long)parent.sublayers.count,
                 parent.superlayer ? "attached" : "detached");
    }
}
