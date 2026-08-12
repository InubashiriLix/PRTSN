#pragma once

#include "src/cfg/pin/PinTypes.h"

#include "driver/gpio.h"

#include <array>
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
    X(GPIO35, 35, ::prtn::pin::PinCap::Digital)                                                            \
    X(GPIO36, 36, ::prtn::pin::PinCap::Digital)                                                            \
    X(GPIO37, 37, ::prtn::pin::PinCap::Digital)                                                            \
    X(GPIO38, 38, ::prtn::pin::PinCap::Digital)                                                            \
    X(GPIO39, 39, ::prtn::pin::PinCap::Digital)                                                            \
    X(GPIO40, 40, ::prtn::pin::PinCap::Digital)                                                            \
    X(GPIO41, 41, ::prtn::pin::PinCap::Digital)                                                            \
    X(GPIO42, 42, ::prtn::pin::PinCap::Digital)                                                            \
    X(GPIO43, 43, ::prtn::pin::PinCap::Digital)                                                            \
    X(GPIO44, 44, ::prtn::pin::PinCap::Digital)                                                            \
    X(GPIO45, 45, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::Strapping)                           \
    X(GPIO46, 46, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::Strapping)                           \
    X(GPIO47, 47, ::prtn::pin::PinCap::Digital)                                                            \
    X(GPIO48, 48, ::prtn::pin::PinCap::Digital | ::prtn::pin::PinCap::RgbLed)

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
        gpio_num_t           gpio;
        ::prtn::pin::PinCaps caps;
        const char*          name;
    };

    inline constexpr PinDef PinDefs[] {
#define PRTN_BOARD_PIN_DEF(name, gpio, caps) {BoardPin::name, GPIO_NUM_##gpio, caps, #name},
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

    constexpr bool hasGpio(gpio_num_t gpio) {
        for (const auto& def : PinDefs) {
            if (def.gpio == gpio) {
                return true;
            }
        }
        return false;
    }

    constexpr gpio_num_t pinGpio(BoardPin pin) {
        for (const auto& def : PinDefs) {
            if (def.pin == pin) {
                return def.gpio;
            }
        }
        return GPIO_NUM_NC;
    }

    constexpr ::prtn::pin::PinCaps pinCaps(BoardPin pin) {
        for (const auto& def : PinDefs) {
            if (def.pin == pin) {
                return def.caps;
            }
        }
        return ::prtn::pin::PinCap::None;
    }

    constexpr ::prtn::pin::PinCaps pinCapsFromGpio(gpio_num_t gpio) {
        for (const auto& def : PinDefs) {
            if (def.gpio == gpio) {
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

    constexpr const char* pinNameFromGpio(gpio_num_t gpio) {
        for (const auto& def : PinDefs) {
            if (def.gpio == gpio) {
                return def.name;
            }
        }
        return "<invalid>";
    }

    namespace gpio
    {
#define PRTN_NATIVE_GPIO(name, number, caps) inline constexpr gpio_num_t gpio##number = GPIO_NUM_##number;
        PRTN_BOARD_PIN_LIST(PRTN_NATIVE_GPIO)
#undef PRTN_NATIVE_GPIO
    }

    namespace spi
    {
        inline constexpr gpio_num_t sck  = GPIO_NUM_36;
        inline constexpr gpio_num_t mosi = GPIO_NUM_37;
        inline constexpr gpio_num_t miso = GPIO_NUM_38;
        inline constexpr gpio_num_t cs   = GPIO_NUM_35;
    }

    namespace i2c0
    {
        inline constexpr gpio_num_t sda = GPIO_NUM_40;
        inline constexpr gpio_num_t scl = GPIO_NUM_39;
    }

    namespace uart0
    {
        inline constexpr gpio_num_t tx = GPIO_NUM_43;
        inline constexpr gpio_num_t rx = GPIO_NUM_44;
    }

    namespace usb
    {
        inline constexpr gpio_num_t dm = GPIO_NUM_19;
        inline constexpr gpio_num_t dp = GPIO_NUM_20;
    }

    namespace rmt
    {
        inline constexpr gpio_num_t tx0 = GPIO_NUM_48;
    }

    namespace rgb
    {
        inline constexpr gpio_num_t data = GPIO_NUM_48;
    }

    inline constexpr std::array<gpio_num_t, 0> ReservedPins {};
}
