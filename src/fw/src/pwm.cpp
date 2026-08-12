#include "src/fw/inc/pwm.h"
#include "driver/ledc.h"
#include "esp32-hal-ledc.h"

Pwm::Pwm(const Config& config) : m_config(config), m_duty(m_config.initialDuty) {}

bool Pwm::setup() {
    if (m_config.pin == GPIO_NUM_NC || m_config.frequencyHz == 0 || m_config.resolutionBits == 0) {
        return false;
    }

    if (m_config.resolutionBits > 14) {
        return false;
    }

    const uint8_t pin = static_cast<uint8_t>(m_config.pin);
    const bool    ok  = ledcAttachChannel(
        pin,
        m_config.frequencyHz,
        m_config.resolutionBits,
        m_config.channel);

    if (!ok) {
        return false;
    }

    if (m_config.invert) {
        if (!ledcOutputInvert(pin, true)) {
            ledcDetach(pin);
            return false;
        }
    }

    m_started = true;

    if (!setDutyRaw(m_config.initialDuty)) {
        end();
        return false;
    }

    return true;
}

void Pwm::end() {
    if (!m_started) {
        return;
    }

    const uint8_t pin = static_cast<uint8_t>(m_config.pin);
    ledcWrite(pin, 0);
    ledcDetach(pin);

    m_started = false;
    m_duty    = 0;
}

bool Pwm::setDutyRaw(uint32_t duty) {
    if (!m_started) {
        return false;
    }

    if (duty > getMaxDuty()) {
        return false;
    }

    if (!ledcWrite(static_cast<uint8_t>(m_config.pin), duty)) {
        return false;
    }

    m_duty = duty;
    return true;
}

bool Pwm::setDutyPercent(float percent) {
    if (percent < 0.0 || percent > 100.0) {
        return false;
    }

    const uint32_t duty = static_cast<uint32_t>((percent / 100.0) * getMaxDuty());

    return setDutyRaw(duty);
}

bool Pwm::setFrequencyHz(uint32_t frequencyHz) {
    if (!m_started || frequencyHz == 0) {
        return false;
    }
    const uint32_t actual = ledcChangeFrequency(static_cast<uint8_t>(m_config.pin), frequencyHz, m_config.resolutionBits);

    if (actual == 0) {
        return false;
    }

    m_config.frequencyHz = actual;
    return true;
}

uint32_t Pwm::getDutyRaw() const {
    return m_duty;
}

uint32_t Pwm::getMaxDuty() const {
    return (1UL << m_config.resolutionBits) - 1UL;
}

uint32_t Pwm::getFrequencyHz() const {
    return m_config.frequencyHz;
}

bool Pwm::getStarted() const {
    return m_started;
}
