#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "src/dvc/inc/ST7735.h"
#include "src/svc/inc/BleAgentLight/AgentRegistry.h"
#include "src/svc/inc/BleAgentLight/AgentVisualTheme.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace ble_agent_light
{
    /**
     * @brief 在 80x160 ST7735 上事件驱动地显示七个 Agent。
     * Event-driven seven-slot Agent display for an 80x160 ST7735.
     *
     * Registry 是唯一的数据源。本服务的回调只记录发生变化的槽位并唤醒内部显示任务，
     * 不会在 BLE callback 中执行 SPI。`setup()` 会创建固定到指定 CPU 的常驻任务；
     * 调用者不需要也不应该编写显示轮询逻辑。
     *
     * The Registry remains the single source of truth. Callbacks only mark slots dirty
     * and wake the service-owned task; all ST7735/SPI access happens in that task.
     */
    class DisplayService
    {
    public:
        static constexpr uint16_t    SlotHeight     = 22;
        static constexpr uint16_t    BarWidth       = 6;
        static constexpr uint16_t    TextX          = 8;
        static constexpr std::size_t NameCharacters = 10;
        static constexpr std::size_t TextCharacters = 12;

        struct Config
        {
            /** FreeRTOS task stack depth (ESP-IDF measures this value in bytes). */
            // Rendering keeps typed Result cause chains, text buffers and the
            // ESP-IDF SPI polling frame on the same call stack. 4 KiB is not
            // sufficient on ESP32-S3 and trips the FreeRTOS stack canary as
            // soon as the first Agent slot is drawn.
            uint32_t stackDepth = 8192;
            /** Display rendering task priority. */
            UBaseType_t priority = 4;
            /** ESP32-S3 application core by default; NimBLE is configured on Core 0. */
            BaseType_t core = 1;
            /** Delay before retrying a failed LCD update. */
            TickType_t retryDelay = pdMS_TO_TICKS(50);
        };

        enum class Detail : uint8_t
        {
            NOT_STARTED = 1,
            ALREADY_STARTED,
            LCD_NOT_STARTED,
            UNSUPPORTED_LAYOUT,
            TASK_CREATE_FAILED,
            LCD_UPDATE_FAILED,
        };

        using SetupErrors = ErrorSet<
            Detail::ALREADY_STARTED,
            Detail::LCD_NOT_STARTED,
            Detail::UNSUPPORTED_LAYOUT,
            Detail::TASK_CREATE_FAILED,
            TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::LCD_UPDATE_FAILED>>;
        using SetupResult = Result<void, SetupErrors>;

        using UpdateErrors = ErrorSet<
            Detail::NOT_STARTED,
            TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::LCD_UPDATE_FAILED>>;
        using UpdateResult = Result<bool, UpdateErrors>;

        explicit DisplayService(ST7735& lcd, const AgentRegistry& registry)
            : DisplayService(lcd, registry, Config {}) {}

        DisplayService(
            ST7735&              lcd,
            const AgentRegistry& registry,
            const Config&        config)
            : m_lcd(lcd),
              m_registry(registry),
              m_config(config) {}

        DisplayService(const DisplayService&)            = delete;
        DisplayService& operator=(const DisplayService&) = delete;

        /**
         * @brief 初始化画面并启动内部显示任务。Initialize the screen and start its task.
         *
         * LCD 驱动必须已经 setup。成功后，所有后续 SPI 绘制只会发生在内部任务中。
         * The LCD driver must already be set up. On success, subsequent SPI rendering
         * is confined to the service-owned task.
         */
        [[nodiscard]] SetupResult setup();

        /**
         * @brief 取走内部显示任务最近一次错误。Consume the latest asynchronous error.
         * @return 没有新错误时返回 `std::nullopt`；读取后该错误会被清除。
         *
         * 显示任务不能直接向 App 的 Console 建立依赖，因此把完整、类型安全的
         * ErrorSet 留给应用任务读取和记录。连续故障只保留最近一次错误。
         */
        [[nodiscard]] std::optional<UpdateErrors> takeLastError();

        [[nodiscard]] EventHandlers eventHandlers() noexcept {
            return {
                this,
                &DisplayService::registeredCallback,
                &DisplayService::stateCallback,
                &DisplayService::textCallback,
                &DisplayService::removedCallback,
            };
        }

    private:
        using RenderErrors = ErrorSet<
            TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::LCD_UPDATE_FAILED>>;
        using RenderResult = Result<void, RenderErrors>;

        static void                taskEntry(void* context);
        void                       taskLoop();
        [[nodiscard]] UpdateResult waitAndUpdate(TickType_t timeout);
        void                       rememberError(const UpdateErrors& error);

        static void registeredCallback(void* context, uint8_t id, const uint8_t*, uint8_t) {
            static_cast<DisplayService*>(context)->markDirty(id);
        }
        static void stateCallback(void* context, uint8_t id, protocol::AgentState) {
            static_cast<DisplayService*>(context)->markDirty(id);
        }
        static void textCallback(void* context, uint8_t id, const uint8_t*, uint8_t) {
            static_cast<DisplayService*>(context)->markDirty(id);
        }
        static void removedCallback(void* context, uint8_t id) {
            static_cast<DisplayService*>(context)->markDirty(id);
        }

        void                       markDirty(uint8_t id);
        [[nodiscard]] uint8_t      takeDirtyMask();
        void                       requeue(uint8_t mask);
        [[nodiscard]] RenderResult renderEmpty(uint8_t id);
        [[nodiscard]] RenderResult renderAgent(uint8_t id, const AgentRegistry::AgentSnapshot& agent);
        [[nodiscard]] RenderResult renderSlot(uint8_t id, uint16_t barColor, const char* firstLine, const char* secondLine);

        [[nodiscard]] static uint16_t stateColor(protocol::AgentState state);
        static void                   copyDisplayText(char* output, std::size_t capacity, const uint8_t* input, uint8_t size, std::size_t limit);

        ST7735&              m_lcd;
        const AgentRegistry& m_registry;
        Config               m_config;

        TaskHandle_t                m_task      = nullptr;
        uint8_t                     m_dirtyMask = 0;
        std::optional<UpdateErrors> m_lastError;
        portMUX_TYPE                m_stateLock = portMUX_INITIALIZER_UNLOCKED;
        bool                        m_started   = false;
    };
}
