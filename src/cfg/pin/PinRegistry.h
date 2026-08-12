#pragma once

#include "src/cfg/BoardSelect.h"
#include "src/cfg/pin/PinTypes.h"

#include <array>
#include <concepts>
#include <cstddef>
#include <type_traits>

namespace prtn::pin
{
    /// 检查重复 ID、GPIO 冲突、板卡有效性、保留引脚和电气能力。
    /// Checks duplicate IDs/GPIOs, board validity, reservations, and electrical capability.
    template <typename Id, std::size_t N>
    [[nodiscard]] constexpr LayoutValidation validateLayout(const std::array<Binding<Id>, N>& bindings) {
        for (std::size_t i = 0; i < N; ++i) {
            const Binding<Id>& current = bindings[i];

            if (!::prtn::b::hasGpio(current.gpio)) {
                return {LayoutError::InvalidGpio, i, LayoutValidation::NoBinding, current.gpio};
            }

            for (const gpio_num_t reserved : ::prtn::b::ReservedPins) {
                if (current.gpio == reserved) {
                    return {LayoutError::ReservedGpio, i, LayoutValidation::NoBinding, current.gpio};
                }
            }

            if (!hasAll(::prtn::b::pinCapsFromGpio(current.gpio), requiredCaps(current.role))) {
                return {LayoutError::CapabilityMismatch, i, LayoutValidation::NoBinding, current.gpio};
            }

            for (std::size_t j = i + 1; j < N; ++j) {
                if (current.id == bindings[j].id) {
                    return {LayoutError::DuplicateId, i, j, current.gpio};
                }
                if (current.gpio == bindings[j].gpio) {
                    return {LayoutError::DuplicateGpio, i, j, current.gpio};
                }
            }
        }

        return {};
    }

    template <typename Id, std::size_t N>
    /// 最终应用的编译期引脚表。operator[] 直接返回原生 gpio_num_t。
    /// Compile-time pin table for a final app. operator[] returns native gpio_num_t directly.
    class PinLayout
    {
    public:
        constexpr explicit PinLayout(std::array<Binding<Id>, N> bindings) : m_bindings(bindings) {}

        [[nodiscard]] consteval gpio_num_t operator[](Id id) const {
            for (const Binding<Id>& binding : m_bindings) {
                if (binding.id == id) {
                    return binding.gpio;
                }
            }
            throw "PinId is not present in this PinLayout";
        }

        [[nodiscard]] constexpr const std::array<Binding<Id>, N>& bindings() const {
            return m_bindings;
        }

    private:
        std::array<Binding<Id>, N> m_bindings;
    };

    namespace detail
    {
        consteval void requireValidLayout(LayoutValidation validation) {
            switch (validation.error) {
                case LayoutError::None:
                    return;
                case LayoutError::DuplicateId:
                    throw "PinLayout contains a duplicate PinId";
                case LayoutError::InvalidGpio:
                    throw "PinLayout contains a GPIO absent from the selected board";
                case LayoutError::ReservedGpio:
                    throw "PinLayout uses a GPIO reserved by the selected board";
                case LayoutError::DuplicateGpio:
                    throw "PinLayout assigns one physical GPIO more than once";
                case LayoutError::CapabilityMismatch:
                    throw "PinLayout role is incompatible with the GPIO electrical capability";
            }
        }
    }

    /**
     * @brief 创建并验证最终应用的编译期引脚表。/ Creates and validates a compile-time app pin table.
     *
     * 所有 binding 必须使用同一种 PinId。layout() 会在编译期拒绝重复 PinId、重复 GPIO、
     * 当前板卡不存在或保留的 GPIO，以及与 Role 不兼容的电气能力。
     * All bindings must use the same PinId type. At compile time, layout() rejects duplicate IDs,
     * duplicate GPIOs, unavailable/reserved board pins, and role/capability mismatches.
     *
     * 返回表的 operator[] 是 consteval，结果类型正是 gpio_num_t，可直接传入驱动。
     * operator[] is consteval and returns gpio_num_t directly for driver APIs.
     *
     * @code
     * enum class PinId : uint8_t { LedData };
     * inline constexpr auto Pins = prtn::pin::layout(
     *     prtn::pin::bind(PinId::LedData, GPIO_NUM_48, prtn::pin::Role::RmtTx));
     * WS2812 led {Pins[PinId::LedData], config};
     * @endcode
     *
     * @tparam Id 应用定义的 enum class PinId。/ Application-defined enum class PinId.
     * @tparam Rest 其余 Binding；通常无需手写。/ Remaining bindings; normally deduced.
     * @param first 第一条绑定；layout 至少需要一条。/ First binding; at least one is required.
     * @param rest 其余绑定。/ Remaining bindings.
     * @return 已验证的 PinLayout；Pins[id] 返回 gpio_num_t。/ Validated PinLayout returning gpio_num_t.
     */
    template <typename Id, typename... Rest>
        requires(std::is_enum_v<Id> && (std::same_as<Binding<Id>, std::remove_cvref_t<Rest>> && ...))
    [[nodiscard]] consteval auto layout(Binding<Id> first, Rest... rest) {
        std::array<Binding<Id>, 1 + sizeof...(Rest)> bindings {first, rest...};
        detail::requireValidLayout(validateLayout(bindings));
        return PinLayout<Id, bindings.size()> {bindings};
    }
}
