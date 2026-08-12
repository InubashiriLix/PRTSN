#pragma once

#include "src/alg/inc/alg_color.h"
#include "src/prt/BleAgentLightProtocol.h"

namespace ble_agent_light
{
    /**
     * @brief Agent Panel 的共享高对比度配色。Shared high-contrast Agent palette.
     *
     * LCD 状态条和 WS2812 必须使用同一份颜色定义，确保同一状态不会在两种
     * 输出设备上产生不同语义。颜色刻意选择高亮、强饱和且色相分离的值：
     *
     * - Off:            `#000000` 黑 / black
     * - Idle:           `#386CFF` 亮蓝 / bright blue
     * - Working:        `#00D9FF` 青 / cyan
     * - WaitPermission: `#FFD000` 黄 / yellow
     * - WaitOption:     `#FF00D4` 品红 / magenta
     * - Done:           `#00E85A` 绿 / green
     * - Error:          `#FF2020` 红 / red
     *
     * WaitPermission 和 WaitOption 不再使用相近的橙红色，因此即使只看余光也能区分。
     */
    struct AgentVisualTheme
    {
        inline static constexpr Color Registered     = Color::fromHex("#FFFFFF");
        inline static constexpr Color Disconnected   = Color::fromHex("#FF0000");
        inline static constexpr Color Off            = Color::fromHex("#000000");
        inline static constexpr Color Idle           = Color::fromHex("#386CFF");
        inline static constexpr Color Working        = Color::fromHex("#00D9FF");
        inline static constexpr Color WaitPermission = Color::fromHex("#FFD000");
        inline static constexpr Color WaitOption     = Color::fromHex("#FF00D4");
        inline static constexpr Color Done           = Color::fromHex("#00E85A");
        inline static constexpr Color Error          = Color::fromHex("#FF2020");

        [[nodiscard]] static constexpr Color state(protocol::AgentState value) noexcept {
            switch (value) {
                case protocol::AgentState::Off:
                    return Off;
                case protocol::AgentState::Idle:
                    return Idle;
                case protocol::AgentState::Working:
                    return Working;
                case protocol::AgentState::WaitPermission:
                    return WaitPermission;
                case protocol::AgentState::WaitOption:
                    return WaitOption;
                case protocol::AgentState::Done:
                    return Done;
                case protocol::AgentState::Error:
                    return Error;
            }
            return Error;
        }
    };
}
