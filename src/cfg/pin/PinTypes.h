#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace prtn::pin
{
    using PinCaps = uint64_t;

    namespace PinCap
    {
        inline constexpr PinCaps None          = 0;
        inline constexpr PinCaps Gpio          = 1ULL << 0;
        inline constexpr PinCaps DigitalIn     = 1ULL << 1;
        inline constexpr PinCaps DigitalOut    = 1ULL << 2;
        inline constexpr PinCaps InputPullup   = 1ULL << 3;
        inline constexpr PinCaps InputPulldown = 1ULL << 4;
        inline constexpr PinCaps Analog        = 1ULL << 5;
        inline constexpr PinCaps Pwm           = 1ULL << 6;
        inline constexpr PinCaps RmtTx         = 1ULL << 7;
        inline constexpr PinCaps SpiSck        = 1ULL << 8;
        inline constexpr PinCaps SpiMosi       = 1ULL << 9;
        inline constexpr PinCaps SpiMiso       = 1ULL << 10;
        inline constexpr PinCaps SpiCs         = 1ULL << 11;
        inline constexpr PinCaps I2cSda        = 1ULL << 12;
        inline constexpr PinCaps I2cScl        = 1ULL << 13;
        inline constexpr PinCaps UartTx        = 1ULL << 14;
        inline constexpr PinCaps UartRx        = 1ULL << 15;
        inline constexpr PinCaps UsbDm         = 1ULL << 16;
        inline constexpr PinCaps UsbDp         = 1ULL << 17;
        inline constexpr PinCaps Boot          = 1ULL << 18;
        inline constexpr PinCaps Enable        = 1ULL << 19;
        inline constexpr PinCaps RgbLed        = 1ULL << 20;
        inline constexpr PinCaps Strapping     = 1ULL << 21;

        inline constexpr PinCaps Digital = Gpio | DigitalIn | DigitalOut;
    }

    enum class PinUse : uint8_t
    {
        Shared,
        Exclusive,
    };

    struct PinClaim
    {
        int         gpio     = -1;
        PinCaps     caps     = PinCap::None;
        PinUse      use      = PinUse::Exclusive;
        const char* owner    = "";
        const char* pinName  = "";
        const char* capLabel = "";
    };

    template <auto PinValue, PinCaps SupportedCaps, PinCaps RequiredCaps>
    struct TypedPin
    {
        static constexpr decltype(PinValue) Pin       = PinValue;
        static constexpr PinCaps            Supported = SupportedCaps;
        static constexpr PinCaps            Required  = RequiredCaps;
    };

    constexpr bool hasAll(PinCaps value, PinCaps required) {
        return (value & required) == required;
    }

    template <std::size_t N>
    constexpr bool noExclusivePinConflicts(const std::array<PinClaim, N>& claims) {
        for (std::size_t i = 0; i < N; ++i) {
            for (std::size_t j = i + 1; j < N; ++j) {
                if (claims[i].gpio == claims[j].gpio &&
                    (claims[i].use == PinUse::Exclusive || claims[j].use == PinUse::Exclusive)) {
                    return false;
                }
            }
        }
        return true;
    }
}
