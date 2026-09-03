#include "src/dvc/inc/BufferedMouseScanner.h"

#include <cassert>
#include <cstdint>

namespace
{
    void testLifecycle() {
        BufferedMouseScanner scanner;
        assert(scanner.scan().error().is<StdError::INVALID_STATE>());
        assert(scanner.queueMotion(1, 2, 3).error().is<StdError::INVALID_STATE>());
        assert(scanner.setup().is_ok());
        assert(scanner.setup().error().is<StdError::INVALID_STATE>());
        assert(scanner.end().is_ok());
        assert(scanner.scan().error().is<StdError::INVALID_STATE>());
    }

    void testMotionIsDrainedInHidSizedChunks() {
        BufferedMouseScanner scanner;
        assert(scanner.setup().is_ok());
        assert(scanner.queueMotion(300, -300, 255).is_ok());

        auto report = scanner.scan().unwrap();
        assert(report.x == 127 && report.y == -127 && report.wheel == 127);
        report = scanner.scan().unwrap();
        assert(report.x == 127 && report.y == -127 && report.wheel == 127);
        report = scanner.scan().unwrap();
        assert(report.x == 46 && report.y == -46 && report.wheel == 1);
        report = scanner.scan().unwrap();
        assert(report.x == 0 && report.y == 0 && report.wheel == 0);
    }

    void testButtonsPersistAndReset() {
        BufferedMouseScanner scanner;
        assert(scanner.setup().is_ok());
        assert(scanner.setButton(prt_hid::MouseBtn::MouseLeft, true).is_ok());
        assert(scanner.setButton(prt_hid::MouseBtn::MouseForward, true).is_ok());

        auto report = scanner.scan().unwrap();
        assert(static_cast<uint8_t>(report.buttons) == 0x11);
        report = scanner.scan().unwrap();
        assert(static_cast<uint8_t>(report.buttons) == 0x11);

        assert(scanner.setButton(static_cast<prt_hid::MouseBtn>(0x80), true).error().is<StdError::INVALID_ARGUMENT>());
        assert(static_cast<uint8_t>(scanner.scan().unwrap().buttons) == 0x11);

        assert(scanner.reset().is_ok());
        report = scanner.scan().unwrap();
        assert(report.buttons == prt_hid::MouseBtn::None);
        assert(!prt_hid::hasMouseMotion(report));
    }
}

int main() {
    testLifecycle();
    testMotionIsDrainedInHidSizedChunks();
    testButtonsPersistAndReset();
}
