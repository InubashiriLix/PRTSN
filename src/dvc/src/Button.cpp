#include "../inc/Button.h"

Button::Button(uint8_t pin, uint8_t activeLevel, uint8_t inputMode, uint32_t debounceMs)
    : m_pin(pin),
      m_activeLevel(activeLevel),
      m_inputMode(inputMode),
      m_debounceMs(debounceMs) {}

bool Button::setup() {
    pinMode(m_pin, m_inputMode);

    const uint8_t  rawLevel = static_cast<uint8_t>(digitalRead(m_pin));
    const uint32_t nowMs    = millis();

    m_lastRawLevel      = rawLevel;
    m_lastRawChangeMs   = nowMs;
    m_lastStateChangeMs = nowMs;
    m_state             = stateFromLevel(rawLevel);

    return true;
}

bool Button::setupInterrupt(Callback callback, void* context, int interruptMode) {
    if (!setup()) {
        return false;
    }

    setCallback(callback, context);
    attachInterruptArg(m_pin, interruptEntry, this, interruptMode);
    m_interruptEnabled = true;

    return true;
}

void Button::detachInterruptMode() {
    if (m_interruptEnabled) {
        detachInterrupt(m_pin);
    }

    m_interruptEnabled = false;
    m_interruptPending = false;
}

void Button::update() {
    if (m_interruptEnabled && !m_interruptPending) {
        return;
    }

    if (m_interruptPending) {
        handleInterrupt();
        m_lastInterruptMs = millis();
    }

    const uint8_t  rawLevel = static_cast<uint8_t>(digitalRead(m_pin));
    const uint32_t nowMs    = millis();

    if (rawLevel != m_lastRawLevel) {
        m_lastRawLevel    = rawLevel;
        m_lastRawChangeMs = nowMs;
        return;
    }

    if (nowMs - m_lastRawChangeMs < m_debounceMs) {
        return;
    }

    const State newState = stateFromLevel(rawLevel);
    if (newState == m_state) {
        return;
    }

    m_state             = newState;
    m_lastStateChangeMs = nowMs;

    const Event event = eventFromState(newState);
    pushEvent(event);

    if (m_callback != nullptr) {
        m_callback(event, m_state, m_callbackContext);
    }
}

void Button::setCallback(Callback callback, void* context) {
    m_callback        = callback;
    m_callbackContext = context;
}

bool Button::hasEvent() const {
    return m_eventCount > 0;
}

bool Button::popEvent(Event& outEvent) {
    if (m_eventCount == 0) {
        outEvent = Event::NONE;
        return false;
    }

    outEvent    = m_eventQueue[m_eventTail];
    m_eventTail = (m_eventTail + 1) % EVENT_QUEUE_CAPACITY;
    --m_eventCount;

    return true;
}

Button::Event Button::popEvent() {
    Event event = Event::NONE;
    popEvent(event);
    return event;
}

Button::State Button::state() const {
    return m_state;
}

bool Button::isPressed() const {
    return m_state == State::PRESSED;
}

bool Button::isReleased() const {
    return m_state == State::RELEASED;
}

uint8_t Button::pin() const {
    return m_pin;
}

uint32_t Button::lastStateChangeMs() const {
    return m_lastStateChangeMs;
}

uint32_t Button::lastInterruptMs() const {
    return m_lastInterruptMs;
}

void IRAM_ATTR Button::interruptEntry(void* arg) {
    auto* self = static_cast<Button*>(arg);
    if (self == nullptr) {
        return;
    }

    self->m_interruptPending = true;
}

Button::State Button::stateFromLevel(uint8_t level) const {
    return level == m_activeLevel ? State::PRESSED : State::RELEASED;
}

Button::Event Button::eventFromState(State state) const {
    return state == State::PRESSED ? Event::PRESSED : Event::RELEASED;
}

void Button::pushEvent(Event event) {
    if (event == Event::NONE) {
        return;
    }

    if (m_eventCount == EVENT_QUEUE_CAPACITY) {
        m_eventTail = (m_eventTail + 1) % EVENT_QUEUE_CAPACITY;
        --m_eventCount;
    }

    m_eventQueue[m_eventHead] = event;
    m_eventHead               = (m_eventHead + 1) % EVENT_QUEUE_CAPACITY;
    ++m_eventCount;
}

void Button::handleInterrupt() {
    noInterrupts();
    m_interruptPending = false;
    interrupts();
}
