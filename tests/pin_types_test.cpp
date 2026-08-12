#include "src/cfg/pin/PinRegistry.h"

#include <array>
#include <concepts>
#include <type_traits>

namespace
{
    enum class PinId : uint8_t
    {
        Sda,
        Scl,
    };

    constexpr auto ValidBindings = std::array {
        ::prtn::pin::bind(PinId::Sda, GPIO_NUM_4, ::prtn::pin::Role::I2cSda),
        ::prtn::pin::bind(PinId::Scl, GPIO_NUM_5, ::prtn::pin::Role::I2cScl),
    };

    constexpr auto DuplicateIdBindings = std::array {
        ::prtn::pin::bind(PinId::Sda, GPIO_NUM_4, ::prtn::pin::Role::I2cSda),
        ::prtn::pin::bind(PinId::Sda, GPIO_NUM_5, ::prtn::pin::Role::I2cScl),
    };

    constexpr auto DuplicateGpioBindings = std::array {
        ::prtn::pin::bind(PinId::Sda, GPIO_NUM_4, ::prtn::pin::Role::I2cSda),
        ::prtn::pin::bind(PinId::Scl, GPIO_NUM_4, ::prtn::pin::Role::I2cScl),
    };

    constexpr auto InvalidGpioBindings = std::array {
        ::prtn::pin::bind(PinId::Sda, static_cast<gpio_num_t>(34), ::prtn::pin::Role::Input),
    };

    constexpr auto CapabilityMismatchBindings = std::array {
        ::prtn::pin::bind(PinId::Sda, GPIO_NUM_4, ::prtn::pin::Role::UsbDm),
    };

    static_assert(::prtn::pin::validateLayout(ValidBindings).ok());
    static_assert(::prtn::pin::validateLayout(DuplicateIdBindings).error == ::prtn::pin::LayoutError::DuplicateId);
    static_assert(::prtn::pin::validateLayout(DuplicateGpioBindings).error == ::prtn::pin::LayoutError::DuplicateGpio);
    static_assert(::prtn::pin::validateLayout(InvalidGpioBindings).error == ::prtn::pin::LayoutError::InvalidGpio);
    static_assert(::prtn::pin::validateLayout(CapabilityMismatchBindings).error == ::prtn::pin::LayoutError::CapabilityMismatch);

    inline constexpr auto Pins = ::prtn::pin::layout(
        ::prtn::pin::bind(PinId::Sda, GPIO_NUM_4, ::prtn::pin::Role::I2cSda),
        ::prtn::pin::bind(PinId::Scl, GPIO_NUM_5, ::prtn::pin::Role::I2cScl));

    static_assert(std::same_as<std::remove_cv_t<decltype(Pins[PinId::Sda])>, gpio_num_t>);
    static_assert(Pins[PinId::Sda] == GPIO_NUM_4);
    static_assert(Pins[PinId::Scl] == GPIO_NUM_5);
}

int main() {}
