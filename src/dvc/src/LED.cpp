#include "../inc/LED.h"

#include <Arduino.h>

LED::LED(uint8_t pin, State ledInitState) : m_pin(pin), m_state(ledInitState) {}

LED::State LED::setState(LED::State state) {
    m_state = state;
    digitalWrite(m_pin, static_cast<uint8_t>(m_state));
    return state;
}

LED::State LED::getState() {
    return m_state;
}

LED::State LED::toggleState() {
    m_state = (m_state == State::DIGITAL_HIGH) ? State::DIGITAL_LOW : State::DIGITAL_HIGH;
    digitalWrite(m_pin, static_cast<uint8_t>(m_state));
    return m_state;
}

LED::State LED::toogleState() {
    return toggleState();
}

bool LED::setup() {
    pinMode(m_pin, OUTPUT);
    setState(m_state);
    return true;
}

bool LED::update() {
    toggleState();
    return true;
}

bool LED::updateByIntervalMs(uint32_t updateIntervalMs) {
    static uint32_t lastUpdateMs = 0;
    const uint32_t  nowMs        = millis();

    if (nowMs - lastUpdateMs >= updateIntervalMs) {
        toggleState();
        lastUpdateMs = nowMs;
        return true;
    }

    return false;
}

bool LED::updateByIntervalNum(uint32_t updateIntervalNum) {
    static uint32_t updateCounter = 0;

    if (++updateCounter >= updateIntervalNum) {
        toggleState();
        updateCounter = 0;
        return true;
    }

    return false;
}
