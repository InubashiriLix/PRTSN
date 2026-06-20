#pragma once
#include "esp32-hal-adc.h"
#include "esp32-hal-gpio.h"

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

inline void stdPinMode(uint8_t pin, StdPinFunc mode) {
    pinMode(pin, static_cast<uint8_t>(mode));
}

inline void stdPinWrite(uint8_t pin, StdPinLevel level) {
    digitalWrite(pin, static_cast<uint8_t>(level));
}

inline void StdPinToggle(uint8_t pin) {
    digitalWrite(pin, !digitalRead(pin));
}

inline StdPinLevel stdPinRead(uint8_t pin) {
    return static_cast<StdPinLevel>(digitalRead(pin));
}

inline void StdAttachInterrupt(uint8_t pin, void (*isr)(void), StdIterruptMode mode) {
    attachInterrupt(pin, isr, static_cast<uint8_t>(mode));
}

inline void StdAttachInterruptArg(uint8_t pin, void (*isr)(void), void* arg, StdIterruptMode mode) {
    attachInterruptArg(pin, reinterpret_cast<void (*)(void*)>(isr), arg, static_cast<int>(mode));
}

inline void StdDetachInterrupt(uint8_t pin) {
    detachInterrupt(pin);
}

inline void StdEnableInterrupt(uint8_t pin) {
    enableInterrupt(pin);
}

inline void StdDisableInterrupt(uint8_t pin) {
    disableInterrupt(pin);
    analogRead(pin);
}
