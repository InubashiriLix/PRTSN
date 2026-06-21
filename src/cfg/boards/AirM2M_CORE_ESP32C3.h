#pragma once

#include "src/cfg/pin/PinTypes.h"

#include <cstddef>
#include <cstdint>

#ifndef PRTN_BOARD_NAME
#define PRTN_BOARD_NAME "AirM2M_CORE_ESP32-C3"
#endif

#define PRTN_BOARD_PIN_LIST(X)                                                                               \
    X(GPIO0, 0, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::UartTx | ::prtn::pin::PinCap::Strapping) \
    X(GPIO1, 1, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::UartRx)                                  \
    X(GPIO2, 2, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::SpiSck | ::prtn::pin::PinCap::Pwm)       \
    X(GPIO3, 3, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::SpiMosi)                                 \
    X(GPIO4, 4, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::I2cSda | ::prtn::pin::PinCap::Analog)    \
    X(GPIO5, 5, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::I2cScl)                                  \
    X(GPIO6, 6, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::Pwm)                                     \
    X(GPIO7, 7, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::SpiCs | ::prtn::pin::PinCap::RmtTx)      \
    X(GPIO8, 8, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::Pwm | ::prtn::pin::PinCap::RmtTx)        \
    X(GPIO9, 9, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::Boot | ::prtn::pin::PinCap::Strapping)   \
    X(GPIO10, 10, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::SpiMiso | ::prtn::pin::PinCap::Pwm)    \
    X(GPIO11, 11, ::prtn::pin::PinCap::Digital)                                                              \
    X(GPIO12, 12, ::prtn::pin::PinCap::Digital)                                                              \
    X(GPIO13, 13, ::prtn::pin::PinCap::Digital)                                                              \
    X(GPIO18, 18, ::prtn::pin::PinCap::Digital)                                                              \
    X(GPIO19, 19, ::prtn::pin::PinCap::Digital)

namespace prtn::c3
{
    enum class BoardPin : uint8_t
    {
#define PRTN_BOARD_PIN_ENUM(name, gpio, caps) name = gpio,
        PRTN_BOARD_PIN_LIST(PRTN_BOARD_PIN_ENUM)
#undef PRTN_BOARD_PIN_ENUM
    };

    struct PinDef
    {
        BoardPin             pin;
        int                  gpio;
        ::prtn::pin::PinCaps caps;
        const char*          name;
    };

    inline constexpr PinDef PinDefs[] {
#define PRTN_BOARD_PIN_DEF(name, gpio, caps) {BoardPin::name, gpio, caps, #name},
        PRTN_BOARD_PIN_LIST(PRTN_BOARD_PIN_DEF)
#undef PRTN_BOARD_PIN_DEF
    };

    constexpr bool hasPin(BoardPin pin) {
        for (const auto& def : PinDefs) {
            if (def.pin == pin) {
                return true;
            }
        }
        return false;
    }

    constexpr int pinGpio(BoardPin pin) {
        for (const auto& def : PinDefs) {
            if (def.pin == pin) {
                return def.gpio;
            }
        }
        return -1;
    }

    constexpr ::prtn::pin::PinCaps pinCaps(BoardPin pin) {
        for (const auto& def : PinDefs) {
            if (def.pin == pin) {
                return def.caps;
            }
        }
        return ::prtn::pin::PinCap::None;
    }

    constexpr const char* pinName(BoardPin pin) {
        for (const auto& def : PinDefs) {
            if (def.pin == pin) {
                return def.name;
            }
        }
        return "<invalid>";
    }

    template <BoardPin Pin, ::prtn::pin::PinCaps Required>
    using TypedPin = ::prtn::pin::TypedPin<Pin, pinCaps(Pin), Required>;

    namespace gpio
    {
        inline constexpr TypedPin<BoardPin::GPIO0, pinCaps(BoardPin::GPIO0)>   gpio0 {};
        inline constexpr TypedPin<BoardPin::GPIO1, pinCaps(BoardPin::GPIO1)>   gpio1 {};
        inline constexpr TypedPin<BoardPin::GPIO2, pinCaps(BoardPin::GPIO2)>   gpio2 {};
        inline constexpr TypedPin<BoardPin::GPIO3, pinCaps(BoardPin::GPIO3)>   gpio3 {};
        inline constexpr TypedPin<BoardPin::GPIO4, pinCaps(BoardPin::GPIO4)>   gpio4 {};
        inline constexpr TypedPin<BoardPin::GPIO5, pinCaps(BoardPin::GPIO5)>   gpio5 {};
        inline constexpr TypedPin<BoardPin::GPIO6, pinCaps(BoardPin::GPIO6)>   gpio6 {};
        inline constexpr TypedPin<BoardPin::GPIO7, pinCaps(BoardPin::GPIO7)>   gpio7 {};
        inline constexpr TypedPin<BoardPin::GPIO8, pinCaps(BoardPin::GPIO8)>   gpio8 {};
        inline constexpr TypedPin<BoardPin::GPIO9, pinCaps(BoardPin::GPIO9)>   gpio9 {};
        inline constexpr TypedPin<BoardPin::GPIO10, pinCaps(BoardPin::GPIO10)> gpio10 {};
        inline constexpr TypedPin<BoardPin::GPIO11, pinCaps(BoardPin::GPIO11)> gpio11 {};
        inline constexpr TypedPin<BoardPin::GPIO12, pinCaps(BoardPin::GPIO12)> gpio12 {};
        inline constexpr TypedPin<BoardPin::GPIO13, pinCaps(BoardPin::GPIO13)> gpio13 {};
        inline constexpr TypedPin<BoardPin::GPIO18, pinCaps(BoardPin::GPIO18)> gpio18 {};
        inline constexpr TypedPin<BoardPin::GPIO19, pinCaps(BoardPin::GPIO19)> gpio19 {};
    }

    namespace spi
    {
        inline constexpr decltype(gpio::gpio2)  sck  = gpio::gpio2;
        inline constexpr decltype(gpio::gpio3)  mosi = gpio::gpio3;
        inline constexpr decltype(gpio::gpio10) miso = gpio::gpio10;
        inline constexpr decltype(gpio::gpio7)  cs   = gpio::gpio7;
    }

    namespace i2c0
    {
        inline constexpr decltype(gpio::gpio4) sda = gpio::gpio4;
        inline constexpr decltype(gpio::gpio5) scl = gpio::gpio5;
    }

    namespace uart0
    {
        inline constexpr decltype(gpio::gpio0) tx = gpio::gpio0;
        inline constexpr decltype(gpio::gpio1) rx = gpio::gpio1;
    }

    namespace rmt
    {
        inline constexpr decltype(gpio::gpio7) tx0 = gpio::gpio7;
    }
}
