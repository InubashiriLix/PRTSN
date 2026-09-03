#pragma once

#include "src/dvc/inc/IKeyboardScanDevice.h"
#include "src/dvc/inc/IMouseScanDevice.h"
#include "src/fw/inc/Result.h"
#include "src/fw/inc/std_err.h"
#include "src/prt/HidProtocol.h"
#include "src/svc/inc/NkroKeyboard.h"

class NkroKeyboardService
{
public:
    struct Config
    {
        IKeyboardScanDevice& scanDevice;
        NkroKeyboard&        keyboard;
        IMouseScanDevice*    mouseScanDevice = nullptr;
    };

    explicit NkroKeyboardService(Config& config)
        : m_config(config) {}

    Result<void, StdErrors> setup() {
        if (m_initialized) {
            return Err<StdError::INVALID_STATE>();
        }

        auto result = m_config.scanDevice.setup();
        if (result.is_err()) {
            return result;
        }

        if (m_config.mouseScanDevice != nullptr) {
            result = m_config.mouseScanDevice->setup();
            if (result.is_err()) {
                m_config.scanDevice.end();
                return result;
            }
        }

        result = m_config.keyboard.setup();
        if (result.is_err()) {
            if (m_config.mouseScanDevice != nullptr) {
                m_config.mouseScanDevice->end();
            }
            m_config.scanDevice.end();
            return result;
        }

        m_initialized = true;
        return Ok();
    }

    Result<void, StdErrors> end() {
        if (!m_initialized) {
            return Err<StdError::INVALID_STATE>();
        }

        auto firstError = m_config.keyboard.updateKeyboardState({});

        Result<void, StdErrors> result = Ok();
        if (m_config.mouseScanDevice != nullptr) {
            result = m_config.keyboard.updateMouseState({});
            if (firstError.is_ok() && result.is_err()) {
                firstError = result;
            }

            result = m_config.mouseScanDevice->end();
            if (firstError.is_ok() && result.is_err()) {
                firstError = result;
            }
        }

        result = m_config.scanDevice.end();
        if (firstError.is_ok() && result.is_err()) {
            firstError = result;
        }

        result = m_config.keyboard.end();
        if (firstError.is_ok() && result.is_err()) {
            firstError = result;
        }

        m_initialized = false;
        return firstError;
    }

    Result<void, StdErrors> reset() {
        if (!m_initialized) {
            return Err<StdError::INVALID_STATE>();
        }

        auto result = m_config.scanDevice.reset();
        if (result.is_err()) {
            return result;
        }

        if (m_config.mouseScanDevice != nullptr) {
            result = m_config.mouseScanDevice->reset();
            if (result.is_err()) {
                return result;
            }
        }

        if (m_config.mouseScanDevice != nullptr) {
            result = m_config.keyboard.updateMouseState({});
            if (result.is_err()) {
                return result;
            }
        }
        return m_config.keyboard.updateKeyboardState({});
    }

    Result<void, StdErrors> update() {
        if (!m_initialized) {
            return Err<StdError::INVALID_STATE>();
        }

        const auto scanResult = m_config.scanDevice.scan();
        if (scanResult.is_err()) {
            return scanResult.propagate();
        }

        prt_hid::MouseReport mouseReport {};
        if (m_config.mouseScanDevice != nullptr) {
            const auto mouseScanResult = m_config.mouseScanDevice->scan();
            if (mouseScanResult.is_err()) {
                return mouseScanResult.propagate();
            }
            mouseReport = mouseScanResult.unwrap();
        }

        const auto readyResult = m_config.keyboard.ready();
        if (readyResult.is_err()) {
            return readyResult.propagate();
        }
        if (!readyResult.unwrap()) {
            return Ok();
        }

        if (m_config.mouseScanDevice != nullptr) {
            const auto mouseResult = m_config.keyboard.updateMouseState(mouseReport);
            if (mouseResult.is_err()) {
                return mouseResult;
            }
        }
        return m_config.keyboard.updateKeyboardState(scanResult.unwrap());
    }

private:
    Config& m_config;
    bool    m_initialized = false;
};
