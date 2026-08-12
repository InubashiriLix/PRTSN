#pragma once

#include "src/alg/inc/alg_matrix.h"
#include "src/dvc/inc/IKeyboardScanDevice.h"
#include "src/fw/inc/Result.h"
#include "src/fw/inc/std_err.h"
#include "src/prt/HidProtocol.h"
#include "src/svc/inc/NkroKeyboard.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

template <size_t RowNums, size_t ColNums>
class NkroKeyboardService
{
    static_assert(RowNums > 0 && ColNums > 0, "Keyboard matrix dimensions must be greater than zero");
    static_assert(RowNums <= UINT8_MAX && ColNums <= UINT8_MAX, "Keyboard dimensions must fit in the scan frame");
    static_assert(RowNums <= UINT16_MAX / ColNums, "Keyboard slot count must fit in the scan frame");

public:
    struct Config
    {
        IKeyboardScanDevice&                      scanDevice;
        NkroKeyboard&                             keyboard;
        uint32_t                                  longPressMs;
        Matrix<RowNums, ColNums, uint32_t>&       KeyStateMatrix;
        Matrix<RowNums, ColNums, prt_hid::KeyId>& KeyIdMapMatrix;
        Matrix<RowNums, ColNums, prt_hid::KeyId>& LongKeyIdMapMatrix;
    };

    explicit NkroKeyboardService(Config& config)
        : m_config(config) {}

    Result<void, StdErrors> setup() {
        if (m_initialized) {
            return Err<StdError::INVALID_STATE>();
        }
        if (m_config.longPressMs == 0 || m_config.scanDevice.getRowNum() != RowNums || m_config.scanDevice.getColNum() != ColNums ||
            m_config.scanDevice.slotCount() != RowNums * ColNums) {
            return Err<StdError::INVALID_ARGUMENT>();
        }

        auto result = m_config.scanDevice.setup();
        if (result.is_err()) {
            return result;
        }

        result = m_config.keyboard.setup();
        if (result.is_err()) {
            m_config.scanDevice.end();
            return result;
        }

        std::memset(m_previousUsageBitmap, 0, sizeof(m_previousUsageBitmap));
        m_hidWasReady = false;
        m_initialized = true;
        return Ok();
    }

    Result<void, StdErrors> end() {
        if (!m_initialized) {
            return Err<StdError::INVALID_STATE>();
        }

        uint8_t emptyUsageBitmap[NkroKeyboard::KEY_USAGE_BITMAP_SIZE] {};
        auto    firstError = m_config.keyboard.updateKeyboardState(emptyUsageBitmap, sizeof(emptyUsageBitmap));

        auto result = m_config.scanDevice.end();
        if (firstError.is_ok() && result.is_err()) {
            firstError = result;
        }

        result = m_config.keyboard.end();
        if (firstError.is_ok() && result.is_err()) {
            firstError = result;
        }

        std::memset(m_previousUsageBitmap, 0, sizeof(m_previousUsageBitmap));
        m_hidWasReady = false;
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

        uint8_t emptyUsageBitmap[NkroKeyboard::KEY_USAGE_BITMAP_SIZE] {};
        result = m_config.keyboard.updateKeyboardState(emptyUsageBitmap, sizeof(emptyUsageBitmap));
        if (result.is_err()) {
            return result;
        }

        std::memset(m_previousUsageBitmap, 0, sizeof(m_previousUsageBitmap));
        m_hidWasReady = false;
        return Ok();
    }

    Result<void, StdErrors> update() {
        if (!m_initialized) {
            return Err<StdError::INVALID_STATE>();
        }

        auto scanResult = m_config.scanDevice.scan();
        if (scanResult.is_err()) {
            return scanResult.propagate();
        }

        const KeyboardScanFrame frame              = scanResult.unwrap();
        const size_t            requiredBitmapSize = (RowNums * ColNums + 7) / 8;
        if (frame.pressedBitmap == nullptr || frame.pressedBitmapSize < requiredBitmapSize || frame.rows != RowNums || frame.cols != ColNums ||
            frame.slotCount != RowNums * ColNums) {
            return Err<StdError::INVALID_RESPONSE>();
        }

        uint8_t usageBitmap[NkroKeyboard::KEY_USAGE_BITMAP_SIZE] {};
        for (size_t row = 0; row < RowNums; ++row) {
            for (size_t col = 0; col < ColNums; ++col) {
                const size_t slot = row * ColNums + col;
                if ((frame.pressedBitmap[slot >> 3] & static_cast<uint8_t>(1u << (slot & 0x07))) == 0) {
                    continue;
                }

                prt_hid::KeyId       keyId     = m_config.KeyIdMapMatrix.data[row][col];
                const prt_hid::KeyId longKeyId = m_config.LongKeyIdMapMatrix.data[row][col];
                if (m_config.KeyStateMatrix.data[row][col] >= m_config.longPressMs && longKeyId != prt_hid::KeyId::None) {
                    keyId = longKeyId;
                }

                const uint8_t usage = static_cast<uint8_t>(keyId);
                if (keyId == prt_hid::KeyId::None) {
                    continue;
                }
                if (usage > prt_hid::KEY_ID_NKRO_MAX &&
                    (usage < prt_hid::KEY_ID_MODIFIERS_START || usage > prt_hid::KEY_ID_MODIFIERS_END)) {
                    return Err<StdError::INVALID_ARGUMENT>();
                }
                usageBitmap[usage >> 3] |= static_cast<uint8_t>(1u << (usage & 0x07));
            }
        }

        const auto readyResult = m_config.keyboard.ready();
        if (readyResult.is_err()) {
            return readyResult.propagate();
        }
        if (!readyResult.unwrap()) {
            m_hidWasReady = false;
            return Ok();
        }
        if (!m_hidWasReady) {
            std::memset(m_previousUsageBitmap, 0, sizeof(m_previousUsageBitmap));
            m_hidWasReady = true;
        }

        if (std::memcmp(usageBitmap, m_previousUsageBitmap, sizeof(usageBitmap)) == 0) {
            return Ok();
        }

        auto result = m_config.keyboard.updateKeyboardState(usageBitmap, sizeof(usageBitmap));
        if (result.is_err()) {
            return result;
        }

        std::memcpy(m_previousUsageBitmap, usageBitmap, sizeof(usageBitmap));
        return Ok();
    }

    Result<prt_hid::KeyId, StdErrors> setShortKey(uint16_t slot, prt_hid::KeyId keyId) {
        return setKeyMapping(m_config.KeyIdMapMatrix, slot, keyId);
    }

    Result<prt_hid::KeyId, StdErrors> setLongKey(uint16_t slot, prt_hid::KeyId keyId) {
        return setKeyMapping(m_config.LongKeyIdMapMatrix, slot, keyId);
    }

private:
    Result<prt_hid::KeyId, StdErrors> setKeyMapping(Matrix<RowNums, ColNums, prt_hid::KeyId>& keyMap, uint16_t slot, prt_hid::KeyId keyId) {
        const uint8_t usage = static_cast<uint8_t>(keyId);
        if (slot >= RowNums * ColNums ||
            (keyId != prt_hid::KeyId::None && usage > prt_hid::KEY_ID_NKRO_MAX &&
             (usage < prt_hid::KEY_ID_MODIFIERS_START || usage > prt_hid::KEY_ID_MODIFIERS_END))) {
            return Err<StdError::INVALID_ARGUMENT>();
        }

        const size_t row           = slot / ColNums;
        const size_t col           = slot % ColNums;
        const auto   previousKeyId = keyMap.data[row][col];
        keyMap.data[row][col]      = keyId;
        return Ok(previousKeyId);
    }

    Config& m_config;
    bool    m_initialized = false;
    bool    m_hidWasReady = false;
    uint8_t m_previousUsageBitmap[NkroKeyboard::KEY_USAGE_BITMAP_SIZE] {};
};
