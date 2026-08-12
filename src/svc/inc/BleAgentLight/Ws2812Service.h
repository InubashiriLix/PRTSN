#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "src/fw/inc/Result.h"
#include "src/dvc/inc/WS2812.h"
#include "src/prt/BleAgentLightProtocol.h"
#include "src/svc/inc/BleAgentLight/AgentRegistry.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace ble_agent_light
{
    class Ws2812Service
    {
    public:
        // the fucking default colors
        static constexpr Color kColorRegistered     = Color::fromHex("#606060");
        static constexpr Color kColorOff            = Color::fromHex("#000000");
        static constexpr Color kColorIdle           = Color::fromHex("#204060");
        static constexpr Color kColorWorking        = Color::fromHex("#0080C0");
        static constexpr Color kColorWaitPermission = Color::fromHex("#F05000");
        static constexpr Color kColorWaitOption     = Color::fromHex("#FF2000");
        static constexpr Color kColorDone           = Color::fromHex("#008040");
        static constexpr Color kColorError          = Color::fromHex("#C00020");

        constexpr static gpio_num_t DefaultPinIo = GPIO_NUM_48;

        struct Config
        {
            size_t     ledCount = protocol::AgentCount;
            gpio_num_t ledPin   = DefaultPinIo;
            uint32_t   blinkMs  = 200;
        };

        enum class Detail : uint8_t
        {
            ALREADY_STARTED = 1,
            NOT_STARTED,
            LED_SETUP_FAILED,
            LED_UPDATE_FAILED,
        };

        explicit Ws2812Service(const Config& config)
            : m_config(config),
              m_leds(config.ledPin, m_config.ledCount) {}

        ~Ws2812Service() = default;

        using SetupErrors = ErrorSet<
            Detail::ALREADY_STARTED,
            Detail::LED_SETUP_FAILED>;
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

            m_started = true;
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

        [[nodiscard]] UpdateResult update() {
            if (!m_started)
                return Err<Detail::NOT_STARTED>("WS2812 service is not started");

            static bool       blinkOn       = true;
            static TickType_t lastBlinkTick = xTaskGetTickCount();

            const TickType_t now                  = xTaskGetTickCount();
            const TickType_t configuredBlinkTicks = pdMS_TO_TICKS(m_config.blinkMs);
            const TickType_t blinkTicks           = configuredBlinkTicks == 0 ? 1 : configuredBlinkTicks;

            std::array<protocol::AgentState, protocol::AgentCount> states;
            uint8_t                                                registeringMask;
            uint8_t                                                dirtyMask;

            portENTER_CRITICAL(&m_stateLock);
            const bool hasNewRegistration = (m_dirtyMask & m_registeringMask) != 0;
            if (hasNewRegistration) {
                // Registration always starts with an illuminated frame, even if
                // the service has been idle for longer than one blink period.
                blinkOn       = true;
                lastBlinkTick = now;
            }
            else if (m_registeringMask != 0 && static_cast<TickType_t>(now - lastBlinkTick) >= blinkTicks) {
                blinkOn       = !blinkOn;
                lastBlinkTick = now;
                m_dirtyMask |= m_registeringMask;
            }

            states          = m_states;
            registeringMask = m_registeringMask;
            dirtyMask       = m_dirtyMask;
            m_dirtyMask     = 0;
            portEXIT_CRITICAL(&m_stateLock);

            if (dirtyMask == 0)
                return Ok();

            for (uint8_t id = 0; id < protocol::AgentCount; ++id) {
                const uint8_t bit = slotBit(id);
                if ((dirtyMask & bit) == 0)
                    continue;

                const Color color          = (registeringMask & bit) != 0
                                                 ? (blinkOn ? kColorRegistered : kColorOff)
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
            m_registeringMask |= bit;
            m_dirtyMask |= bit;
            portEXIT_CRITICAL(&m_stateLock);
        }

        [[nodiscard]] constexpr static Color colorFor(protocol::AgentState state) noexcept {
            switch (state) {
                case protocol::AgentState::Off:
                    return kColorOff;
                case protocol::AgentState::Idle:
                    return kColorIdle;
                case protocol::AgentState::Working:
                    return kColorWorking;
                case protocol::AgentState::WaitPermission:
                    return kColorWaitPermission;
                case protocol::AgentState::WaitOption:
                    return kColorWaitOption;
                case protocol::AgentState::Done:
                    return kColorDone;
                case protocol::AgentState::Error:
                    return kColorError;
            }

            return kColorError;
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
            m_registeringMask &= static_cast<uint8_t>(~bit);
            m_dirtyMask |= bit;
            portEXIT_CRITICAL(&m_stateLock);
        }

        [[nodiscard]] bool validId(uint8_t id) const noexcept {
            return id < protocol::AgentCount && id < m_config.ledCount;
        }

        [[nodiscard]] static constexpr uint8_t slotBit(uint8_t id) noexcept {
            return static_cast<uint8_t>(uint8_t {1} << id);
        }

        void requeue(uint8_t dirtyMask) noexcept {
            portENTER_CRITICAL(&m_stateLock);
            m_dirtyMask |= dirtyMask;
            portEXIT_CRITICAL(&m_stateLock);
        }

    private:
        std::array<protocol::AgentState, protocol::AgentCount> m_states {};
        uint8_t                                                m_registeringMask {};
        uint8_t                                                m_dirtyMask {};
        portMUX_TYPE                                           m_stateLock = portMUX_INITIALIZER_UNLOCKED;
        bool                                                   m_started   = false;
        Config                                                 m_config;
        WS2812                                                 m_leds;
    };
}
