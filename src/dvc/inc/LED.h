#pragma once

#include <cstdint>
#include "driver/gpio.h"
#include "esp32-hal-gpio.h"

class LED
{
public:
    static constexpr gpio_num_t DefaultMainPin = GPIO_NUM_12;
    static constexpr gpio_num_t DefaultAuxPin  = GPIO_NUM_13;

    enum class State : uint8_t
    {
        DIGITAL_HIGH = HIGH,
        DIGITAL_LOW  = LOW,
    };

private:
    gpio_num_t m_pin;
    State      m_state;

public:
    LED(gpio_num_t pin, State ledInitState = State::DIGITAL_HIGH);
    bool  setup();
    State getState();
    State setState(State state);
    State toggleState();
    State toogleState();
    bool  update();
    bool  updateByIntervalMs(uint32_t updateIntervalMs);
    bool  updateByIntervalNum(uint32_t updateIntervalNum);
};
