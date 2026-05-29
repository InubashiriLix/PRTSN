#include "src/dvc/inc/INMP441.h"
#include "driver/i2s_types_legacy.h"

INMP441::INMP441(
    int               bckPin,
    int               wsPin,
    int               dataInPin,
    uint32_t          sampleRate,
    i2s_channel_fmt_t channel,
    i2s_port_t        port)
    : IIS(makeConfig(bckPin, wsPin, dataInPin, sampleRate, channel, port)) {
}

INMP441::Config INMP441::makeConfig(
    int               bckPin,
    int               wsPin,
    int               dataInPin,
    uint32_t          sampleRate,
    i2s_channel_fmt_t channel,
    i2s_port_t        port) {

    i2s_config_t driverConfig = {
        .mode                 = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate          = sampleRate,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT, // TODO: we may want to use other bits
        .channel_format       = channel,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = 0, // TODO: what's this?
        .dma_desc_num         = 8, // TODO: what's this, enough or too big?
        .dma_frame_num        = 256,
        .use_apll             = false, // TODO: waht's this?
        .tx_desc_auto_clear   = false, // TODO: what's this?
        .fixed_mclk           = 0,
        .mclk_multiple        = I2S_MCLK_MULTIPLE_256, // TODO: what;s this?
        .bits_per_chan        = I2S_BITS_PER_CHAN_32BIT,
    };

    i2s_pin_config_t pinConfig = {
        .mck_io_num   = I2S_PIN_NO_CHANGE,
        .bck_io_num   = bckPin,
        .ws_io_num    = wsPin,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = dataInPin,
    };

    Config config = {
        .port         = port,
        .driverConfig = driverConfig,
        .pinConfig    = pinConfig};

    return config;
}

INMP441::Err INMP441::setup() {
    return IIS::setup();
}

INMP441::Err INMP441::readRaw(int32_t* samples, size_t sampleCount, size_t& samplesRead, TickType_t ticksToWait) {
    size_t bytesRead = 0;
    auto   err       = IIS::read(samples, sampleCount * sizeof(int32_t), bytesRead, ticksToWait);
    samplesRead      = bytesRead / sizeof(int32_t);
    return err;
}

const i2s_config_t& INMP441::getDriverConfig() const {
    return getConfig().driverConfig;
}

const i2s_pin_config_t& INMP441::getPinConfig() const {
    return getConfig().pinConfig;
}
