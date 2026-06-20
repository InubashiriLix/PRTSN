#include "src/dvc/inc/KeyBoard4x5.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"

#include <cstring>
#include <limits>

KeyBoard4x5::KeyBoard4x5(PinConfig& pinConfig, SerialConsoleService* debugConsole)
    : m_config(pinConfig),
      m_debugConsole(debugConsole) {}

KeyBoard4x5::~KeyBoard4x5() {
    reset();
}

bool KeyBoard4x5::setup() {
    if (m_config.colNum == 0 || m_config.colNum > MaxColNum || m_config.rowNum == 0 || m_config.rowNum > MaxRowNum ||
        m_config.debounceMs == 0 || m_config.longPressMs < m_config.debounceMs) {
        return false;
    }

    reset();

    for (uint8_t col = 0; col < m_config.colNum; ++col) {
        stdPinMode(m_config.colPins[col], m_config.colPinMode);
        stdPinWrite(m_config.colPins[col], StdPinLevel::Low);
    }

    for (uint8_t row = 0; row < m_config.rowNum; ++row) {
        stdPinMode(m_config.rowPins[row], m_config.rowPinMode);
    }

    return true;
}

bool KeyBoard4x5::update() {
    if (m_config.colNum == 0 || m_config.colNum > MaxColNum || m_config.rowNum == 0 || m_config.rowNum > MaxRowNum ||
        m_config.debounceMs == 0 || m_config.longPressMs < m_config.debounceMs) {
        return false;
    }

    const uint32_t nowMs     = pdTICKS_TO_MS(xTaskGetTickCount());
    const uint32_t elapsedMs = m_lastUpdateMs == 0 ? 0 : nowMs - m_lastUpdateMs;
    m_lastUpdateMs           = nowMs;

    for (uint8_t col = 0; col < m_config.colNum; ++col) {
        stdPinWrite(m_config.colPins[col], StdPinLevel::High);

        for (uint8_t row = 0; row < m_config.rowNum; ++row) {
            uint32_t&  activeMs = m_keyState[col][row];
            const bool active   = stdPinRead(m_config.rowPins[row]) == m_config.activeLevel;

            if (active) {
                const uint32_t prevActiveMs = activeMs;
                if (activeMs <= std::numeric_limits<uint32_t>::max() - elapsedMs) {
                    activeMs += elapsedMs;
                }
                else {
                    activeMs = std::numeric_limits<uint32_t>::max();
                }

                if (m_debugConsole != nullptr && prevActiveMs < m_config.debounceMs && activeMs >= m_config.debounceMs) {
                    m_debugConsole->info("keyboard col=%u row=%u pressed duration_ms=%lu",
                                         static_cast<unsigned>(col),
                                         static_cast<unsigned>(row),
                                         static_cast<unsigned long>(activeMs));
                }

                if (m_debugConsole != nullptr && activeMs >= m_config.longPressMs && !m_longPressLogged[col][row]) {
                    m_longPressLogged[col][row] = true;
                    m_debugConsole->info("keyboard col=%u row=%u long press duration_ms=%lu",
                                         static_cast<unsigned>(col),
                                         static_cast<unsigned>(row),
                                         static_cast<unsigned long>(activeMs));
                }
            }
            else {
                if (m_debugConsole != nullptr && activeMs >= m_config.debounceMs) {
                    m_debugConsole->info("keyboard col=%u row=%u released duration_ms=%lu",
                                         static_cast<unsigned>(col),
                                         static_cast<unsigned>(row),
                                         static_cast<unsigned long>(activeMs));
                }

                activeMs                    = 0;
                m_longPressLogged[col][row] = false;
            }
        }

        stdPinWrite(m_config.colPins[col], StdPinLevel::Low);
    }

    if (m_config.scanIntervalMs > 0) {
        vTaskDelay(pdMS_TO_TICKS(m_config.scanIntervalMs));
    }

    return true;
}

void KeyBoard4x5::reset() {
    std::memset(m_keyState, 0, sizeof(m_keyState));
    std::memset(m_longPressLogged, 0, sizeof(m_longPressLogged));
    m_lastUpdateMs = 0;
}
