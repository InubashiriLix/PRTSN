#pragma once

#include "driver/gpio.h"
#include "src/fw/inc/spi.h"
#include "src/fw/inc/std_err.h"

#include <cstddef>
#include <cstdint>

class ST7735
{
public:
    static constexpr uint16_t DefaultWidth    = 80;
    static constexpr uint16_t DefaultHeight   = 160;
    static constexpr uint16_t LandscapeWidth  = DefaultHeight;
    static constexpr uint16_t LandscapeHeight = DefaultWidth;

    static constexpr uint16_t BLACK   = 0x0000;
    static constexpr uint16_t BLUE    = 0x001F;
    static constexpr uint16_t RED     = 0xF800;
    static constexpr uint16_t GREEN   = 0x07E0;
    static constexpr uint16_t CYAN    = 0x07FF;
    static constexpr uint16_t MAGENTA = 0xF81F;
    static constexpr uint16_t YELLOW  = 0xFFE0;
    static constexpr uint16_t WHITE   = 0xFFFF;

    enum class Orientation : uint8_t
    {
        Portrait = 0,
        Landscape,
        PortraitInverted,
        LandscapeInverted,
    };

    struct Config
    {
        uint16_t    width                = DefaultWidth;
        uint16_t    height               = DefaultHeight;
        size_t      dmaBufferBytes       = 3200;
        uint8_t     columnOffset         = 26;
        uint8_t     rowOffset            = 1;
        Orientation orientation          = Orientation::Portrait;
        gpio_num_t  resetPin             = GPIO_NUM_1;
        gpio_num_t  dcPin                = GPIO_NUM_0;
        gpio_num_t  backlightPin         = GPIO_NUM_10;
        bool        useResetPin          = true;
        bool        useBacklightPin      = true;
        bool        autoDmaBuffer        = true;
        bool        useOrientationPreset = true;
        bool        invertColors         = true;
        uint8_t     madctl               = 0xC8;
    };

    struct TextStyle
    {
        uint16_t x           = 0;
        uint16_t y           = 0;
        uint8_t  scale       = 1;
        uint16_t color       = WHITE;
        uint16_t background  = BLACK;
        bool     transparent = false;
        bool     wrap        = false;
    };

    struct TextBounds
    {
        uint16_t width  = 0;
        uint16_t height = 0;
    };

    struct FrameBuffer
    {
        uint8_t* data   = nullptr;
        uint16_t width  = 0;
        uint16_t height = 0;
        size_t   stride = 0;
    };

    struct AnimationView
    {
        const uint8_t*  frames           = nullptr;
        uint16_t        width            = 0;
        uint16_t        height           = 0;
        uint16_t        frameCount       = 0;
        size_t          frameStride      = 0;
        const uint16_t* frameDurationsMs = nullptr;
    };

    struct AnimationPlayer
    {
        uint16_t frameIndex  = 0;
        uint16_t fallbackFps = 60;
        int64_t  nextFrameUs = 0;
    };

    enum class Detail : uint8_t
    {
        NONE = 0,
        NOT_STARTED,
        ALREADY_STARTED,
        INVALID_PARAM,
        INVALID_WINDOW,
        GPIO_CONFIG_FAILED,
        GPIO_WRITE_FAILED,
        RESET_FAILED,
        DMA_ALLOC_FAILED,
        COMMAND_FAILED,
        DATA_FAILED,
        TEXT_FAILED,
        SPI_FAILED,
    };

    struct Error
    {
        StdError   code   = StdError::OK;
        Detail     detail = Detail::NONE;
        SPI::Error spi {};
        esp_err_t  native = ESP_OK;

        constexpr bool ok() const noexcept {
            return code == StdError::OK && detail == Detail::NONE && spi.ok() && native == ESP_OK;
        }

        constexpr explicit operator bool() const noexcept {
            return ok();
        }
    };

public:
    ST7735(SPI& spi, size_t deviceIndex);
    ST7735(SPI& spi, size_t deviceIndex, Config config);
    ~ST7735();

    ST7735(const ST7735&)            = delete;
    ST7735& operator=(const ST7735&) = delete;

    [[nodiscard]] Error setup();
    [[nodiscard]] Error setOrientation(Orientation orientation);
    void                releaseDmaBuffer();
    [[nodiscard]] Error allocateDmaBuffer(size_t bytes);
    [[nodiscard]] Error setDmaBuffer(uint8_t* buffer, size_t bytes);
    [[nodiscard]] Error setBacklight(bool enabled);
    [[nodiscard]] Error fillScreen(uint16_t color);
    [[nodiscard]] Error fillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
    [[nodiscard]] Error drawPixel(uint16_t x, uint16_t y, uint16_t color);
    [[nodiscard]] Error drawChar(char value, uint16_t x, uint16_t y, uint16_t color);
    [[nodiscard]] Error drawChar(char value, const TextStyle& style);
    [[nodiscard]] Error drawText(const char* text, uint16_t x, uint16_t y, uint16_t color);
    [[nodiscard]] Error drawText(const char* text, const TextStyle& style);
    [[nodiscard]] Error drawFrameBuffer(const FrameBuffer& frame, uint16_t x = 0, uint16_t y = 0, uint8_t* dmaBuffer = nullptr, size_t dmaBufferBytes = 0);
    [[nodiscard]] Error pushPixels565(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* rgb565Be);
    [[nodiscard]] Error beginWriteRegion(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
    [[nodiscard]] Error writePixels565(const uint8_t* rgb565Be, size_t length);
    [[nodiscard]] Error writeColor(uint16_t color, size_t pixels);
    [[nodiscard]] Error drawFrame565(uint16_t       x,
                                     uint16_t       y,
                                     uint16_t       width,
                                     uint16_t       height,
                                     const uint8_t* rgb565Be,
                                     uint8_t*       dmaBuffer,
                                     size_t         dmaBufferBytes);
    [[nodiscard]] Error drawAnimationFrame(const AnimationView& animation,
                                           uint16_t             frameIndex,
                                           uint16_t             x,
                                           uint16_t             y,
                                           uint8_t*             dmaBuffer,
                                           size_t               dmaBufferBytes);
    [[nodiscard]] Error drawAnimationFrameLocked(const AnimationView& animation,
                                                 AnimationPlayer&     player,
                                                 uint16_t             x,
                                                 uint16_t             y,
                                                 uint8_t*             dmaBuffer,
                                                 size_t               dmaBufferBytes);

    [[nodiscard]] bool     started() const;
    [[nodiscard]] Error    lastError() const;
    [[nodiscard]] uint16_t width() const;
    [[nodiscard]] uint16_t height() const;
    [[nodiscard]] bool     dmaReady() const;
    [[nodiscard]] size_t   dmaBufferBytes() const;

    static constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
        return static_cast<uint16_t>(((r & 0xF8U) << 8U) | ((g & 0xFCU) << 3U) | (b >> 3U));
    }

    static bool        clearFrame(FrameBuffer& frame, uint16_t color);
    static bool        copyFrame(FrameBuffer& frame, const uint8_t* rgb565Be, uint16_t width, uint16_t height, size_t stride);
    static bool        copyAnimationFrame(FrameBuffer& frame, const AnimationView& animation, uint16_t frameIndex);
    static bool        fillFrameRect(FrameBuffer& frame, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
    static bool        drawFrameChar(FrameBuffer& frame, char value, const TextStyle& style);
    static bool        drawFrameText(FrameBuffer& frame, const char* text, const TextStyle& style);
    static Config      makeConfig(Orientation orientation);
    static void        applyOrientation(Config& config, Orientation orientation);
    static TextBounds  measureText(const char* text, uint8_t scale = 1);
    static const char* detailName(Detail detail) noexcept;
    static const char* orientationName(Orientation orientation) noexcept;

private:
    static constexpr uint8_t  CmdSwReset = 0x01;
    static constexpr uint8_t  CmdSlpOut  = 0x11;
    static constexpr uint8_t  CmdNorOn   = 0x13;
    static constexpr uint8_t  CmdInvOff  = 0x20;
    static constexpr uint8_t  CmdInvOn   = 0x21;
    static constexpr uint8_t  CmdDispOn  = 0x29;
    static constexpr uint8_t  CmdCaseT   = 0x2A;
    static constexpr uint8_t  CmdRaseT   = 0x2B;
    static constexpr uint8_t  CmdRamWr   = 0x2C;
    static constexpr uint8_t  CmdMadCtl  = 0x36;
    static constexpr uint8_t  CmdColMod  = 0x3A;
    static constexpr size_t   DataChunk  = 3200;
    static constexpr uint8_t  FontWidth  = 5;
    static constexpr uint8_t  FontHeight = 7;
    static constexpr uint8_t  CellWidth  = 6;
    static constexpr uint8_t  CellHeight = 8;
    static constexpr uint16_t MinFps     = 1;
    static constexpr uint16_t MaxFps     = 240;

private:
    SPI&     m_spi;
    size_t   m_deviceIndex = 0;
    Config   m_config {};
    bool     m_started = false;
    Error    m_lastError {};
    bool     m_regionOpen    = false;
    uint8_t* m_dmaBuffer     = nullptr;
    size_t   m_dmaBufferSize = 0;
    bool     m_ownsDmaBuffer = false;

private:
    [[nodiscard]] Error configurePins();
    [[nodiscard]] Error hardwareReset();
    [[nodiscard]] Error writeCommand(uint8_t command);
    [[nodiscard]] Error writeData(const uint8_t* data, size_t length);
    [[nodiscard]] Error writeCommandData(uint8_t command, const uint8_t* data, size_t length);
    [[nodiscard]] Error setAddressWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
    [[nodiscard]] Error writeRepeatedColor(uint16_t color, size_t pixels);
    [[nodiscard]] Error drawGlyphSolid(char value, uint16_t x, uint16_t y, const TextStyle& style);
    [[nodiscard]] Error drawGlyphTransparent(char value, uint16_t x, uint16_t y, const TextStyle& style);
    [[nodiscard]] Error fillGlyphRows(char             value,
                                      uint16_t         x,
                                      uint16_t         y,
                                      uint16_t         visibleWidth,
                                      uint16_t         rowOffset,
                                      uint16_t         rows,
                                      const TextStyle& style,
                                      uint8_t*         out);

    [[nodiscard]] Error                 makeError(StdError code, Detail detail, esp_err_t native = ESP_OK, SPI::Error spi = {});
    [[nodiscard]] Error                 mapSpiError(SPI::Error spi, Detail detail);
    [[nodiscard]] Error                 clearError();
    [[nodiscard]] bool                  validWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height) const;
    [[nodiscard]] bool                  validTextStyle(const TextStyle& style) const;
    [[nodiscard]] uint8_t*              dmaBufferOr(uint8_t* buffer) const;
    [[nodiscard]] size_t                dmaBufferBytesOr(size_t bytes) const;
    [[nodiscard]] static bool           validFrameBuffer(const FrameBuffer& frame);
    [[nodiscard]] static uint8_t*       framePixel(FrameBuffer& frame, uint16_t x, uint16_t y);
    static void                         writeFramePixel(FrameBuffer& frame, uint16_t x, uint16_t y, uint16_t color);
    [[nodiscard]] static bool           validAnimation(const AnimationView& animation);
    [[nodiscard]] static bool           glyphPixelOn(char value, uint16_t glyphX, uint16_t glyphY, uint8_t scale);
    [[nodiscard]] static size_t         frameSize(uint16_t width, uint16_t height);
    [[nodiscard]] static size_t         textLength(const char* text);
    [[nodiscard]] static const uint8_t* glyph(char value);
    [[nodiscard]] static uint8_t        msb(uint16_t value);
    [[nodiscard]] static uint8_t        lsb(uint16_t value);
};
