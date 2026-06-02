#pragma once

#include <cstdint>
class Pwm
{
public:
    constexpr static uint32_t DefaultFrequencyHz    = 1000;
    constexpr static uint32_t DefaultResolutionBits = 8;
    constexpr static uint8_t  DefaultChannel        = 1;

    struct Config
    {
        uint8_t  pin;
        uint32_t frequencyHz    = DefaultFrequencyHz;
        uint8_t  resolutionBits = DefaultResolutionBits;
        uint8_t  channel        = DefaultChannel;
        uint32_t initialDuty    = 0;
        bool     invert         = false;
    };

    explicit Pwm(const Config& config);

    bool setup();
    void end();

    bool setDutyRaw(uint32_t duty);
    bool setDutyPercent(float dutyPercent);
    bool setFrequencyHz(uint32_t frequencyHz);

    uint32_t getFrequencyHz() const;
    uint32_t getDutyRaw() const;
    uint32_t getMaxDuty() const;
    bool     getStarted() const;

private:
    Config   m_config;
    bool     m_started = false;
    uint32_t m_duty    = 0;
};
