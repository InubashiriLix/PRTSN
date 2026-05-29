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
        .mode        = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = sampleRate,
        // NOTE: the INMP441 usually uses 24-bit valid data in a 32-bit slot, so we read 32-bit samples here.
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format       = channel,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = 0,     // NOTE: default interrupt allocation flags, see esp_intr_alloc.h, the 0 means use default flags;
        .dma_desc_num         = 8,     // NOTE: how many DMA descriptors to use, more -> more delay, but less chance of overflow. less -> less delay, but more chance of overflow;
        .dma_frame_num        = 256,   // NOTE: how many frames per DMA descriptor
        .use_apll             = false, // NOTE: use more precise APLL clock, if true, it can be hi-fi
        .tx_desc_auto_clear   = false, // NOTE: for speaker, useless currently
        .fixed_mclk           = 0,     // NOTE: if not 0, the MCLK will be fixed to this value, otherwise, it will be generated according to the sample rate and mclk_multiple
        .mclk_multiple        = I2S_MCLK_MULTIPLE_256,
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
        .port            = port,
        .driverConfig    = driverConfig,
        .pinConfig       = pinConfig,
        .eventQueueDepth = DEFAULT_EVENT_QUEUE_DEPTH};

    return config;
}

INMP441::Err INMP441::setup() {
    const auto err       = IIS::setup();
    m_stats.lastSetupErr = err;

    if (err == Err::OK) {
        ++m_stats.setupOk;
        updateI2sEvents();
    }
    else {
        ++m_stats.setupError;
    }

    return err;
}

INMP441::Err INMP441::readRaw(int32_t* samples, size_t sampleCount, size_t& samplesRead, TickType_t ticksToWait) {
    updateI2sEvents();

    size_t bytesRead = 0;
    auto   err       = IIS::read(samples, sampleCount * sizeof(int32_t), bytesRead, ticksToWait);
    samplesRead      = bytesRead / sizeof(int32_t);

    m_stats.lastSamplesRead = samplesRead;
    m_stats.lastReadErr     = err;

    if (err == Err::OK) {
        ++m_stats.readOk;
    }
    else {
        ++m_stats.readError;
    }

    updateI2sEvents();
    return err;
}

const INMP441::Stats& INMP441::stats() const {
    return m_stats;
}

bool INMP441::healthy() const {
    return m_stats.setupError == 0 &&
           m_stats.readError == 0 &&
           m_stats.rxOverflow == 0 &&
           m_stats.dmaError == 0;
}

void INMP441::resetStats() {
    m_stats = Stats {};
}

void INMP441::updateI2sEvents() {
    i2s_event_t event {};

    while (pollEvent(event, 0)) {
        m_stats.lastEventSize = event.size;

        switch (event.type) {
            case I2S_EVENT_RX_DONE:
                ++m_stats.rxDone;
                break;

            case I2S_EVENT_RX_Q_OVF:
                ++m_stats.rxOverflow;
                break;

            case I2S_EVENT_DMA_ERROR:
                ++m_stats.dmaError;
                break;

            default:
                ++m_stats.ignoredEvents;
                break;
        }
    }
}

const i2s_config_t& INMP441::getDriverConfig() const {
    return getConfig().driverConfig;
}

const i2s_pin_config_t& INMP441::getPinConfig() const {
    return getConfig().pinConfig;
}
