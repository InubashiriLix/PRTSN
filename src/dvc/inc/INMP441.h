#pragma once

#include "driver/i2s_types.h"
#include "driver/i2s_types_legacy.h"
#include "src/fw/inc/IIS.h"

class INMP441 : public IIS
{

public:
    static constexpr uint32_t DEFAULT_SAMPLE_RATE = 16000;

private:
    static Config makeConfig(int               bckPin,
                             int               wsPin,
                             int               dataInPin,
                             uint32_t          sampleRate,
                             i2s_channel_fmt_t channel,
                             i2s_port_t        port);

public:
    INMP441(int               bckPin,
            int               wsPin,
            int               dataInPin,
            uint32_t          sampleRate = DEFAULT_SAMPLE_RATE,
            i2s_channel_fmt_t channel    = I2S_CHANNEL_FMT_RIGHT_LEFT,
            i2s_port_t        port       = I2S_NUM_0);

    Err setup();
    Err readRaw(int32_t* samples, size_t sampleCount, size_t& samplesRead, TickType_t ticksToWait);

    const i2s_config_t&     getDriverConfig() const;
    const i2s_pin_config_t& getPinConfig() const;
};
