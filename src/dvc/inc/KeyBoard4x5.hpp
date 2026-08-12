#pragma once

#include "src/fw/inc/std_pinMode.h"
#include "src/svc/inc/SerialConsoleService.h"

#include <cstdint>

class KeyBoard4x5
{
public:
    static constexpr uint8_t MaxColNum = 4;
    static constexpr uint8_t MaxRowNum = 5;

    struct PinConfig
    {
        uint32_t    scanIntervalMs     = 1;
        uint32_t    debounceMs         = 20;
        uint32_t    longPressMs        = 600;
        StdPinLevel activeLevel        = StdPinLevel::High;
        gpio_num_t  colPins[MaxColNum] = {GPIO_NUM_12, GPIO_NUM_18, GPIO_NUM_19};
        StdPinFunc  colPinMode         = StdPinFunc::Output;
        uint8_t     colNum             = 3;
        gpio_num_t  rowPins[MaxRowNum] = {GPIO_NUM_2, GPIO_NUM_3, GPIO_NUM_10};
        StdPinFunc  rowPinMode         = StdPinFunc::InputPulldown;
        uint8_t     rowNum             = 3;
    };

public:
    explicit KeyBoard4x5(PinConfig& pinConfig, SerialConsoleService* debugConsole = nullptr);
    ~KeyBoard4x5();

    bool setup();
    bool update();
    void reset();

private:
    uint32_t              m_keyState[MaxColNum][MaxRowNum]        = {};
    bool                  m_longPressLogged[MaxColNum][MaxRowNum] = {};
    uint32_t              m_lastUpdateMs                          = 0;
    PinConfig&            m_config;
    SerialConsoleService* m_debugConsole;
};
