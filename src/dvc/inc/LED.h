#pragma once

#include <cstdint>
#include "esp32-hal-gpio.h"

class LED
{
public:
    enum class State : uint8_t
    {
        DIGITAL_HIGH = HIGH,
        DIGITAL_LOW  = LOW,
    };

private:
    uint8_t m_pin;
    State   m_state;

public:
    LED(uint8_t pin, State ledInitState = State::DIGITAL_HIGH);
    bool  setup();
    State getState();
    State setState(State state);
    State toggleState();
    State toogleState();
    bool  update();
    bool  updateByIntervalMs(uint32_t updateIntervalMs);
    bool  updateByIntervalNum(uint32_t updateIntervalNum);
};
