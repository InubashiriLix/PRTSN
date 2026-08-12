#pragma once

#include "driver/gpio.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace prtn::pin
{
    using PinCaps = uint32_t;

    namespace PinCap
    {
        inline constexpr PinCaps None          = 0;
        inline constexpr PinCaps DigitalIn     = 1U << 0;
        inline constexpr PinCaps DigitalOut    = 1U << 1;
        inline constexpr PinCaps InputPullup   = 1U << 2;
        inline constexpr PinCaps InputPulldown = 1U << 3;
        inline constexpr PinCaps Analog        = 1U << 4;
        inline constexpr PinCaps UsbDm         = 1U << 5;
        inline constexpr PinCaps UsbDp         = 1U << 6;

        // Board metadata. These flags do not make a GPIO unavailable by themselves.
        inline constexpr PinCaps Boot      = 1U << 16;
        inline constexpr PinCaps Enable    = 1U << 17;
        inline constexpr PinCaps RgbLed    = 1U << 18;
        inline constexpr PinCaps Strapping = 1U << 19;

        inline constexpr PinCaps Digital = DigitalIn | DigitalOut | InputPullup | InputPulldown;
    }

    /// 引脚在最终应用中的用途；用于编译期电气能力检查。
    /// Intended use of a pin in the final app; used for compile-time capability checks.
    enum class Role : uint8_t
    {
        Input,
        Output,
        InputPullup,
        InputPulldown,
        AnalogInput,
        PwmOutput,
        RmtTx,
        SpiSck,
        SpiMosi,
        SpiMiso,
        SpiCs,
        I2cSda,
        I2cScl,
        UartTx,
        UartRx,
        I2sBclk,
        I2sWs,
        I2sDataIn,
        UsbDm,
        UsbDp,
    };

    [[nodiscard]] constexpr PinCaps requiredCaps(Role role) {
        switch (role) {
            case Role::Input:
            case Role::SpiMiso:
            case Role::UartRx:
            case Role::I2sDataIn:
                return PinCap::DigitalIn;
            case Role::Output:
            case Role::PwmOutput:
            case Role::RmtTx:
            case Role::SpiSck:
            case Role::SpiMosi:
            case Role::SpiCs:
            case Role::UartTx:
            case Role::I2sBclk:
            case Role::I2sWs:
                return PinCap::DigitalOut;
            case Role::InputPullup:
                return PinCap::DigitalIn | PinCap::InputPullup;
            case Role::InputPulldown:
                return PinCap::DigitalIn | PinCap::InputPulldown;
            case Role::AnalogInput:
                return PinCap::Analog;
            case Role::I2cSda:
            case Role::I2cScl:
                return PinCap::DigitalIn | PinCap::DigitalOut;
            case Role::UsbDm:
                return PinCap::UsbDm;
            case Role::UsbDp:
                return PinCap::UsbDp;
        }
        return PinCap::None;
    }

    [[nodiscard]] constexpr bool hasAll(PinCaps value, PinCaps required) {
        return (value & required) == required;
    }

    /**
     * @brief 一条应用级引脚绑定。/ One application-level pin binding.
     *
     * 保存逻辑资源 ID、原生 ESP-IDF GPIO 编号和用途。通常不直接构造，请使用 bind()。
     * Stores a logical resource ID, native ESP-IDF GPIO number, and role. Prefer bind().
     *
     * @tparam Id 应用定义的 enum class PinId。/ Application-defined enum class PinId.
     */
    template <typename Id>
        requires std::is_enum_v<Id>
    struct Binding
    {
        Id         id;
        gpio_num_t gpio;
        Role       role;
    };

    /**
     * @brief 创建一条编译期引脚绑定。/ Creates one compile-time pin binding.
     *
     * bind() 只描述一条接线；把所有 bind() 传给 layout() 后才会统一检查冲突。
     * bind() describes one wire. Pass all bindings to layout() for conflict validation.
     *
     * @code
     * prtn::pin::bind(PinId::LedData, GPIO_NUM_48, prtn::pin::Role::RmtTx)
     * @endcode
     *
     * @tparam Id 应用定义的 enum class PinId。/ Application-defined enum class PinId.
     * @param id 逻辑资源名称，例如 PinId::LedData。/ Logical resource name.
     * @param gpio 原生 ESP-IDF GPIO，例如 GPIO_NUM_48。/ Native ESP-IDF GPIO.
     * @param role 该 GPIO 在应用中的用途。/ Intended use in the application.
     * @return 可直接传给 layout() 的 Binding。/ A Binding accepted by layout().
     */
    template <typename Id>
        requires std::is_enum_v<Id>
    [[nodiscard]] consteval Binding<Id> bind(Id id, gpio_num_t gpio, Role role) {
        return {id, gpio, role};
    }

    enum class LayoutError : uint8_t
    {
        None,
        DuplicateId,
        InvalidGpio,
        ReservedGpio,
        DuplicateGpio,
        CapabilityMismatch,
    };

    struct LayoutValidation
    {
        static constexpr std::size_t NoBinding = static_cast<std::size_t>(-1);

        LayoutError error  = LayoutError::None;
        std::size_t first  = NoBinding;
        std::size_t second = NoBinding;
        gpio_num_t  gpio   = GPIO_NUM_NC;

        [[nodiscard]] constexpr bool ok() const {
            return error == LayoutError::None;
        }

        [[nodiscard]] constexpr explicit operator bool() const {
            return ok();
        }
    };
}
