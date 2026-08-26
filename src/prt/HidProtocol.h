#pragma once

#include <cstddef>
#include <cstdint>

namespace prt_hid
{
    enum class ReportId : uint8_t
    {
        KEYBOARD = 1,
        CONSUMER = 2,
        SYSTEM   = 3,
        MOUSE    = 4
    };

    struct KeyboardReport
    {
        uint8_t modifiers;
        uint8_t keys[15];
    } __attribute__((packed));

    struct FeatureReport
    {
        uint8_t command;
        uint8_t slot;
        uint8_t usage_lo;
        uint8_t usage_hi;
        uint8_t layer;
        uint8_t reserved[27];
    } __attribute__((packed));

    enum class MouseBtn : uint8_t
    {
        None         = 0u,
        MouseLeft    = 1u << 0,
        MouseRight   = 1u << 1,
        MouseMiddle  = 1u << 2,
        MouseBack    = 1u << 3,
        MouseForward = 1u << 4,
    };

    constexpr MouseBtn operator|(MouseBtn a, MouseBtn b) {
        return static_cast<MouseBtn>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }

    constexpr MouseBtn operator&(MouseBtn a, MouseBtn b) {
        return static_cast<MouseBtn>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
    }

    constexpr MouseBtn operator~(MouseBtn a) {
        return static_cast<MouseBtn>(~static_cast<uint8_t>(a));
    }

    constexpr MouseBtn& operator|=(MouseBtn& a, MouseBtn b) {
        a = a | b;
        return a;
    }

    constexpr MouseBtn& operator&=(MouseBtn& a, MouseBtn b) {
        a = a & b;
        return a;
    }

    enum class KeyboardLed : uint8_t
    {
        None       = 0u,
        NumLock    = 1u << 0,
        CapsLock   = 1u << 1,
        ScrollLock = 1u << 2,
        Compose    = 1u << 3,
        Kana       = 1u << 4,
    };

    struct MouseReport
    {
        MouseBtn buttons = MouseBtn::None;
        int8_t   x       = 0;
        int8_t   y       = 0;
        int8_t   wheel   = 0;
    } __attribute__((packed));

    // clang-format off
    // 说明：
    // Report ID 1:
    //   1 byte modifier + 15 byte bitmap
    //   bitmap 覆盖 HID usage 0x00 ~ 0x77
    //   0x77 足够包含字母、数字、方向键、F1~F24、导航键、小键盘等
    //
    // Report ID 2:
    //   Consumer Control, 16-bit usage, 一次一个媒体键
    //
    // Report ID 3:
    //   System Control bitmap:
    //     bit0 = Power Down
    //     bit1 = Sleep
    //     bit2 = Wake Up
    static const uint8_t nkro_report_desc[] = {
        // ---------- Keyboard NKRO ----------
        0x05, 0x01,        // Usage Page (Generic Desktop)
        0x09, 0x06,        // Usage (Keyboard)
        0xA1, 0x01,        // Collection (Application)
        0x85, 0x01,        //   Report ID (1)

        // Modifiers: E0 ~ E7
        0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
        0x19, 0xE0,        //   Usage Minimum (LeftControl)
        0x29, 0xE7,        //   Usage Maximum (Right GUI)
        0x15, 0x00,        //   Logical Minimum (0)
        0x25, 0x01,        //   Logical Maximum (1)
        0x75, 0x01,        //   Report Size (1)
        0x95, 0x08,        //   Report Count (8)
        0x81, 0x02,        //   Input (Data,Var,Abs)

        // NKRO bitmap: usage 0x00 ~ 0x77, 120 bits = 15 bytes
        0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
        0x19, 0x00,        //   Usage Minimum (0)
        0x29, 0x77,        //   Usage Maximum (0x77)
        0x15, 0x00,        //   Logical Minimum (0)
        0x25, 0x01,        //   Logical Maximum (1)
        0x75, 0x01,        //   Report Size (1)
        0x95, 0x78,        //   Report Count (120)
        0x81, 0x02,        //   Input (Data,Var,Abs)

        // Keyboard LEDs: NumLock/CapsLock/ScrollLock/Compose/Kana
        0x05, 0x08,        //   Usage Page (LEDs)
        0x19, 0x01,        //   Usage Minimum (Num Lock)
        0x29, 0x05,        //   Usage Maximum (Kana)
        0x75, 0x01,        //   Report Size (1)
        0x95, 0x05,        //   Report Count (5)
        0x91, 0x02,        //   Output (Data,Var,Abs)

        // LED padding
        0x75, 0x03,        //   Report Size (3)
        0x95, 0x01,        //   Report Count (1)
        0x91, 0x03,        //   Output (Const,Var,Abs)

        // Feature report: 32-byte vendor-defined key mapping command
        0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
        0x09, 0x01,        //   Usage (Vendor Usage 1)
        0x15, 0x00,        //   Logical Minimum (0)
        0x26, 0xFF, 0x00,  //   Logical Maximum (255)
        0x75, 0x08,        //   Report Size (8)
        0x95, 0x20,        //   Report Count (32)
        0xB1, 0x02,        //   Feature (Data,Var,Abs)
        0xC0,              // End Collection

        // ---------- Consumer Control ----------
        0x05, 0x0C,        // Usage Page (Consumer)
        0x09, 0x01,        // Usage (Consumer Control)
        0xA1, 0x01,        // Collection (Application)
        0x85, 0x02,        //   Report ID (2)
        0x15, 0x00,        //   Logical Minimum (0)
        0x26, 0xFF, 0x03,  //   Logical Maximum (0x03FF)
        0x19, 0x00,        //   Usage Minimum (0)
        0x2A, 0xFF, 0x03,  //   Usage Maximum (0x03FF)
        0x75, 0x10,        //   Report Size (16)
        0x95, 0x01,        //   Report Count (1)
        0x81, 0x00,        //   Input (Data,Array,Abs)
        0xC0,              // End Collection

        // ---------- System Control ----------
        0x05, 0x01,        // Usage Page (Generic Desktop)
        0x09, 0x80,        // Usage (System Control)
        0xA1, 0x01,        // Collection (Application)
        0x85, 0x03,        //   Report ID (3)
        0x15, 0x00,        //   Logical Minimum (0)
        0x25, 0x01,        //   Logical Maximum (1)
        0x75, 0x01,        //   Report Size (1)
        0x95, 0x03,        //   Report Count (3)
        0x09, 0x81,        //   Usage (System Power Down)
        0x09, 0x82,        //   Usage (System Sleep)
        0x09, 0x83,        //   Usage (System Wake Up)
        0x81, 0x02,        //   Input (Data,Var,Abs)
        0x75, 0x05,        //   Report Size (5)
        0x95, 0x01,        //   Report Count (1)
        0x81, 0x03,        //   Input (Const,Var,Abs)
        0xC0,               // End Collection

        // ---------- Report ID 4: relative mouse / 相对位移鼠标 ----------
        0x05, 0x01,       // Usage Page (Generic Desktop)
        0x09, 0x02,       // Usage (Mouse)
        0xA1, 0x01,       // Collection (Application)
        0x85, 0x04,       //   Report ID (4)
        0x09, 0x01,       //   Usage (Pointer)
        0xA1, 0x00,       //   Collection (Physical)

        0x05, 0x09,       //     Usage Page (Button)
        0x19, 0x01,       //     Usage Minimum (Button 1)
        0x29, 0x05,       //     Usage Maximum (Button 5)
        0x15, 0x00,       //     Logical Minimum (0)
        0x25, 0x01,       //     Logical Maximum (1)
        0x75, 0x01,       //     Report Size (1 bit)
        0x95, 0x05,       //     Report Count (5)
        0x81, 0x02,       //     Input (Data, Variable, Absolute)
        0x75, 0x03,       //     Report Size (3-bit padding)
        0x95, 0x01,       //     Report Count (1)
        0x81, 0x03,       //     Input (Constant, Variable, Absolute)

        0x05, 0x01,       //     Usage Page (Generic Desktop)
        0x09, 0x30,       //     Usage (X)
        0x09, 0x31,       //     Usage (Y)
        0x09, 0x38,       //     Usage (Wheel)
        0x15, 0x81,       //     Logical Minimum (-127)
        0x25, 0x7F,       //     Logical Maximum (127)
        0x75, 0x08,       //     Report Size (8 bits)
        0x95, 0x03,       //     Report Count (3)
        0x81, 0x06,       //     Input (Data, Variable, Relative)
        0xC0,             //   End Collection
        0xC0,             // End Collection
    };

    enum class KeyId : uint8_t
    {
        None            = 0x00, // Reserved (no event indicated)
        // --- Error Keys ---
        KeyboardErrorRollOver  = 0x01,
        KeyboardPOSTFail       = 0x02,
        KeyboardErrorUndefined = 0x03,
        // --- Letters ---
        A = 0x04, B = 0x05, C = 0x06, D = 0x07, E = 0x08, F = 0x09,
        G = 0x0A, H = 0x0B, I = 0x0C, J = 0x0D, K = 0x0E, L = 0x0F,
        M = 0x10, N = 0x11, O = 0x12, P = 0x13, Q = 0x14, R = 0x15,
        S = 0x16, T = 0x17, U = 0x18, V = 0x19, W = 0x1A, X = 0x1B,
        Y = 0x1C, Z = 0x1D,
        // --- Numbers ---
        D1 = 0x1E, D2 = 0x1F, D3 = 0x20, D4 = 0x21,
        D5 = 0x22, D6 = 0x23, D7 = 0x24, D8 = 0x25,
        D9 = 0x26, D0 = 0x27,
        // --- Basic ---
        Return     = 0x28, // Enter
        Escape     = 0x29,
        Backspace  = 0x2A, // Delete (Backspace)
        Tab        = 0x2B,
        Space      = 0x2C,
        // --- Punctuation (US layout) ---
        Minus      = 0x2D, // - and _
        Equal      = 0x2E, // = and +
        LeftBrace  = 0x2F, // [ and {
        RightBrace = 0x30, // ] and }
        Backslash  = 0x31, // \ and |
        NonUSHash  = 0x32, // Non-US # and ~
        Semicolon  = 0x33, // ; and :
        Quote      = 0x34, // ' and "
        Grave      = 0x35, // ` and ~
        Comma      = 0x36, // , and <
        Period     = 0x37, // . and >
        Slash      = 0x38, // / and ?

        // --- Lock Keys ---
        CapsLock   = 0x39,
        // --- Function Keys ---
        F1  = 0x3A, F2  = 0x3B, F3  = 0x3C, F4  = 0x3D,
        F5  = 0x3E, F6  = 0x3F, F7  = 0x40, F8  = 0x41,
        F9  = 0x42, F10 = 0x43, F11 = 0x44, F12 = 0x45,
        // --- Navigation / Editing ---
        PrintScreen  = 0x46,
        ScrollLock   = 0x47,
        Pause        = 0x48, // Pause / Break
        Insert       = 0x49,
        Home         = 0x4A,
        PageUp       = 0x4B,
        Delete       = 0x4C, // Delete Forward
        End          = 0x4D,
        PageDown     = 0x4E,
        RightArrow   = 0x4F,
        LeftArrow    = 0x50,
        DownArrow    = 0x51,
        UpArrow      = 0x52,
        // --- Keypad ---
        NumLockClear = 0x53,        // Num Lock / Clear
        KeypadDivide = 0x54,        // Keypad /
        KeypadMultiply = 0x55,      // Keypad *
        KeypadMinus  = 0x56,        // Keypad -
        KeypadPlus   = 0x57,        // Keypad +
        KeypadEnter  = 0x58,        // Keypad Enter
        Keypad1 = 0x59, Keypad2 = 0x5A, Keypad3 = 0x5B, Keypad4 = 0x5C,
        Keypad5 = 0x5D, Keypad6 = 0x5E, Keypad7 = 0x5F, Keypad8 = 0x60,
        Keypad9 = 0x61, Keypad0 = 0x62, KeypadPeriod = 0x63,

        // --- Miscellaneous ---
        NonUSBackslash = 0x64,      // Non-US \ and |
        Application    = 0x65,      // Application (Menu key)
        Power          = 0x66,      // Keyboard Power
        KeypadEquals   = 0x67,      // Keypad =

        // --- F13 ~ F24 ---
        F13 = 0x68, F14 = 0x69, F15 = 0x6A, F16 = 0x6B,
        F17 = 0x6C, F18 = 0x6D, F19 = 0x6E, F20 = 0x6F,
        F21 = 0x70, F22 = 0x71, F23 = 0x72, F24 = 0x73,

        // --- More Miscellaneous ---
        Execute      = 0x74,
        Help         = 0x75,
        Menu         = 0x76,
        Select       = 0x77,
        Stop         = 0x78,
        Again        = 0x79,
        Undo         = 0x7A,
        Cut          = 0x7B,
        Copy         = 0x7C,
        Paste        = 0x7D,
        Find         = 0x7E,
        Mute         = 0x7F,
        VolumeUp     = 0x80,
        VolumeDown   = 0x81,
        // --- Locking keys ---
        LockingCapsLock    = 0x82,
        LockingNumLock     = 0x83,
        LockingScrollLock  = 0x84,
        KeypadComma        = 0x85,  // Keypad Comma (Brazilian)
        KeypadEqualSignAS400 = 0x86, // Keypad = (AS/400)
    //
        International1   = 0x87, // Ro
        International2   = 0x88, // Katakana/Hiragana
        International3   = 0x89, // Yen
        International4   = 0x8A, // Henkan
        International5   = 0x8B, // Muhenkan
        International6   = 0x8C, // PC9800 Keypad Comma
        International7   = 0x8D,
        International8   = 0x8E,
        International9   = 0x8F,

        Lang1 = 0x90, // Hangul/English (Korean)
        Lang2 = 0x91, // Hanja (Korean)
        Lang3 = 0x92, // Katakana (Japanese)
        Lang4 = 0x93, // Hiragana (Japanese)
        Lang5 = 0x94, // Zenkaku/Hankaku (Japanese)
        Lang6 = 0x95,
        Lang7 = 0x96,
        Lang8 = 0x97,
        Lang9 = 0x98,

        AlternateErase          = 0x99,
        SysReq                  = 0x9A, // SysReq/Attention
        Cancel                  = 0x9B,
        Clear                   = 0x9C,
        Prior                   = 0x9D,
        Return2                 = 0x9E,
        Separator               = 0x9F,
        Out                     = 0xA0,
        Oper                    = 0xA1,
        ClearAgain              = 0xA2,
        CrSelProps              = 0xA3,
        ExSel                   = 0xA4,

        // Reserved 0xA5 ~ 0xAF

        // --- Keypad (cont.) ---
        Keypad00        = 0xB0,
        Keypad000       = 0xB1,
        ThousandsSep    = 0xB2,
        DecimalSep      = 0xB3,
        CurrencyUnit    = 0xB4,
        CurrencySubUnit = 0xB5,
        KeypadLeftParen = 0xB6,
        KeypadRightParen= 0xB7,
        KeypadLeftBrace = 0xB8,
        KeypadRightBrace= 0xB9,
        KeypadTab       = 0xBA,
        KeypadBackspace = 0xBB,
        KeypadA = 0xBC, KeypadB = 0xBD, KeypadC = 0xBE, KeypadD = 0xBF,
        KeypadE = 0xC0, KeypadF = 0xC1,

        KeypadXOR       = 0xC2,
        KeypadCaret     = 0xC3, // ^
        KeypadPercent   = 0xC4,
        KeypadLess      = 0xC5, // <
        KeypadGreater   = 0xC6, // >
        KeypadAmpersand = 0xC7, // &
        KeypadDoubleAmpersand = 0xC8,
        KeypadBar       = 0xC9, // |
        KeypadDoubleBar = 0xCA,
        KeypadColon     = 0xCB,
        KeypadHash      = 0xCC,
        KeypadSpace     = 0xCD,
        KeypadAt        = 0xCE,
        KeypadExclam    = 0xCF,
        KeypadMemStore  = 0xD0,
        KeypadMemRecall = 0xD1,
        KeypadMemClear  = 0xD2,
        KeypadMemAdd    = 0xD3,
        KeypadMemSub    = 0xD4,
        KeypadMemMul    = 0xD5,
        KeypadMemDiv    = 0xD6,
        KeypadPlusMinus = 0xD7,
        KeypadClear     = 0xD8,
        KeypadClearEntry= 0xD9,
        KeypadBinary    = 0xDA,
        KeypadOctal     = 0xDB,
        KeypadDecimal   = 0xDC,
        KeypadHex       = 0xDD,

        // Reserved 0xDE ~ 0xDF

        // --- Modifiers (sent as bitfield in modifier byte, not bitmap) ---
        LeftControl  = 0xE0,
        LeftShift    = 0xE1,
        LeftAlt      = 0xE2,
        LeftGUI      = 0xE3,
        RightControl = 0xE4,
        RightShift   = 0xE5,
        RightAlt     = 0xE6,
        RightGUI     = 0xE7,
    };

    constexpr static uint8_t  KEY_ID_MODIFIERS_START = 0xE0;
    constexpr static uint8_t  KEY_ID_MODIFIERS_END   = 0xE7;
    constexpr static uint8_t  KEY_ID_NKRO_MAX        = 0x77;
    constexpr static uint16_t HID_CONSUMER_USAGE_MAX = 0x03FF;
    constexpr static size_t   HID_FEATURE_MAX_SLOTS  = 256;

    // clang-format on

};
