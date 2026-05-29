#pragma once

#include "driver/i2s_types.h"
#include "driver/i2s_types_legacy.h"
#include "src/fw/inc/IIS.h"

#include <cstddef>
#include <cstdint>

class INMP441 : public IIS
{

public:
    static constexpr uint32_t DEFAULT_SAMPLE_RATE       = 16000;
    static constexpr int      DEFAULT_EVENT_QUEUE_DEPTH = 8;

    struct Stats
    {
        uint32_t setupOk       = 0;
        uint32_t setupError    = 0;
        uint32_t readOk        = 0;
        uint32_t readError     = 0;
        uint32_t rxDone        = 0;
        uint32_t rxOverflow    = 0;
        uint32_t dmaError      = 0;
        uint32_t ignoredEvents = 0;

        size_t lastSamplesRead = 0;
        size_t lastEventSize   = 0;

        IIS::Err lastSetupErr = IIS::Err::OK;
        IIS::Err lastReadErr  = IIS::Err::OK;
    };

private:
    static Config makeConfig(int               bckPin,
                             int               wsPin,
                             int               dataInPin,
                             uint32_t          sampleRate,
                             i2s_channel_fmt_t channel,
                             i2s_port_t        port);

    void updateI2sEvents();

    Stats m_stats {};

public:
    INMP441(int               bckPin,
            int               wsPin,
            int               dataInPin,
            uint32_t          sampleRate = DEFAULT_SAMPLE_RATE,
            i2s_channel_fmt_t channel    = I2S_CHANNEL_FMT_ONLY_LEFT, // INMP441 L/R tied to GND.
            i2s_port_t        port       = I2S_NUM_0);

    Err setup();
    Err readRaw(int32_t* samples, size_t sampleCount, size_t& samplesRead, TickType_t ticksToWait);

    const Stats& stats() const;
    bool         healthy() const;
    void         resetStats();

    const i2s_config_t&     getDriverConfig() const;
    const i2s_pin_config_t& getPinConfig() const;
};
