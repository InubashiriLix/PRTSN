#pragma once

#include "driver/gpio.h"
#include "src/cfg/BoardSelect.h"
#include "src/cfg/pin/PinTypes.h"

#include <array>
#include <cstdint>

namespace prtn::pin
{
    template <auto Pin, PinCaps RequiredCaps, typename Owner>
    class RegisteredPin
    {
    public:
        using OwnerType = Owner;

        static constexpr decltype(Pin) BoardPinValue = Pin;
        static constexpr PinCaps       Required      = RequiredCaps;
        static constexpr int           Gpio          = ::prtn::b::pinGpio(Pin);
        static constexpr PinCaps       Supported     = ::prtn::b::pinCaps(Pin);
        static constexpr const char*   Name          = ::prtn::b::pinName(Pin);

        constexpr int number() const {
            return Gpio;
        }
        constexpr uint8_t arduinoPin() const {
            return static_cast<uint8_t>(Gpio);
        }
        constexpr gpio_num_t gpioNum() const {
            return static_cast<gpio_num_t>(Gpio);
        }
    };

    template <typename Owner, auto Pin, PinCaps RequiredCaps>
    consteval auto makeRegisteredPinByValue() {
        static_assert(::prtn::b::hasPin(Pin), "Pin is not present on the selected board");
        static_assert(hasAll(::prtn::b::pinCaps(Pin), RequiredCaps), "Pin does not support the requested capability");
        return RegisteredPin<Pin, RequiredCaps, Owner> {};
    }

    template <typename Owner, typename Typed>
    consteval auto makeRegisteredPin(Typed) {
        static_assert(::prtn::b::hasPin(Typed::Pin), "Pin is not present on the selected board");
        static_assert(hasAll(::prtn::b::pinCaps(Typed::Pin), Typed::Required), "Pin does not support the requested capability");
        static_assert(hasAll(Typed::Supported, Typed::Required), "Typed pin definition does not include the requested capability");
        return RegisteredPin<Typed::Pin, Typed::Required, Owner> {};
    }

    template <auto Pin, PinCaps RequiredCaps, typename Owner>
    constexpr PinClaim makePinClaim(RegisteredPin<Pin, RequiredCaps, Owner>, PinUse use, const char* owner, const char* capLabel) {
        return PinClaim {
            .gpio     = ::prtn::b::pinGpio(Pin),
            .caps     = RequiredCaps,
            .use      = use,
            .owner    = owner,
            .pinName  = ::prtn::b::pinName(Pin),
            .capLabel = capLabel,
        };
    }

    template <std::size_t N>
    constexpr bool validatePinClaims(const std::array<PinClaim, N>& claims) {
        return noExclusivePinConflicts(claims);
    }
}

#define PRTN_REG_PIN__(owner_type, typed_pin) \
    (::prtn::pin::makeRegisteredPin<owner_type>((typed_pin)))

#define PRTN_PIN_CLAIM__(registered_pin, pin_use, cap_label) \
    (::prtn::pin::makePinClaim((registered_pin), (pin_use), #registered_pin, (cap_label)))
