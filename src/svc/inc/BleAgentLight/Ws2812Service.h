#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "src/fw/inc/Result.h"
#include "src/dvc/inc/WS2812.h"
#include "src/prt/BleAgentLightProtocol.h"
#include "src/svc/inc/BleAgentLight/AgentRegistry.h"
#include "src/svc/inc/BleAgentLight/AgentVisualTheme.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace ble_agent_light
{
    class Ws2812Service
    {
    public:
        // Public compatibility aliases; the shared theme is the source of truth.
        static constexpr Color kColorRegistered     = AgentVisualTheme::Registered;
        static constexpr Color kColorOff            = AgentVisualTheme::Off;
        static constexpr Color kColorIdle           = AgentVisualTheme::Idle;
        static constexpr Color kColorWorking        = AgentVisualTheme::Working;
        static constexpr Color kColorWaitPermission = AgentVisualTheme::WaitPermission;
        static constexpr Color kColorWaitOption     = AgentVisualTheme::WaitOption;
        static constexpr Color kColorDone           = AgentVisualTheme::Done;
        static constexpr Color kColorError          = AgentVisualTheme::Error;
        static constexpr Color kColorDisconnected   = AgentVisualTheme::Disconnected;

        constexpr static gpio_num_t DefaultPinIo = GPIO_NUM_48;

        struct Config
        {
            size_t      ledCount   = protocol::AgentCount;
            gpio_num_t  ledPin     = DefaultPinIo;
            uint32_t    blinkMs    = 200;
            uint32_t    stackDepth = 4096;
            UBaseType_t priority   = 4;
            BaseType_t  core       = 1;
            TickType_t  retryDelay = pdMS_TO_TICKS(50);
        };

        enum class Detail : uint8_t
        {
            ALREADY_STARTED = 1,
            NOT_STARTED,
            LED_SETUP_FAILED,
            TASK_CREATE_FAILED,
            LED_UPDATE_FAILED,
        };

        explicit Ws2812Service(const Config& config)
            : m_config(config),
              m_leds(config.ledPin, m_config.ledCount) {}

        ~Ws2812Service() = default;

        using SetupErrors = ErrorSet<
            Detail::ALREADY_STARTED,
            Detail::LED_SETUP_FAILED,
            Detail::TASK_CREATE_FAILED>;
        using SetupResult = Result<void, SetupErrors>;

        SetupResult setup() {
            // check whether the service is already started
            if (m_started)
                return Err<Detail::ALREADY_STARTED>("WS2812 service is already started");

            // setup the LED driver
            const auto ledSetupResult = m_leds.setup();
            if (ledSetupResult.is_err())
                return ledSetupResult.propagate<Detail::LED_SETUP_FAILED>(
                    "WS2812 service could not initialize its LED driver");

            m_started                   = true;
            m_dirtyMask                 = allSlotsMask();
            const BaseType_t taskResult = xTaskCreatePinnedToCore(
                taskEntry,
                "agent leds",
                m_config.stackDepth,
                this,
                m_config.priority,
                &m_task,
                m_config.core);
            if (taskResult != pdPASS) {
                m_started = false;
                m_task    = nullptr;
                (void)m_leds.end();
                return Err<Detail::TASK_CREATE_FAILED>("WS2812 service could not create its rendering task");
            }
            return Ok();
        }

        [[nodiscard]] EventHandlers eventHandlers() noexcept {
            return {
                this,
                &Ws2812Service::registeredCallback,
                &Ws2812Service::stateCallback,
                &Ws2812Service::textCallback,
                &Ws2812Service::removedCallback,
            };
        }

        // LED driver access is deliberately confined to update(). BLE callbacks
        // only change the desired visual state under m_stateLock.
        using UpdateErrors = ErrorSet<
            Detail::NOT_STARTED,
            TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::LED_UPDATE_FAILED>>;
        using UpdateResult = Result<void, UpdateErrors>;

        /**
         * @brief 设置 BLE 连接状态。Set the BLE connection state.
         *
         * 断连时所有 Agent LED 常亮红色；重新连接后先全部熄灭，随后由新的
         * Agent 注册/状态事件逐槽恢复显示。这里只修改状态并唤醒内部任务。
         */
        void setConnected(bool connected) noexcept {
            portENTER_CRITICAL(&m_stateLock);
            if (m_connected == connected) {
                portEXIT_CRITICAL(&m_stateLock);
                return;
            }
            m_connected = connected;
            if (!connected) {
                m_activeMask      = 0;
                m_registeringMask = 0;
            }
            m_dirtyMask |= allSlotsMask();
            const TaskHandle_t task = m_task;
            portEXIT_CRITICAL(&m_stateLock);
            if (task != nullptr)
                xTaskNotifyGive(task);
        }

        /** 取走内部 LED 任务最近一次错误。Consume its latest asynchronous error. */
        [[nodiscard]] std::optional<UpdateErrors> takeLastError() {
            portENTER_CRITICAL(&m_stateLock);
            std::optional<UpdateErrors> result = m_lastError;
            m_lastError.reset();
            portEXIT_CRITICAL(&m_stateLock);
            return result;
        }

        [[nodiscard]] UpdateResult update() {
            if (!m_started)
                return Err<Detail::NOT_STARTED>("WS2812 service is not started");

            const TickType_t now                  = xTaskGetTickCount();
            const TickType_t configuredBlinkTicks = pdMS_TO_TICKS(m_config.blinkMs);
            const TickType_t blinkTicks           = configuredBlinkTicks == 0 ? 1 : configuredBlinkTicks;

            std::array<protocol::AgentState, protocol::AgentCount> states;
            uint8_t                                                registeringMask;
            uint8_t                                                activeMask;
            uint8_t                                                dirtyMask;
            bool                                                   connected;

            portENTER_CRITICAL(&m_stateLock);
            const bool hasNewRegistration = (m_dirtyMask & m_registeringMask) != 0;
            if (hasNewRegistration) {
                // Registration always starts with an illuminated frame, even if
                // the service has been idle for longer than one blink period.
                m_blinkOn       = true;
                m_lastBlinkTick = now;
            }
            else if (m_registeringMask != 0 && static_cast<TickType_t>(now - m_lastBlinkTick) >= blinkTicks) {
                m_blinkOn       = !m_blinkOn;
                m_lastBlinkTick = now;
                m_dirtyMask |= m_registeringMask;
            }

            states          = m_states;
            registeringMask = m_registeringMask;
            activeMask      = m_activeMask;
            dirtyMask       = m_dirtyMask;
            connected       = m_connected;
            m_dirtyMask     = 0;
            portEXIT_CRITICAL(&m_stateLock);

            if (dirtyMask == 0)
                return Ok();

            for (uint8_t id = 0; id < protocol::AgentCount; ++id) {
                const uint8_t bit = slotBit(id);
                if ((dirtyMask & bit) == 0)
                    continue;

                const Color color          = !connected
                                                 ? kColorDisconnected
                                             : (activeMask & bit) == 0
                                                 ? kColorOff
                                             : (registeringMask & bit) != 0
                                                 ? (m_blinkOn ? kColorRegistered : kColorOff)
                                                 : colorFor(states[id]);
                const auto  setPixelResult = m_leds.setPixel(id, color);
                if (setPixelResult.is_err()) {
                    requeue(dirtyMask);
                    return setPixelResult.propagate<Detail::LED_UPDATE_FAILED>(
                        "WS2812 service could not update a pixel");
                }
            }

            const auto showResult = m_leds.show();
            if (showResult.is_err()) {
                requeue(dirtyMask);
                return showResult.propagate<Detail::LED_UPDATE_FAILED>(
                    "WS2812 service could not flush its pixels");
            }

            return Ok();
        }

    private:
        static void taskEntry(void* context) {
            static_cast<Ws2812Service*>(context)->taskLoop();
        }

        void taskLoop() {
            const TickType_t configured = pdMS_TO_TICKS(m_config.blinkMs);
            const TickType_t period     = configured == 0 ? 1 : configured;
            for (;;) {
                const UpdateResult result = update();
                if (result.is_err()) {
                    portENTER_CRITICAL(&m_stateLock);
                    m_lastError = result.error();
                    portEXIT_CRITICAL(&m_stateLock);
                    vTaskDelay(m_config.retryDelay == 0 ? 1 : m_config.retryDelay);
                    continue;
                }
                (void)ulTaskNotifyTake(pdTRUE, period);
            }
        }

        static void registeredCallback(
            void*          context,
            uint8_t        id,
            const uint8_t* name,
            uint8_t        size) {
            static_cast<Ws2812Service*>(context)
                ->handleRegistered(id, name, size);
        }

        static void stateCallback(
            void*                context,
            uint8_t              id,
            protocol::AgentState state) {
            static_cast<Ws2812Service*>(context)->handleState(id, state);
        }

        static void textCallback(
            void*          context,
            uint8_t        id,
            const uint8_t* text,
            uint8_t        size) {
            static_cast<Ws2812Service*>(context)->handleText(id, text, size);
        }

        static void removedCallback(void* context, uint8_t id) {
            static_cast<Ws2812Service*>(context)->handleRemoved(id);
        }

        void handleRegistered(uint8_t id, const uint8_t*, uint8_t) {
            if (!validId(id))
                return;

            portENTER_CRITICAL(&m_stateLock);
            const uint8_t bit = slotBit(id);
            m_states[id]      = protocol::AgentState::Idle;
            m_activeMask |= bit;
            m_registeringMask |= bit;
            m_dirtyMask |= bit;
            portEXIT_CRITICAL(&m_stateLock);
            notifyTask();
        }

        [[nodiscard]] constexpr static Color colorFor(protocol::AgentState state) noexcept {
            return AgentVisualTheme::state(state);
        }

        void handleState(uint8_t id, protocol::AgentState state) {
            if (!validId(id))
                return;

            portENTER_CRITICAL(&m_stateLock);
            const uint8_t bit = slotBit(id);
            m_states[id]      = state;
            m_registeringMask &= static_cast<uint8_t>(~bit);
            m_dirtyMask |= bit;
            portEXIT_CRITICAL(&m_stateLock);
            notifyTask();
        }

        void handleText(uint8_t, const uint8_t*, uint8_t) {
            // we do not fuck with the text, it is the oled diplayers job
        }

        void handleRemoved(uint8_t id) {
            if (!validId(id))
                return;

            portENTER_CRITICAL(&m_stateLock);
            const uint8_t bit = slotBit(id);
            m_states[id]      = protocol::AgentState::Off;
            m_activeMask &= static_cast<uint8_t>(~bit);
            m_registeringMask &= static_cast<uint8_t>(~bit);
            m_dirtyMask |= bit;
            portEXIT_CRITICAL(&m_stateLock);
            notifyTask();
        }

        [[nodiscard]] bool validId(uint8_t id) const noexcept {
            return id < protocol::AgentCount && id < m_config.ledCount;
        }

        [[nodiscard]] static constexpr uint8_t slotBit(uint8_t id) noexcept {
            return static_cast<uint8_t>(uint8_t {1} << id);
        }

        [[nodiscard]] constexpr uint8_t allSlotsMask() const noexcept {
            const size_t count = m_config.ledCount < protocol::AgentCount
                                     ? m_config.ledCount
                                     : protocol::AgentCount;
            return count == 0 ? 0 : static_cast<uint8_t>((uint16_t {1} << count) - 1U);
        }

        void requeue(uint8_t dirtyMask) noexcept {
            portENTER_CRITICAL(&m_stateLock);
            m_dirtyMask |= dirtyMask;
            portEXIT_CRITICAL(&m_stateLock);
        }

        void notifyTask() noexcept {
            TaskHandle_t task;
            portENTER_CRITICAL(&m_stateLock);
            task = m_task;
            portEXIT_CRITICAL(&m_stateLock);
            if (task != nullptr)
                xTaskNotifyGive(task);
        }

    private:
        std::array<protocol::AgentState, protocol::AgentCount> m_states {};
        uint8_t                                                m_activeMask {};
        uint8_t                                                m_registeringMask {};
        uint8_t                                                m_dirtyMask {};
        std::optional<UpdateErrors>                            m_lastError;
        portMUX_TYPE                                           m_stateLock = portMUX_INITIALIZER_UNLOCKED;
        bool                                                   m_started   = false;
        bool                                                   m_connected = false;
        bool                                                   m_blinkOn   = true;
        TickType_t                                             m_lastBlinkTick {};
        TaskHandle_t                                           m_task = nullptr;
        Config                                                 m_config;
        WS2812                                                 m_leds;
    };
}
