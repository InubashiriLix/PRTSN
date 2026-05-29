#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

class Button
{
public:
    enum class State : uint8_t
    {
        PRESSED = 0,
        RELEASED,
    };

    enum class Event : uint8_t
    {
        NONE = 0,
        PRESSED,
        RELEASED,
    };

    using Callback = void (*)(Event event, State state, void* context);

private:
    constexpr static size_t EVENT_QUEUE_CAPACITY = 8;

    uint8_t  m_pin;
    uint8_t  m_activeLevel;
    uint8_t  m_inputMode;
    uint32_t m_debounceMs;

    State    m_state             = State::RELEASED;
    uint8_t  m_lastRawLevel      = HIGH;
    uint32_t m_lastRawChangeMs   = 0;
    uint32_t m_lastStateChangeMs = 0;

    Event  m_eventQueue[EVENT_QUEUE_CAPACITY] {};
    size_t m_eventHead  = 0;
    size_t m_eventTail  = 0;
    size_t m_eventCount = 0;

    Callback m_callback        = nullptr;
    void*    m_callbackContext = nullptr;

    bool              m_interruptEnabled = false;
    volatile bool     m_interruptPending = false;
    volatile uint32_t m_lastInterruptMs  = 0;

public:
    Button(uint8_t  pin,
           uint8_t  activeLevel = LOW,
           uint8_t  inputMode   = INPUT_PULLUP,
           uint32_t debounceMs  = 30);

    bool setup();
    bool setupInterrupt(Callback callback      = nullptr,
                        void*    context       = nullptr,
                        int      interruptMode = CHANGE);
    void detachInterruptMode();

    void update();
    void setCallback(Callback callback, void* context = nullptr);

    bool  hasEvent() const;
    bool  popEvent(Event& outEvent);
    Event popEvent();

    State state() const;
    bool  isPressed() const;
    bool  isReleased() const;

    uint8_t  pin() const;
    uint32_t lastStateChangeMs() const;
    uint32_t lastInterruptMs() const;

private:
    static void IRAM_ATTR interruptEntry(void* arg);

    State stateFromLevel(uint8_t level) const;
    Event eventFromState(State state) const;
    void  pushEvent(Event event);
    void  handleInterrupt();
};
