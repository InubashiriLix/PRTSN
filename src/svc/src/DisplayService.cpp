#include "src/svc/inc/BleAgentLight/DisplayService.h"

#include <algorithm>

namespace ble_agent_light
{
    namespace
    {
        constexpr uint16_t EmptyColor     = ST7735::rgb565(32, 32, 32);
        constexpr uint16_t SeparatorColor = ST7735::rgb565(24, 24, 24);
    }

    DisplayService::SetupResult DisplayService::setup() {
        if (m_started)
            return Err<Detail::ALREADY_STARTED>("Agent display service is already started");
        if (!m_lcd.started())
            return Err<Detail::LCD_NOT_STARTED>("ST7735 must be initialized before the Agent display service");
        if (m_lcd.width() < ST7735::DefaultWidth ||
            m_lcd.height() < SlotHeight * protocol::AgentCount)
            return Err<Detail::UNSUPPORTED_LAYOUT>("Agent display requires an 80x160 portrait layout");

        const auto clearResult = m_lcd.fillScreen(ST7735::BLACK);
        if (clearResult.is_err())
            return clearResult.propagate<Detail::LCD_UPDATE_FAILED>("Agent display could not clear the LCD");

        for (uint8_t id = 0; id < protocol::AgentCount; ++id) {
            const RenderResult result = renderEmpty(id);
            if (result.is_err())
                return result.propagate();
        }

        m_started                   = true;
        const BaseType_t taskResult = xTaskCreatePinnedToCore(
            taskEntry,
            "agent display",
            m_config.stackDepth,
            this,
            m_config.priority,
            &m_task,
            m_config.core);
        if (taskResult != pdPASS) {
            m_task    = nullptr;
            m_started = false;
            return Err<Detail::TASK_CREATE_FAILED>("Agent display could not create its rendering task");
        }

        return Ok();
    }

    void DisplayService::taskEntry(void* context) {
        static_cast<DisplayService*>(context)->taskLoop();
    }

    void DisplayService::taskLoop() {
        for (;;) {
            const UpdateResult result = waitAndUpdate(portMAX_DELAY);
            if (result.is_err()) {
                rememberError(result.error());
                vTaskDelay(m_config.retryDelay == 0 ? 1 : m_config.retryDelay);
            }
        }
    }

    std::optional<DisplayService::UpdateErrors> DisplayService::takeLastError() {
        portENTER_CRITICAL(&m_stateLock);
        std::optional<UpdateErrors> result = m_lastError;
        m_lastError.reset();
        portEXIT_CRITICAL(&m_stateLock);
        return result;
    }

    void DisplayService::rememberError(const UpdateErrors& error) {
        portENTER_CRITICAL(&m_stateLock);
        m_lastError = error;
        portEXIT_CRITICAL(&m_stateLock);
    }

    DisplayService::UpdateResult DisplayService::waitAndUpdate(TickType_t timeout) {
        if (!m_started)
            return Err<Detail::NOT_STARTED>("Agent display service is not started");

        uint8_t dirtyMask = takeDirtyMask();
        if (dirtyMask == 0) {
            (void)ulTaskNotifyTake(pdTRUE, timeout);
            dirtyMask = takeDirtyMask();
        }
        else {
            (void)ulTaskNotifyTake(pdTRUE, 0);
        }

        if (dirtyMask == 0)
            return Ok(false);

        for (uint8_t id = 0; id < protocol::AgentCount; ++id) {
            const uint8_t bit = static_cast<uint8_t>(uint8_t {1} << id);
            if ((dirtyMask & bit) == 0)
                continue;

            const std::optional<AgentRegistry::AgentSnapshot> agent  = m_registry.agent(id);
            const RenderResult                                result = agent.has_value()
                                                                           ? renderAgent(id, *agent)
                                                                           : renderEmpty(id);
            if (result.is_err()) {
                requeue(static_cast<uint8_t>(dirtyMask & static_cast<uint8_t>(~((uint8_t {1} << id) - 1U))));
                return result.propagate();
            }
        }

        return Ok(true);
    }

    void DisplayService::markDirty(uint8_t id) {
        if (id >= protocol::AgentCount)
            return;

        TaskHandle_t task;
        portENTER_CRITICAL(&m_stateLock);
        m_dirtyMask |= static_cast<uint8_t>(uint8_t {1} << id);
        task = m_task;
        portEXIT_CRITICAL(&m_stateLock);

        if (task != nullptr)
            xTaskNotifyGive(task);
    }

    uint8_t DisplayService::takeDirtyMask() {
        portENTER_CRITICAL(&m_stateLock);
        const uint8_t result = m_dirtyMask;
        m_dirtyMask          = 0;
        portEXIT_CRITICAL(&m_stateLock);
        return result;
    }

    void DisplayService::requeue(uint8_t mask) {
        portENTER_CRITICAL(&m_stateLock);
        m_dirtyMask |= mask;
        portEXIT_CRITICAL(&m_stateLock);
    }

    DisplayService::RenderResult DisplayService::renderEmpty(uint8_t id) {
        const char firstLine[] {
            static_cast<char>('0' + id),
            ' ',
            '-',
            '-',
            '\0',
        };
        return renderSlot(id, EmptyColor, firstLine, "");
    }

    DisplayService::RenderResult DisplayService::renderAgent(
        uint8_t                             id,
        const AgentRegistry::AgentSnapshot& agent) {
        char text[TextCharacters + 1] {};
        copyDisplayText(text, sizeof(text), agent.text.data(), agent.textSize, TextCharacters);

        char firstLine[2 + NameCharacters + 1] {};
        firstLine[0] = static_cast<char>('0' + id);
        firstLine[1] = ' ';
        copyDisplayText(
            firstLine + 2,
            sizeof(firstLine) - 2,
            agent.name.data(),
            agent.nameSize,
            NameCharacters);
        return renderSlot(id, stateColor(agent.state), firstLine, text);
    }

    DisplayService::RenderResult DisplayService::renderSlot(
        uint8_t     id,
        uint16_t    barColor,
        const char* firstLine,
        const char* secondLine) {
        const uint16_t y = static_cast<uint16_t>(id * SlotHeight);

        auto result = m_lcd.fillRect(0, y, m_lcd.width(), SlotHeight, ST7735::BLACK);
        if (result.is_err())
            return result.propagate<Detail::LCD_UPDATE_FAILED>("Agent display could not clear a slot");

        result = m_lcd.fillRect(0, y, BarWidth, SlotHeight - 1, barColor);
        if (result.is_err())
            return result.propagate<Detail::LCD_UPDATE_FAILED>("Agent display could not draw a state bar");

        result = m_lcd.drawText(firstLine, ST7735::TextStyle {
                                               .x          = TextX,
                                               .y          = static_cast<uint16_t>(y + 1),
                                               .scale      = 1,
                                               .color      = ST7735::WHITE,
                                               .background = ST7735::BLACK,
                                           });
        if (result.is_err())
            return result.propagate<Detail::LCD_UPDATE_FAILED>("Agent display could not draw an Agent name");

        if (secondLine[0] != '\0') {
            result = m_lcd.drawText(secondLine, ST7735::TextStyle {
                                                    .x          = TextX,
                                                    .y          = static_cast<uint16_t>(y + 10),
                                                    .scale      = 1,
                                                    .color      = ST7735::CYAN,
                                                    .background = ST7735::BLACK,
                                                });
            if (result.is_err())
                return result.propagate<Detail::LCD_UPDATE_FAILED>("Agent display could not draw Agent text");
        }

        result = m_lcd.fillRect(0, static_cast<uint16_t>(y + SlotHeight - 1), m_lcd.width(), 1, SeparatorColor);
        if (result.is_err())
            return result.propagate<Detail::LCD_UPDATE_FAILED>("Agent display could not draw a slot separator");

        return Ok();
    }

    uint16_t DisplayService::stateColor(protocol::AgentState state) {
        switch (state) {
            case protocol::AgentState::Off:
                return ST7735::BLACK;
            case protocol::AgentState::Idle:
                return ST7735::rgb565(32, 64, 96);
            case protocol::AgentState::Working:
                return ST7735::rgb565(0, 128, 192);
            case protocol::AgentState::WaitPermission:
                return ST7735::rgb565(240, 80, 0);
            case protocol::AgentState::WaitOption:
                return ST7735::rgb565(255, 32, 0);
            case protocol::AgentState::Done:
                return ST7735::rgb565(0, 128, 64);
            case protocol::AgentState::Error:
                return ST7735::rgb565(192, 0, 32);
        }
        return ST7735::RED;
    }

    void DisplayService::copyDisplayText(
        char*          output,
        std::size_t    capacity,
        const uint8_t* input,
        uint8_t        size,
        std::size_t    limit) {
        if (output == nullptr || capacity == 0)
            return;

        const std::size_t count = std::min({
            static_cast<std::size_t>(size),
            limit,
            capacity - 1,
        });
        for (std::size_t index = 0; index < count; ++index) {
            const uint8_t value = input[index];
            output[index]       = value >= 0x20 && value <= 0x7E
                                      ? static_cast<char>(value)
                                      : '?';
        }
        output[count] = '\0';
    }
}
