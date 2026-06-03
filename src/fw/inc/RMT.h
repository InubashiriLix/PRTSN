#pragma once

#include "driver/gpio.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_types.h"
#include "src/fw/inc/std_err.h"

#include <cstddef>
#include <cstdint>

class RMT
{
public:
    static constexpr uint32_t DefaultResolutionHz    = 10 * 1000 * 1000;
    static constexpr size_t   DefaultMemBlockSymbols = 64;
    static constexpr size_t   DefaultTransQueueDepth = 4;

    using Symbol = rmt_symbol_word_t;

    struct Config
    {
        gpio_num_t gpio            = GPIO_NUM_NC;
        uint32_t   resolutionHz    = DefaultResolutionHz;
        size_t     memBlockSymbols = DefaultMemBlockSymbols;
        size_t     transQueueDepth = DefaultTransQueueDepth;
        bool       invertOut       = false;
        bool       withDma         = false;
        bool       openDrain       = false;
        bool       initLevel       = false;
    };

    enum class Detail : uint8_t
    {
        NONE = 0,
        NOT_STARTED,
        ALREADY_STARTED,
        INVALID_CONFIG,
        INVALID_BUFFER,
        CHANNEL_CREATE_FAILED,
        ENCODER_CREATE_FAILED,
        ENABLE_FAILED,
        TRANSMIT_FAILED,
        WAIT_DONE_FAILED,
        DISABLE_FAILED,
        ENCODER_DELETE_FAILED,
        CHANNEL_DELETE_FAILED,
    };

    struct Error
    {
        StdError    code   = StdError::OK;
        RMT::Detail detail = Detail::NONE;
        esp_err_t   native = ESP_OK;

        constexpr bool ok() const noexcept {
            return code == StdError::OK && detail == Detail::NONE && native == ESP_OK;
        }

        constexpr explicit operator bool() const noexcept {
            return ok();
        }
    };

public:
    explicit RMT(Config config);
    ~RMT();

    RMT(const RMT&)            = delete;
    RMT& operator=(const RMT&) = delete;

    [[nodiscard]] Error setup();
    [[nodiscard]] Error end();
    [[nodiscard]] Error transmit(const Symbol* symbols, size_t count, bool nonBlocking = false);
    [[nodiscard]] Error waitDone(uint32_t timeoutMs);

    [[nodiscard]] bool          started() const;
    [[nodiscard]] Error         lastError() const;
    [[nodiscard]] const Config& config() const;

    static uint16_t    ticksFromNs(uint32_t resolutionHz, uint32_t ns);
    static const char* detailName(Detail detail) noexcept;

private:
    Config               m_config {};
    rmt_channel_handle_t m_channel = nullptr;
    rmt_encoder_handle_t m_encoder = nullptr;
    bool                 m_started = false;
    Error                m_lastError {};

private:
    [[nodiscard]] Error makeError(StdError code, Detail detail, esp_err_t native);
    [[nodiscard]] Error clearError();
    [[nodiscard]] Error ensureStarted();
    [[nodiscard]] bool  validConfig() const;
};
