#pragma once

#include "driver/gpio.h"
#include "src/fw/inc/RMT.h"
#include "src/fw/inc/std_err.h"

#include <cstddef>
#include <cstdint>

class WS2812
{
public:
    static constexpr uint32_t DefaultResolutionHz = RMT::DefaultResolutionHz;
    static constexpr uint32_t DefaultT0HNs        = 400;
    static constexpr uint32_t DefaultT0LNs        = 850;
    static constexpr uint32_t DefaultT1HNs        = 800;
    static constexpr uint32_t DefaultT1LNs        = 450;
    static constexpr uint32_t DefaultResetUs      = 80;
    static constexpr uint32_t DefaultTimeoutMs    = 20;

    enum class ColorOrder : uint8_t
    {
        RGB = 0,
        GRB,
        BRG,
        BGR,
    };

    struct Color
    {
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
        uint8_t a = 255;
    };

    struct Config
    {
        size_t     pixelCount    = 1;
        ColorOrder colorOrder    = ColorOrder::GRB;
        uint8_t    brightness    = 255;
        uint32_t   resolutionHz  = DefaultResolutionHz;
        uint32_t   t0hNs         = DefaultT0HNs;
        uint32_t   t0lNs         = DefaultT0LNs;
        uint32_t   t1hNs         = DefaultT1HNs;
        uint32_t   t1lNs         = DefaultT1LNs;
        uint32_t   resetUs       = DefaultResetUs;
        uint32_t   showTimeoutMs = DefaultTimeoutMs;
    };

    enum class Detail : uint8_t
    {
        NONE = 0,
        NOT_STARTED,
        ALREADY_STARTED,
        INVALID_CONFIG,
        INVALID_INDEX,
        NO_BUFFER,
        SETUP_RMT_FAILED,
        END_RMT_FAILED,
        TRANSMIT_FAILED,
        WAIT_FAILED,
    };

    struct Error
    {
        StdError   code   = StdError::OK;
        Detail     detail = Detail::NONE;
        RMT::Error rmt {};
        esp_err_t  native = ESP_OK;

        constexpr bool ok() const noexcept {
            return code == StdError::OK && detail == Detail::NONE && rmt.ok() && native == ESP_OK;
        }

        constexpr explicit operator bool() const noexcept {
            return ok();
        }
    };

public:
    WS2812(gpio_num_t pin, size_t pixelCount);
    WS2812(gpio_num_t pin, Config config);
    ~WS2812();

    WS2812(const WS2812&)            = delete;
    WS2812& operator=(const WS2812&) = delete;

    [[nodiscard]] Error setup();
    [[nodiscard]] Error end();
    [[nodiscard]] Error show();
    [[nodiscard]] Error clear(bool flush = true);

    [[nodiscard]] Error setPixel(size_t index, Color color);
    [[nodiscard]] Error setColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a, size_t index);
    [[nodiscard]] Error setAllColors(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    [[nodiscard]] Error setRed(uint8_t r, size_t index);
    [[nodiscard]] Error setGreen(uint8_t g, size_t index);
    [[nodiscard]] Error setBlue(uint8_t b, size_t index);
    [[nodiscard]] Error setAlpha(uint8_t a, size_t index);
    [[nodiscard]] Error setBrightness(uint8_t brightness, bool flush = false);

    [[nodiscard]] bool    started() const;
    [[nodiscard]] size_t  pixelCount() const;
    [[nodiscard]] uint8_t brightness() const;
    [[nodiscard]] Color   pixel(size_t index) const;
    [[nodiscard]] Error   lastError() const;

    static const char* detailName(Detail detail) noexcept;
    static const char* colorOrderName(ColorOrder order) noexcept;

private:
    gpio_num_t   m_pin = GPIO_NUM_NC;
    Config       m_config {};
    RMT          m_rmt;
    Color*       m_pixels      = nullptr;
    RMT::Symbol* m_symbols     = nullptr;
    size_t       m_symbolCount = 0;
    bool         m_started     = false;
    Error        m_lastError {};

private:
    [[nodiscard]] Error allocateBuffers();
    void                releaseBuffers();
    void                encode();
    void                encodePixel(size_t pixelIndex, size_t& symbolIndex);
    void                encodeByte(uint8_t value, size_t& symbolIndex);
    void                encodeBit(bool one, size_t& symbolIndex);
    void                orderedBytes(Color color, uint8_t& first, uint8_t& second, uint8_t& third) const;
    [[nodiscard]] Color scaled(Color color) const;

    [[nodiscard]] Error makeError(StdError code, Detail detail, esp_err_t native = ESP_OK, RMT::Error rmt = {});
    [[nodiscard]] Error mapRmtError(RMT::Error rmt, Detail detail);
    [[nodiscard]] Error clearError();
    [[nodiscard]] bool  validConfig() const;
    [[nodiscard]] bool  validIndex(size_t index) const;

    static RMT::Config makeRmtConfig(gpio_num_t pin, const Config& config);
};

using WS2812LED = WS2812;
