#pragma once

#include "driver/gpio.h"
#include "src/fw/inc/RMT.h"
#include "src/fw/inc/std_err.h"
#include "src/fw/inc/Result.h"
#include "src/alg/inc/alg_color.h"

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

    using Color = Color;

    enum class ColorOrder : uint8_t
    {
        RGB = 0,
        GRB,
        BRG,
        BGR,
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
        NOT_STARTED = 1,
        ALREADY_STARTED,
        INVALID_CONFIG,
        INVALID_INDEX,
        ALLOCATE_BUFFER_FAILED,
        NO_BUFFER,
        SETUP_RMT_FAILED,
        END_RMT_FAILED,
        RMT_TRANSMIT_FAILED,
        RMT_WAIT_FAILED,
    };

public:
    WS2812(gpio_num_t pin, size_t pixelCount);
    WS2812(gpio_num_t pin, Config config);
    ~WS2812();

    WS2812(const WS2812&)            = delete;
    WS2812& operator=(const WS2812&) = delete;

    // the Result defines
    using SetupErrors     = ErrorSet<WS2812::Detail::ALREADY_STARTED,
                                     WS2812::Detail::ALLOCATE_BUFFER_FAILED,
                                     WS2812::Detail::INVALID_CONFIG,
                                     WS2812::Detail::NO_BUFFER,
                                     Detail::SETUP_RMT_FAILED>;
    using SetupResult     = Result<void, SetupErrors>;
    using EndResult       = Result<void, ErrorSet<Detail::NOT_STARTED, Detail::END_RMT_FAILED>>;
    using ShowErrors      = ErrorSet<WS2812::Detail::NOT_STARTED,
                                     WS2812::Detail::NO_BUFFER,
                                     WS2812::Detail::RMT_TRANSMIT_FAILED,
                                     WS2812::Detail::RMT_WAIT_FAILED>;
    using ShowResult      = Result<void, ShowErrors>;
    using ClearErrors     = ErrorSet<WS2812::Detail::NO_BUFFER>;
    using ClearResult     = Result<void, ClearErrors>;
    using ClearShowErrors = ShowErrors;
    using ClearShowResult = Result<void, ClearShowErrors>;
    // gaze into a pit

    [[nodiscard]] SetupResult     setup();
    [[nodiscard]] EndResult       end();
    [[nodiscard]] ShowResult      show();
    [[nodiscard]] ClearResult     clear();
    [[nodiscard]] ClearShowResult clearShow();

    using SetPixelResult       = Result<void, ErrorSet<WS2812::Detail::INVALID_INDEX, WS2812::Detail::NO_BUFFER>>;
    using SetColorResult       = SetPixelResult;
    using SetAllColorsResult   = Result<void, ErrorSet<WS2812::Detail::NO_BUFFER>>;
    using SetSingleColorResult = SetPixelResult;
    using SetAlphaResult       = SetPixelResult;
    using SetBrightnessErrors  = ShowErrors;
    using SetBrightnessResult  = Result<void, SetBrightnessErrors>;
    [[nodiscard]] SetPixelResult       setPixel(size_t index, Color color);
    [[nodiscard]] SetColorResult       setColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a, size_t index);
    [[nodiscard]] SetAllColorsResult   setAllColors(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    [[nodiscard]] SetSingleColorResult setRed(uint8_t r, size_t index);
    [[nodiscard]] SetSingleColorResult setGreen(uint8_t g, size_t index);
    [[nodiscard]] SetSingleColorResult setBlue(uint8_t b, size_t index);
    [[nodiscard]] SetAlphaResult       setAlpha(uint8_t a, size_t index);
    [[nodiscard]] SetBrightnessResult  setBrightness(uint8_t brightness, bool flush = false);

    [[nodiscard]] bool    started() const;
    [[nodiscard]] size_t  pixelCount() const;
    [[nodiscard]] uint8_t brightness() const;
    [[nodiscard]] Color   pixel(size_t index) const;

    static const char* colorOrderName(ColorOrder order) noexcept;

private:
    gpio_num_t   m_pin = GPIO_NUM_NC;
    Config       m_config {};
    RMT          m_rmt;
    Color*       m_pixels      = nullptr;
    RMT::Symbol* m_symbols     = nullptr;
    size_t       m_symbolCount = 0;
    bool         m_started     = false;

private:
    using AllocateBufferErrors = ErrorSet<StdError::NO_MEM>;
    using AllocateBufferResult = Result<void, AllocateBufferErrors>;
    [[nodiscard]] AllocateBufferResult allocateBuffers();
    void                               releaseBuffers();
    void                               encode();
    void                               encodePixel(size_t pixelIndex, size_t& symbolIndex);
    void                               encodeByte(uint8_t value, size_t& symbolIndex);
    void                               encodeBit(bool one, size_t& symbolIndex);
    void                               orderedBytes(Color color, uint8_t& first, uint8_t& second, uint8_t& third) const;
    [[nodiscard]] Color                scaled(Color color) const;

    [[nodiscard]] bool validConfig() const;
    [[nodiscard]] bool validIndex(size_t index) const;

    static RMT::Config makeRmtConfig(gpio_num_t pin, const Config& config);
};

using WS2812LED = WS2812;
