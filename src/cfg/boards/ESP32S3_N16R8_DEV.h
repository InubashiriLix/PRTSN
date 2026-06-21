#pragma once

#include "src/cfg/pin/PinTypes.h"

#include <cstddef>
#include <cstdint>

#ifndef PRTN_BOARD_NAME
#define PRTN_BOARD_NAME "ESP32-S3-N16R8_Dev_Board"
#endif

#define PRTN_BOARD_PIN_LIST(X)                                                                             \
    X(GPIO0, 0, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::Boot | ::prtn::pin::PinCap::Strapping) \
    X(GPIO1, 1, ::prtn::pin::PinCap::Digital)                                                              \
    X(GPIO2, 2, ::prtn::pin::PinCap::Digital)                                                              \
    X(GPIO3, 3, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::Strapping)                             \
    X(GPIO4, 4, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::Analog)                                \
    X(GPIO5, 5, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::Analog)                                \
    X(GPIO6, 6, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::Analog)                                \
    X(GPIO7, 7, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::Analog)                                \
    X(GPIO8, 8, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::Analog)                                \
    X(GPIO9, 9, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::Analog)                                \
    X(GPIO10, 10, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::Analog)                              \
    X(GPIO11, 11, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::Analog)                              \
    X(GPIO12, 12, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::Analog)                              \
    X(GPIO13, 13, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::Analog)                              \
    X(GPIO14, 14, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::Analog)                              \
    X(GPIO15, 15, ::prtn::pin::PinCap::Digital)                                                            \
    X(GPIO16, 16, ::prtn::pin::PinCap::Digital)                                                            \
    X(GPIO17, 17, ::prtn::pin::PinCap::Digital)                                                            \
    X(GPIO18, 18, ::prtn::pin::PinCap::Digital)                                                            \
    X(GPIO19, 19, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::UsbDm)                               \
    X(GPIO20, 20, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::UsbDp)                               \
    X(GPIO21, 21, ::prtn::pin::PinCap::Digital)                                                            \
    X(GPIO35, 35, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::SpiCs)                               \
    X(GPIO36, 36, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::SpiSck)                              \
    X(GPIO37, 37, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::SpiMosi)                             \
    X(GPIO38, 38, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::SpiMiso)                             \
    X(GPIO39, 39, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::I2cScl)                              \
    X(GPIO40, 40, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::I2cSda)                              \
    X(GPIO41, 41, ::prtn::pin::PinCap::Digital)                                                            \
    X(GPIO42, 42, ::prtn::pin::PinCap::Digital)                                                            \
    X(GPIO43, 43, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::UartTx)                              \
    X(GPIO44, 44, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::UartRx)                              \
    X(GPIO45, 45, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::Strapping)                           \
    X(GPIO46, 46, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::Strapping)                           \
    X(GPIO47, 47, ::prtn::pin::PinCap::Digital)                                                            \
    X(GPIO48, 48, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::RmtTx | ::prtn::pin::PinCap::RgbLed)

namespace prtn::s3
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
        inline constexpr TypedPin<BoardPin::GPIO14, pinCaps(BoardPin::GPIO14)> gpio14 {};
        inline constexpr TypedPin<BoardPin::GPIO15, pinCaps(BoardPin::GPIO15)> gpio15 {};
        inline constexpr TypedPin<BoardPin::GPIO16, pinCaps(BoardPin::GPIO16)> gpio16 {};
        inline constexpr TypedPin<BoardPin::GPIO17, pinCaps(BoardPin::GPIO17)> gpio17 {};
        inline constexpr TypedPin<BoardPin::GPIO18, pinCaps(BoardPin::GPIO18)> gpio18 {};
        inline constexpr TypedPin<BoardPin::GPIO19, pinCaps(BoardPin::GPIO19)> gpio19 {};
        inline constexpr TypedPin<BoardPin::GPIO20, pinCaps(BoardPin::GPIO20)> gpio20 {};
        inline constexpr TypedPin<BoardPin::GPIO21, pinCaps(BoardPin::GPIO21)> gpio21 {};
        inline constexpr TypedPin<BoardPin::GPIO35, pinCaps(BoardPin::GPIO35)> gpio35 {};
        inline constexpr TypedPin<BoardPin::GPIO36, pinCaps(BoardPin::GPIO36)> gpio36 {};
        inline constexpr TypedPin<BoardPin::GPIO37, pinCaps(BoardPin::GPIO37)> gpio37 {};
        inline constexpr TypedPin<BoardPin::GPIO38, pinCaps(BoardPin::GPIO38)> gpio38 {};
        inline constexpr TypedPin<BoardPin::GPIO39, pinCaps(BoardPin::GPIO39)> gpio39 {};
        inline constexpr TypedPin<BoardPin::GPIO40, pinCaps(BoardPin::GPIO40)> gpio40 {};
        inline constexpr TypedPin<BoardPin::GPIO41, pinCaps(BoardPin::GPIO41)> gpio41 {};
        inline constexpr TypedPin<BoardPin::GPIO42, pinCaps(BoardPin::GPIO42)> gpio42 {};
        inline constexpr TypedPin<BoardPin::GPIO43, pinCaps(BoardPin::GPIO43)> gpio43 {};
        inline constexpr TypedPin<BoardPin::GPIO44, pinCaps(BoardPin::GPIO44)> gpio44 {};
        inline constexpr TypedPin<BoardPin::GPIO45, pinCaps(BoardPin::GPIO45)> gpio45 {};
        inline constexpr TypedPin<BoardPin::GPIO46, pinCaps(BoardPin::GPIO46)> gpio46 {};
        inline constexpr TypedPin<BoardPin::GPIO47, pinCaps(BoardPin::GPIO47)> gpio47 {};
        inline constexpr TypedPin<BoardPin::GPIO48, pinCaps(BoardPin::GPIO48)> gpio48 {};
    }

    namespace spi
    {
        inline constexpr decltype(gpio::gpio36) sck  = gpio::gpio36;
        inline constexpr decltype(gpio::gpio37) mosi = gpio::gpio37;
        inline constexpr decltype(gpio::gpio38) miso = gpio::gpio38;
        inline constexpr decltype(gpio::gpio35) cs   = gpio::gpio35;
    }

    namespace i2c0
    {
        inline constexpr decltype(gpio::gpio40) sda = gpio::gpio40;
        inline constexpr decltype(gpio::gpio39) scl = gpio::gpio39;
    }

    namespace uart0
    {
        inline constexpr decltype(gpio::gpio43) tx = gpio::gpio43;
        inline constexpr decltype(gpio::gpio44) rx = gpio::gpio44;
    }

    namespace usb
    {
        inline constexpr decltype(gpio::gpio19) dm = gpio::gpio19;
        inline constexpr decltype(gpio::gpio20) dp = gpio::gpio20;
    }

    namespace rmt
    {
        inline constexpr decltype(gpio::gpio48) tx0 = gpio::gpio48;
    }

    namespace rgb
    {
        inline constexpr decltype(gpio::gpio48) data = gpio::gpio48;
    }
}
