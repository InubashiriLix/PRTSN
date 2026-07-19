#pragma once

#include "src/alg/inc/alg_matrix.h"
#include "src/dvc/inc/IKeyboardScanDevice.h"
#include "src/fw/inc/std_pinMode.h"
#include "src/prt/HidProtocol.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
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
        uint8_t                                   rowPins[RowNums] = {};
        StdPinFunc                                rowPinMode       = StdPinFunc::InputPulldown;
        uint8_t                                   colPins[ColNums] = {};
        StdPinFunc                                colPinMode       = StdPinFunc::Output;
        Matrix<RowNums, ColNums, uint32_t>&       KeyStateMatrix;
        Matrix<RowNums, ColNums, prt_hid::KeyId>& KeyIdMapMatrix;
        Matrix<RowNums, ColNums, prt_hid::KeyId>& LongKeyIdMapMatrix;
    };

    explicit NkroKeyboardScanner(Config& config)
        : m_config(config) {
        m_config.KeyStateMatrix.clear();
        std::memset(m_pressedBitmap, 0, sizeof(m_pressedBitmap));
        std::memset(m_previousPressedBitmap, 0, sizeof(m_previousPressedBitmap));
        m_snapshot = makeSnapshot(0, false);
    }

    Result<void, StdError> setup() override {
        if (m_initialized) {
            return Err(StdError::INVALID_STATE);
        }
        if (m_config.debounceMs == 0 || m_config.longPressMs < m_config.debounceMs) {
            return Err(StdError::INVALID_ARGUMENT);
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
        std::memset(m_pressedBitmap, 0, sizeof(m_pressedBitmap));
        std::memset(m_previousPressedBitmap, 0, sizeof(m_previousPressedBitmap));
        m_snapshot    = makeSnapshot(0, false);
        m_initialized = true;
        return Ok();
    }

    Result<void, StdError> end() override {
        if (!m_initialized) {
            return Err(StdError::INVALID_STATE);
        }

        const StdPinLevel inactiveLevel = m_config.activeLevel == StdPinLevel::High ? StdPinLevel::Low : StdPinLevel::High;
        for (size_t col = 0; col < ColNums; ++col) {
            stdPinWrite(m_config.colPins[col], inactiveLevel);
        }

        m_config.KeyStateMatrix.clear();
        std::memset(m_pressedBitmap, 0, sizeof(m_pressedBitmap));
        std::memset(m_previousPressedBitmap, 0, sizeof(m_previousPressedBitmap));
        m_snapshot    = makeSnapshot(0, false);
        m_initialized = false;
        return Ok();
    }

    Result<void, StdError> reset() override {
        if (m_resetting) {
            return Err(StdError::INVALID_STATE);
        }

        m_resetting = true;
        m_config.KeyStateMatrix.clear();
        std::memset(m_pressedBitmap, 0, sizeof(m_pressedBitmap));
        std::memset(m_previousPressedBitmap, 0, sizeof(m_previousPressedBitmap));
        m_snapshot  = makeSnapshot(0, false);
        m_resetting = false;
        return Ok();
    }

    Result<KeyboardScanFrame, StdError> scan() override {
        if (!m_initialized || m_resetting) {
            return Err(StdError::INVALID_STATE);
        }

        const uint32_t    nowMs         = pdTICKS_TO_MS(xTaskGetTickCount());
        const uint32_t    elapsedMs     = m_snapshot.timestemp == 0 ? 0 : nowMs - m_snapshot.timestemp;
        const StdPinLevel inactiveLevel = m_config.activeLevel == StdPinLevel::High ? StdPinLevel::Low : StdPinLevel::High;

        std::memset(m_pressedBitmap, 0, sizeof(m_pressedBitmap));

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
                        const size_t slot = row * ColNums + col;
                        m_pressedBitmap[slot >> 3] |= static_cast<uint8_t>(1u << (slot & 0x07));
                    }
                }
                else {
                    activeMs = 0;
                }
            }

            stdPinWrite(m_config.colPins[col], inactiveLevel);
        }

        const bool changed = std::memcmp(m_pressedBitmap, m_previousPressedBitmap, sizeof(m_pressedBitmap)) != 0;
        if (changed) {
            std::memcpy(m_previousPressedBitmap, m_pressedBitmap, sizeof(m_pressedBitmap));
        }
        m_snapshot = makeSnapshot(nowMs, changed);

        if (m_config.scanIntervalMs > 0) {
            vTaskDelay(pdMS_TO_TICKS(m_config.scanIntervalMs));
        }
        return Ok(m_snapshot);
    }

    Result<KeyboardScanFrame, StdError> snapshot() override {
        if (!m_initialized || m_resetting) {
            return Err(StdError::INVALID_STATE);
        }
        return Ok(m_snapshot);
    }

    uint8_t getRowNum() const override {
        return static_cast<uint8_t>(RowNums);
    }

    uint8_t getColNum() const override {
        return static_cast<uint8_t>(ColNums);
    }

    uint16_t slotCount() const override {
        return static_cast<uint16_t>(RowNums * ColNums);
    }

private:
    static constexpr size_t PressedBitmapSize = (RowNums * ColNums + 7) / 8;

    KeyboardScanFrame makeSnapshot(uint32_t timestamp, bool changed) const {
        return KeyboardScanFrame {
            m_pressedBitmap,
            PressedBitmapSize,
            static_cast<uint8_t>(RowNums),
            static_cast<uint8_t>(ColNums),
            static_cast<uint16_t>(RowNums * ColNums),
            timestamp,
            changed,
        };
    }

    Config&           m_config;
    bool              m_initialized = false;
    bool              m_resetting   = false;
    uint8_t           m_pressedBitmap[PressedBitmapSize] {};
    uint8_t           m_previousPressedBitmap[PressedBitmapSize] {};
    KeyboardScanFrame m_snapshot {};
};
