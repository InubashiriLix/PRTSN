#include "src/prt/HidProtocol.h"

#include <cassert>
#include <cstdint>

namespace
{
    constexpr bool constexprReportOperationsWork() {
        prt_hid::KeyboardReport report {};
        if (!prt_hid::setKeyboardKey(report, prt_hid::KeyId::A, true)) {
            return false;
        }
        if (!prt_hid::setKeyboardKey(report, prt_hid::KeyId::LeftShift, true)) {
            return false;
        }
        if ((report.keys[0] & (1u << 4)) == 0 || report.modifiers != (1u << 1)) {
            return false;
        }
        if (!prt_hid::setKeyboardKey(report, prt_hid::KeyId::A, false)) {
            return false;
        }
        return report.keys[0] == 0 && report.modifiers == (1u << 1);
    }

    static_assert(sizeof(prt_hid::KeyboardReport) == 16);
    static_assert(sizeof(prt_hid::MouseReport) == 4);
    static_assert(constexprReportOperationsWork());

    void testNormalAndModifierKeys() {
        prt_hid::KeyboardReport report {};

        assert(prt_hid::setKeyboardKey(report, prt_hid::KeyId::A, true));
        assert(prt_hid::setKeyboardKey(report, prt_hid::KeyId::F24, true));
        assert(prt_hid::setKeyboardKey(report, prt_hid::KeyId::LeftControl, true));
        assert(prt_hid::setKeyboardKey(report, prt_hid::KeyId::RightGUI, true));

        assert((report.keys[0] & (1u << 4)) != 0);
        assert((report.keys[0x73 >> 3] & (1u << (0x73 & 0x07))) != 0);
        assert(report.modifiers == ((1u << 0) | (1u << 7)));

        assert(prt_hid::setKeyboardKey(report, prt_hid::KeyId::A, false));
        assert(prt_hid::setKeyboardKey(report, prt_hid::KeyId::RightGUI, false));
        assert((report.keys[0] & (1u << 4)) == 0);
        assert(report.modifiers == (1u << 0));
    }

    void testUnsupportedKeysDoNotMutateReport() {
        prt_hid::KeyboardReport report {};
        assert(prt_hid::setKeyboardKey(report, prt_hid::KeyId::Escape, true));
        const auto expected = report;

        assert(!prt_hid::setKeyboardKey(report, prt_hid::KeyId::None, true));
        assert(!prt_hid::setKeyboardKey(report, static_cast<prt_hid::KeyId>(0x78), true));
        assert(prt_hid::keyboardReportsEqual(report, expected));
    }

    void testReportEquality() {
        prt_hid::KeyboardReport lhs {};
        prt_hid::KeyboardReport rhs {};
        assert(prt_hid::keyboardReportsEqual(lhs, rhs));

        assert(prt_hid::setKeyboardKey(lhs, prt_hid::KeyId::KeypadEnter, true));
        assert(!prt_hid::keyboardReportsEqual(lhs, rhs));
        assert(prt_hid::setKeyboardKey(rhs, prt_hid::KeyId::KeypadEnter, true));
        assert(prt_hid::keyboardReportsEqual(lhs, rhs));

        assert(prt_hid::setKeyboardKey(lhs, prt_hid::KeyId::RightAlt, true));
        assert(!prt_hid::keyboardReportsEqual(lhs, rhs));
    }

    void testMouseProtocol() {
        prt_hid::MouseBtn buttons = prt_hid::MouseBtn::None;
        assert(prt_hid::setMouseButton(buttons, prt_hid::MouseBtn::MouseLeft, true));
        assert(prt_hid::setMouseButton(buttons, prt_hid::MouseBtn::MouseForward, true));
        assert(static_cast<uint8_t>(buttons) == 0x11);
        assert(prt_hid::setMouseButton(buttons, prt_hid::MouseBtn::MouseLeft, false));
        assert(buttons == prt_hid::MouseBtn::MouseForward);

        const auto unchanged = buttons;
        assert(!prt_hid::setMouseButton(buttons, static_cast<prt_hid::MouseBtn>(0x80), true));
        assert(buttons == unchanged);

        assert(prt_hid::clampMouseDelta(-1000) == -127);
        assert(prt_hid::clampMouseDelta(-42) == -42);
        assert(prt_hid::clampMouseDelta(1000) == 127);

        const prt_hid::MouseReport still {.buttons = buttons};
        const prt_hid::MouseReport moving {.buttons = buttons, .x = 1, .y = -1, .wheel = 1};
        const prt_hid::MouseReport invalidDelta {.x = INT8_MIN};
        const prt_hid::MouseReport invalidButtons {.buttons = static_cast<prt_hid::MouseBtn>(0x80)};
        assert(prt_hid::isMouseReportValid(still));
        assert(!prt_hid::hasMouseMotion(still));
        assert(prt_hid::isMouseReportValid(moving));
        assert(prt_hid::hasMouseMotion(moving));
        assert(!prt_hid::isMouseReportValid(invalidDelta));
        assert(!prt_hid::isMouseReportValid(invalidButtons));
    }
}

int main() {
    testNormalAndModifierKeys();
    testUnsupportedKeysDoNotMutateReport();
    testReportEquality();
    testMouseProtocol();
}
