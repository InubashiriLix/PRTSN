#include "src/fw/inc/RMT.h"

#include <algorithm>

RMT::RMT(Config config) : m_config(config) {}

RMT::~RMT() {
    end();
}

RMT::Error RMT::setup() {
    if (m_started) {
        return makeError(StdError::INVALID_STATE, Detail::ALREADY_STARTED, ESP_ERR_INVALID_STATE);
    }
    if (!validConfig()) {
        return makeError(StdError::INVALID_ARGS, Detail::INVALID_CONFIG, ESP_ERR_INVALID_ARG);
    }

    rmt_tx_channel_config_t channelConfig {};
    channelConfig.gpio_num          = m_config.gpio;
    channelConfig.clk_src           = RMT_CLK_SRC_DEFAULT;
    channelConfig.resolution_hz     = m_config.resolutionHz;
    channelConfig.mem_block_symbols = m_config.memBlockSymbols;
    channelConfig.trans_queue_depth = m_config.transQueueDepth;
    channelConfig.intr_priority     = 0;
    channelConfig.flags.invert_out  = m_config.invertOut ? 1 : 0;
    channelConfig.flags.with_dma    = m_config.withDma ? 1 : 0;
    channelConfig.flags.io_od_mode  = m_config.openDrain ? 1 : 0;
    channelConfig.flags.init_level  = m_config.initLevel ? 1 : 0;

    esp_err_t native = rmt_new_tx_channel(&channelConfig, &m_channel);
    if (native != ESP_OK) {
        m_channel = nullptr;
        return makeError(toStdErr(native), Detail::CHANNEL_CREATE_FAILED, native);
    }

    rmt_copy_encoder_config_t encoderConfig {};
    native = rmt_new_copy_encoder(&encoderConfig, &m_encoder);
    if (native != ESP_OK) {
        rmt_del_channel(m_channel);
        m_channel = nullptr;
        return makeError(toStdErr(native), Detail::ENCODER_CREATE_FAILED, native);
    }

    native = rmt_enable(m_channel);
    if (native != ESP_OK) {
        rmt_del_encoder(m_encoder);
        rmt_del_channel(m_channel);
        m_encoder = nullptr;
        m_channel = nullptr;
        return makeError(toStdErr(native), Detail::ENABLE_FAILED, native);
    }

    m_started = true;
    return clearError();
}

RMT::Error RMT::end() {
    if (!m_started && m_channel == nullptr && m_encoder == nullptr) {
        return clearError();
    }

    if (m_started && m_channel != nullptr) {
        const esp_err_t native = rmt_disable(m_channel);
        if (native != ESP_OK) {
            return makeError(toStdErr(native), Detail::DISABLE_FAILED, native);
        }
    }
    m_started = false;

    if (m_encoder != nullptr) {
        const esp_err_t native = rmt_del_encoder(m_encoder);
        if (native != ESP_OK) {
            return makeError(toStdErr(native), Detail::ENCODER_DELETE_FAILED, native);
        }
        m_encoder = nullptr;
    }

    if (m_channel != nullptr) {
        const esp_err_t native = rmt_del_channel(m_channel);
        if (native != ESP_OK) {
            return makeError(toStdErr(native), Detail::CHANNEL_DELETE_FAILED, native);
        }
        m_channel = nullptr;
    }

    return clearError();
}

RMT::Error RMT::transmit(const Symbol* symbols, size_t count, bool nonBlocking) {
    Error err = ensureStarted();
    if (!err) {
        return err;
    }
    if (symbols == nullptr || count == 0) {
        return makeError(StdError::INVALID_ARGS, Detail::INVALID_BUFFER, ESP_ERR_INVALID_ARG);
    }

    rmt_transmit_config_t transmitConfig {};
    transmitConfig.loop_count              = 0;
    transmitConfig.flags.eot_level         = 0;
    transmitConfig.flags.queue_nonblocking = nonBlocking ? 1 : 0;

    const esp_err_t native = rmt_transmit(m_channel, m_encoder, symbols, count * sizeof(Symbol), &transmitConfig);
    if (native != ESP_OK) {
        return makeError(toStdErr(native), Detail::TRANSMIT_FAILED, native);
    }

    return clearError();
}

RMT::Error RMT::waitDone(uint32_t timeoutMs) {
    Error err = ensureStarted();
    if (!err) {
        return err;
    }

    const int       timeout = timeoutMs == 0 ? -1 : static_cast<int>(timeoutMs);
    const esp_err_t native  = rmt_tx_wait_all_done(m_channel, timeout);
    if (native != ESP_OK) {
        return makeError(toStdErr(native), Detail::WAIT_DONE_FAILED, native);
    }

    return clearError();
}

bool RMT::started() const {
    return m_started;
}

RMT::Error RMT::lastError() const {
    return m_lastError;
}

const RMT::Config& RMT::config() const {
    return m_config;
}

uint16_t RMT::ticksFromNs(uint32_t resolutionHz, uint32_t ns) {
    const uint64_t ticks = (static_cast<uint64_t>(resolutionHz) * ns + 999999999ULL) / 1000000000ULL;
    return static_cast<uint16_t>(std::min<uint64_t>(ticks, 0x7FFFU));
}

const char* RMT::detailName(Detail detail) noexcept {
    switch (detail) {
        case Detail::NONE:
            return "NONE";
        case Detail::NOT_STARTED:
            return "NOT_STARTED";
        case Detail::ALREADY_STARTED:
            return "ALREADY_STARTED";
        case Detail::INVALID_CONFIG:
            return "INVALID_CONFIG";
        case Detail::INVALID_BUFFER:
            return "INVALID_BUFFER";
        case Detail::CHANNEL_CREATE_FAILED:
            return "CHANNEL_CREATE_FAILED";
        case Detail::ENCODER_CREATE_FAILED:
            return "ENCODER_CREATE_FAILED";
        case Detail::ENABLE_FAILED:
            return "ENABLE_FAILED";
        case Detail::TRANSMIT_FAILED:
            return "TRANSMIT_FAILED";
        case Detail::WAIT_DONE_FAILED:
            return "WAIT_DONE_FAILED";
        case Detail::DISABLE_FAILED:
            return "DISABLE_FAILED";
        case Detail::ENCODER_DELETE_FAILED:
            return "ENCODER_DELETE_FAILED";
        case Detail::CHANNEL_DELETE_FAILED:
            return "CHANNEL_DELETE_FAILED";
    }

    return "UNKNOWN";
}

RMT::Error RMT::makeError(StdError code, Detail detail, esp_err_t native) {
    m_lastError = Error {.code = code, .detail = detail, .native = native};
    return m_lastError;
}

RMT::Error RMT::clearError() {
    m_lastError = Error {};
    return m_lastError;
}

RMT::Error RMT::ensureStarted() {
    if (m_started && m_channel != nullptr && m_encoder != nullptr) {
        return Error {};
    }

    return makeError(StdError::INVALID_STATE, Detail::NOT_STARTED, ESP_ERR_INVALID_STATE);
}

bool RMT::validConfig() const {
    return m_config.gpio != GPIO_NUM_NC &&
           m_config.resolutionHz > 0 &&
           m_config.memBlockSymbols >= 2 &&
           m_config.transQueueDepth > 0;
}
