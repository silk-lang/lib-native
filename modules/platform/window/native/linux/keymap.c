#include <xkbcommon/xkbcommon-keysyms.h>
#include "linux.h"

uint32_t silk_keysym_named(uint32_t keysym) {
    if (keysym >= XKB_KEY_F1 && keysym <= XKB_KEY_F24) return SILK_NAMED_F1 + (keysym - XKB_KEY_F1);
    switch (keysym) {
    case XKB_KEY_Return:
    case XKB_KEY_KP_Enter:
    case XKB_KEY_ISO_Enter: return SILK_NAMED_Enter;
    case XKB_KEY_Tab:
    case XKB_KEY_ISO_Left_Tab:
    case XKB_KEY_KP_Tab: return SILK_NAMED_Tab;
    case XKB_KEY_BackSpace: return SILK_NAMED_Backspace;
    case XKB_KEY_Delete:
    case XKB_KEY_KP_Delete: return SILK_NAMED_Delete;
    case XKB_KEY_Insert:
    case XKB_KEY_KP_Insert: return SILK_NAMED_Insert;
    case XKB_KEY_Escape: return SILK_NAMED_Escape;
    case XKB_KEY_Left:
    case XKB_KEY_KP_Left: return SILK_NAMED_ArrowLeft;
    case XKB_KEY_Right:
    case XKB_KEY_KP_Right: return SILK_NAMED_ArrowRight;
    case XKB_KEY_Up:
    case XKB_KEY_KP_Up: return SILK_NAMED_ArrowUp;
    case XKB_KEY_Down:
    case XKB_KEY_KP_Down: return SILK_NAMED_ArrowDown;
    case XKB_KEY_Home:
    case XKB_KEY_KP_Home: return SILK_NAMED_Home;
    case XKB_KEY_End:
    case XKB_KEY_KP_End: return SILK_NAMED_End;
    case XKB_KEY_Page_Up:
    case XKB_KEY_KP_Page_Up: return SILK_NAMED_PageUp;
    case XKB_KEY_Page_Down:
    case XKB_KEY_KP_Page_Down: return SILK_NAMED_PageDown;
    case XKB_KEY_Shift_L:
    case XKB_KEY_Shift_R: return SILK_NAMED_Shift;
    case XKB_KEY_Control_L:
    case XKB_KEY_Control_R: return SILK_NAMED_Control;
    case XKB_KEY_Alt_L:
    case XKB_KEY_Alt_R:
    case XKB_KEY_ISO_Level3_Shift: return SILK_NAMED_Alt;
    case XKB_KEY_Super_L:
    case XKB_KEY_Super_R:
    case XKB_KEY_Meta_L:
    case XKB_KEY_Meta_R: return SILK_NAMED_Meta;
    case XKB_KEY_Caps_Lock: return SILK_NAMED_CapsLock;
    case XKB_KEY_Menu: return SILK_NAMED_ContextMenu;
    case XKB_KEY_Help: return SILK_NAMED_Help;
    case XKB_KEY_Num_Lock: return SILK_NAMED_NumLock;
    case XKB_KEY_Scroll_Lock: return SILK_NAMED_ScrollLock;
    case XKB_KEY_Pause: return SILK_NAMED_Pause;
    case XKB_KEY_Print: return SILK_NAMED_PrintScreen;
    case XKB_KEY_XF86AudioRaiseVolume: return SILK_NAMED_AudioVolumeUp;
    case XKB_KEY_XF86AudioLowerVolume: return SILK_NAMED_AudioVolumeDown;
    case XKB_KEY_XF86AudioMute: return SILK_NAMED_AudioVolumeMute;
    case XKB_KEY_Hangul:
    case XKB_KEY_Kana_Lock:
    case XKB_KEY_Hiragana_Katakana: return SILK_NAMED_KanaMode;
    case XKB_KEY_Eisu_toggle:
    case XKB_KEY_Hangul_Hanja: return SILK_NAMED_Eisu;
    default: return 0;
    }
}

uint32_t silk_evdev_code(uint32_t evdev) {
    if (evdev >= sizeof silk_evdev_key_codes / sizeof silk_evdev_key_codes[0]) return 0;
    return silk_evdev_key_codes[evdev];
}

uint32_t silk_evdev_from_code(uint32_t code) {
    if (code == 0) return 0;
    for (uint32_t i = 0; i < sizeof silk_evdev_key_codes / sizeof silk_evdev_key_codes[0]; i++)
        if (silk_evdev_key_codes[i] == code) return i;
    return 0;
}

uint32_t silk_named_modifier(uint32_t named) {
    switch (named) {
    case SILK_NAMED_Shift: return SILK_MOD_SHIFT;
    case SILK_NAMED_Control: return SILK_MOD_CONTROL;
    case SILK_NAMED_Alt: return SILK_MOD_ALT;
    case SILK_NAMED_Meta: return SILK_MOD_META;
    case SILK_NAMED_CapsLock: return SILK_MOD_CAPS_LOCK;
    default: return 0;
    }
}
