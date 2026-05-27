#pragma once

#include <Arduino.h>
#include <cstdint>

class HardwareTimer
{
public:
    static constexpr uint32_t DefaultResolutionHz = 1000000;

    using Callback = void (*)(void* context);

    enum class Error : uint8_t
    {
        OK = 0,
        NOT_STARTED,
        ALREADY_STARTED,
        INVALID_ARGUMENT,
        BEGIN_FAILED,
        CALLBACK_NOT_SET,
    };

    struct Config
    {
        uint32_t resolutionHz = DefaultResolutionHz;
        uint64_t alarmTicks   = 0;
        bool     autoReload   = true;
        uint64_t reloadCount  = 0;
        bool     autoStart    = false;
    };

    struct Status
    {
        bool     started      = false;
        bool     running      = false;
        uint32_t resolutionHz = DefaultResolutionHz;
        uint64_t alarmTicks   = 0;
        bool     autoReload   = true;
        uint64_t reloadCount  = 0;
        Error    lastError    = Error::OK;
    };

public:
    HardwareTimer();
    explicit HardwareTimer(const Config& config);
    HardwareTimer(const HardwareTimer&)            = delete;
    HardwareTimer& operator=(const HardwareTimer&) = delete;
    ~HardwareTimer();

    bool begin();
    bool begin(const Config& config);
    void end();

    bool attachInterrupt(Callback callback, void* context = nullptr);
    void detachInterrupt();

    bool setAlarmTicks(uint64_t alarmTicks, bool autoReload = true, uint64_t reloadCount = 0);
    bool setPeriodUs(uint64_t periodUs, bool autoReload = true, uint64_t reloadCount = 0);

    bool start();
    void stop();
    bool restart();
    void write(uint64_t ticks);

    uint64_t readTicks() const;
    uint64_t readUs() const;
    uint32_t getResolutionHz() const;

    bool   getStarted() const;
    bool   getRunning() const;
    Status getStatus() const;
    Error  getLastError() const;

private:
    Config      m_config {};
    Status      m_status {};
    hw_timer_t* m_timer    = nullptr;
    Callback    m_callback = nullptr;
    void*       m_context  = nullptr;

    static void IRAM_ATTR interruptEntry(void* context);

    bool ensureStarted();
    void setError(Error error);
};
