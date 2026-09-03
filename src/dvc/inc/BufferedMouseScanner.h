#pragma once

#include "src/dvc/inc/IMouseScanDevice.h"
#include "src/fw/inc/Result.h"
#include "src/fw/inc/std_err.h"
#include "src/prt/HidProtocol.h"

#include <cstdint>
#include <limits>

class BufferedMouseScanner final : public IMouseScanDevice
{
public:
    Result<void, StdErrors> setup() override {
        if (m_initialized) {
            return Err<StdError::INVALID_STATE>();
        }
        clear();
        m_initialized = true;
        return Ok();
    }

    Result<void, StdErrors> end() override {
        if (!m_initialized) {
            return Err<StdError::INVALID_STATE>();
        }
        clear();
        m_initialized = false;
        return Ok();
    }

    Result<void, StdErrors> reset() override {
        if (!m_initialized) {
            return Err<StdError::INVALID_STATE>();
        }
        clear();
        return Ok();
    }

    Result<void, StdErrors> queueMotion(int32_t x, int32_t y, int32_t wheel) {
        if (!m_initialized) {
            return Err<StdError::INVALID_STATE>();
        }

        m_pendingX     = saturatingAdd(m_pendingX, x);
        m_pendingY     = saturatingAdd(m_pendingY, y);
        m_pendingWheel = saturatingAdd(m_pendingWheel, wheel);
        return Ok();
    }

    Result<prt_hid::MouseBtn, StdErrors> setButton(prt_hid::MouseBtn buttonMask, bool pressed) {
        if (!m_initialized) {
            return Err<StdError::INVALID_STATE>();
        }

        const auto previous = m_buttons;
        if (!prt_hid::setMouseButton(m_buttons, buttonMask, pressed)) {
            return Err<StdError::INVALID_ARGUMENT>();
        }
        return Ok(previous);
    }

    Result<prt_hid::MouseReport, StdErrors> scan() override {
        if (!m_initialized) {
            return Err<StdError::INVALID_STATE>();
        }

        return Ok(prt_hid::MouseReport {
            .buttons = m_buttons,
            .x       = takeDelta(m_pendingX),
            .y       = takeDelta(m_pendingY),
            .wheel   = takeDelta(m_pendingWheel),
        });
    }

private:
    static int32_t saturatingAdd(int32_t current, int32_t delta) {
        const int64_t sum = static_cast<int64_t>(current) + delta;
        if (sum < std::numeric_limits<int32_t>::min()) {
            return std::numeric_limits<int32_t>::min();
        }
        if (sum > std::numeric_limits<int32_t>::max()) {
            return std::numeric_limits<int32_t>::max();
        }
        return static_cast<int32_t>(sum);
    }

    static int8_t takeDelta(int32_t& pending) {
        const int8_t delta = prt_hid::clampMouseDelta(pending);
        pending -= delta;
        return delta;
    }

    void clear() {
        m_pendingX     = 0;
        m_pendingY     = 0;
        m_pendingWheel = 0;
        m_buttons      = prt_hid::MouseBtn::None;
    }

    int32_t           m_pendingX {0};
    int32_t           m_pendingY {0};
    int32_t           m_pendingWheel {0};
    prt_hid::MouseBtn m_buttons {prt_hid::MouseBtn::None};
    bool              m_initialized {false};
};
