#pragma once
#include "esp32-hal-adc.h"
#include "esp32-hal-gpio.h"
#include "driver/gpio.h"

enum class StdPinLevel : uint8_t
{
    Low  = LOW,
    High = HIGH,
};

enum class StdPinFunc : uint8_t
{
    Input           = INPUT,
    Output          = OUTPUT,
    Pullup          = PULLUP,
    InputPullup     = INPUT_PULLUP,
    Pulldown        = PULLDOWN,
    InputPulldown   = INPUT_PULLDOWN,
    OpenDrain       = OPEN_DRAIN,
    OutputOpenDrain = OUTPUT_OPEN_DRAIN,
    Analog          = ANALOG,
};

enum class StdIterruptMode : uint8_t
{
    Disabled  = DISABLED,
    Rising    = RISING,
    Falling   = FALLING,
    Change    = CHANGE,
    OnLow     = ONLOW,
    OnHigh    = ONHIGH,
    Onlow_we  = ONLOW_WE,
    Onhigh_we = ONHIGH_WE,
};

inline void stdPinMode(gpio_num_t pin, StdPinFunc mode) {
    pinMode(static_cast<uint8_t>(pin), static_cast<uint8_t>(mode));
}

inline void stdPinWrite(gpio_num_t pin, StdPinLevel level) {
    digitalWrite(static_cast<uint8_t>(pin), static_cast<uint8_t>(level));
}

inline void StdPinToggle(gpio_num_t pin) {
    const uint8_t arduinoPin = static_cast<uint8_t>(pin);
    digitalWrite(arduinoPin, !digitalRead(arduinoPin));
}

inline StdPinLevel stdPinRead(gpio_num_t pin) {
    return static_cast<StdPinLevel>(digitalRead(static_cast<uint8_t>(pin)));
}

inline void StdAttachInterrupt(gpio_num_t pin, void (*isr)(void), StdIterruptMode mode) {
    attachInterrupt(static_cast<uint8_t>(pin), isr, static_cast<uint8_t>(mode));
}

inline void StdAttachInterruptArg(gpio_num_t pin, void (*isr)(void), void* arg, StdIterruptMode mode) {
    attachInterruptArg(static_cast<uint8_t>(pin), reinterpret_cast<void (*)(void*)>(isr), arg, static_cast<int>(mode));
}

inline void StdDetachInterrupt(gpio_num_t pin) {
    detachInterrupt(static_cast<uint8_t>(pin));
}

inline void StdEnableInterrupt(gpio_num_t pin) {
    enableInterrupt(static_cast<uint8_t>(pin));
}

inline void StdDisableInterrupt(gpio_num_t pin) {
    const uint8_t arduinoPin = static_cast<uint8_t>(pin);
    disableInterrupt(arduinoPin);
    analogRead(arduinoPin);
}
