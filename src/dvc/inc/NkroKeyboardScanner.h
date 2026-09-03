#pragma once

#include "src/alg/inc/alg_matrix.h"
#include "src/dvc/inc/IKeyboardScanDevice.h"
#include "src/fw/inc/std_pinMode.h"
#include "src/prt/HidProtocol.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstddef>
#include <cstdint>
#include <limits>

template <size_t RowNums, size_t ColNums>
class NkroKeyboardScanner : public IKeyboardScanDevice
{
    static_assert(RowNums > 0 && ColNums > 0, "Keyboard matrix dimensions must be greater than zero");
    static_assert(RowNums <= std::numeric_limits<uint8_t>::max(), "Keyboard row count must fit in uint8_t");
    static_assert(ColNums <= std::numeric_limits<uint8_t>::max(), "Keyboard column count must fit in uint8_t");
    static_assert(RowNums <= std::numeric_limits<uint16_t>::max() / ColNums, "Keyboard slot count must fit in uint16_t");

public:
    struct Config
    {
        uint32_t                                  scanIntervalMs   = 1;
        uint32_t                                  debounceMs       = 20;
        uint32_t                                  longPressMs      = 600;
        StdPinLevel                               activeLevel      = StdPinLevel::High;
        gpio_num_t                                rowPins[RowNums] = {};
        StdPinFunc                                rowPinMode       = StdPinFunc::InputPulldown;
        gpio_num_t                                colPins[ColNums] = {};
        StdPinFunc                                colPinMode       = StdPinFunc::Output;
        Matrix<RowNums, ColNums, uint32_t>&       KeyStateMatrix;
        Matrix<RowNums, ColNums, prt_hid::KeyId>& KeyIdMapMatrix;
        Matrix<RowNums, ColNums, prt_hid::KeyId>& LongKeyIdMapMatrix;
    };

    explicit NkroKeyboardScanner(Config& config)
        : m_config(config) {
        m_config.KeyStateMatrix.clear();
        m_snapshot = {};
    }

    Result<void, StdErrors> setup() override {
        if (m_initialized) {
            return Err<StdError::INVALID_STATE>();
        }
        if (m_config.debounceMs == 0 || m_config.longPressMs < m_config.debounceMs) {
            return Err<StdError::INVALID_ARGUMENT>();
        }

        const StdPinLevel inactiveLevel = m_config.activeLevel == StdPinLevel::High ? StdPinLevel::Low : StdPinLevel::High;

        for (size_t col = 0; col < ColNums; ++col) {
            stdPinMode(m_config.colPins[col], m_config.colPinMode);
            stdPinWrite(m_config.colPins[col], inactiveLevel);
        }
        for (size_t row = 0; row < RowNums; ++row) {
            stdPinMode(m_config.rowPins[row], m_config.rowPinMode);
        }

        m_config.KeyStateMatrix.clear();
        m_snapshot            = {};
        m_lastScanTimestampMs = 0;
        m_initialized         = true;
        return Ok();
    }

    Result<void, StdErrors> end() override {
        if (!m_initialized) {
            return Err<StdError::INVALID_STATE>();
        }

        const StdPinLevel inactiveLevel = m_config.activeLevel == StdPinLevel::High ? StdPinLevel::Low : StdPinLevel::High;
        for (size_t col = 0; col < ColNums; ++col) {
            stdPinWrite(m_config.colPins[col], inactiveLevel);
        }

        m_config.KeyStateMatrix.clear();
        m_snapshot            = {};
        m_lastScanTimestampMs = 0;
        m_initialized         = false;
        return Ok();
    }

    Result<void, StdErrors> reset() override {
        if (m_resetting) {
            return Err<StdError::INVALID_STATE>();
        }

        m_resetting = true;
        m_config.KeyStateMatrix.clear();
        m_snapshot            = {};
        m_lastScanTimestampMs = 0;
        m_resetting           = false;
        return Ok();
    }

    Result<prt_hid::KeyboardReport, StdErrors> scan() override {
        if (!m_initialized || m_resetting) {
            return Err<StdError::INVALID_STATE>();
        }

        const uint32_t          nowMs         = pdTICKS_TO_MS(xTaskGetTickCount());
        const uint32_t          elapsedMs     = m_lastScanTimestampMs == 0 ? 0 : nowMs - m_lastScanTimestampMs;
        const StdPinLevel       inactiveLevel = m_config.activeLevel == StdPinLevel::High ? StdPinLevel::Low : StdPinLevel::High;
        prt_hid::KeyboardReport nextReport {};
        bool                    invalidMapping = false;

        for (size_t col = 0; col < ColNums; ++col) {
            stdPinWrite(m_config.colPins[col], m_config.activeLevel);

            for (size_t row = 0; row < RowNums; ++row) {
                uint32_t& activeMs = m_config.KeyStateMatrix.data[row][col];
                if (stdPinRead(m_config.rowPins[row]) == m_config.activeLevel) {
                    if (activeMs <= std::numeric_limits<uint32_t>::max() - elapsedMs) {
                        activeMs += elapsedMs;
                    }
                    else {
                        activeMs = std::numeric_limits<uint32_t>::max();
                    }

                    if (activeMs >= m_config.debounceMs) {
                        auto key = m_config.KeyIdMapMatrix.data[row][col];
                        if (activeMs >= m_config.longPressMs && m_config.LongKeyIdMapMatrix.data[row][col] != prt_hid::KeyId::None) {
                            key = m_config.LongKeyIdMapMatrix.data[row][col];
                        }
                        if (key != prt_hid::KeyId::None && !prt_hid::setKeyboardKey(nextReport, key, true)) {
                            invalidMapping = true;
                        }
                    }
                }
                else {
                    activeMs = 0;
                }
            }

            stdPinWrite(m_config.colPins[col], inactiveLevel);
        }

        m_lastScanTimestampMs = nowMs;

        if (m_config.scanIntervalMs > 0) {
            vTaskDelay(pdMS_TO_TICKS(m_config.scanIntervalMs));
        }
        if (invalidMapping) {
            return Err<StdError::INVALID_ARGUMENT>();
        }

        m_snapshot = nextReport;
        return Ok(m_snapshot);
    }

    Result<prt_hid::KeyboardReport, StdErrors> snapshot() override {
        if (!m_initialized || m_resetting) {
            return Err<StdError::INVALID_STATE>();
        }
        return Ok(m_snapshot);
    }

    Result<prt_hid::KeyId, StdErrors> setShortKey(uint16_t slot, prt_hid::KeyId key) {
        return setKeyMapping(m_config.KeyIdMapMatrix, slot, key);
    }

    Result<prt_hid::KeyId, StdErrors> setLongKey(uint16_t slot, prt_hid::KeyId key) {
        return setKeyMapping(m_config.LongKeyIdMapMatrix, slot, key);
    }

private:
    Result<prt_hid::KeyId, StdErrors> setKeyMapping(Matrix<RowNums, ColNums, prt_hid::KeyId>& keyMap, uint16_t slot, prt_hid::KeyId key) {
        if (slot >= RowNums * ColNums || (key != prt_hid::KeyId::None && !prt_hid::isKeyboardKey(key))) {
            return Err<StdError::INVALID_ARGUMENT>();
        }

        const size_t row      = slot / ColNums;
        const size_t col      = slot % ColNums;
        const auto   previous = keyMap.data[row][col];
        keyMap.data[row][col] = key;
        return Ok(previous);
    }

    Config&                 m_config;
    bool                    m_initialized         = false;
    bool                    m_resetting           = false;
    uint32_t                m_lastScanTimestampMs = 0;
    prt_hid::KeyboardReport m_snapshot {};
};
