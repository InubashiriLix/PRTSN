#pragma once

#include "driver/gpio.h"
#include "src/fw/inc/Result.h"
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
        NOT_STARTED = 1,
        ALREADY_STARTED,
        INVALID_CONFIG,
        INVALID_WINDOW,
        INVALID_BUFFER,
        INVALID_TEXT_STYLE,
        REGION_NOT_OPEN,
        REGION_OVERFLOW,
        GPIO_CONFIG_FAILED,
        GPIO_WRITE_FAILED,
        RESET_FAILED,
        DMA_ALLOCATE_FAILED,
        SPI_TRANSFER_FAILED,
    };

public:
    ST7735(SPI& spi, size_t deviceIndex);
    ST7735(SPI& spi, size_t deviceIndex, Config config);
    ~ST7735();

    ST7735(const ST7735&)            = delete;
    ST7735& operator=(const ST7735&) = delete;

    /**
     * @brief 初始化显示器。Initialize the display controller.
     *
     * SPI/GPIO 原生失败会自动保留在 cause chain 中，因此调用者只处理
     * ST7735 语义，日志仍可看到底层 SPI 与 `esp_err_t`。
     *
     * @code
     * const auto result = lcd.setup();
     * if (result.is_err()) {
     *     const auto& error = result.error();
     *     // error.name(), error.message(), error.for_each_cause(...)
     * }
     * @endcode
     */
    using SetupResult = Result<void, ErrorSet<Detail::ALREADY_STARTED, Detail::INVALID_CONFIG, Detail::INVALID_BUFFER, Detail::INVALID_WINDOW, Detail::DMA_ALLOCATE_FAILED, TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::GPIO_CONFIG_FAILED>, TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::GPIO_WRITE_FAILED>, TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::RESET_FAILED>, TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::SPI_TRANSFER_FAILED>>>;

    /**
     * @brief 屏幕绘制和流式区域写入共用的结果类型。
     * Result shared by drawing and streaming-region operations.
     *
     * `beginWriteRegion()` 后只能写入该区域对应数量的 RGB565 字节；未打开
     * 区域返回 `REGION_NOT_OPEN`，写多了返回 `REGION_OVERFLOW`。
     */
    using DrawResult = Result<void, ErrorSet<Detail::NOT_STARTED, Detail::INVALID_WINDOW, Detail::INVALID_BUFFER, Detail::INVALID_TEXT_STYLE, Detail::REGION_NOT_OPEN, Detail::REGION_OVERFLOW, Detail::DMA_ALLOCATE_FAILED, TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::GPIO_WRITE_FAILED>, TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::SPI_TRANSFER_FAILED>>>;

    /** DMA 缓冲区的本地管理结果，不涉及显示器传输。 */
    using BufferResult = Result<void, ErrorSet<Detail::INVALID_BUFFER, Detail::DMA_ALLOCATE_FAILED>>;

    /** 不直接绘制像素的显示器控制操作。 */
    using ControlResult = Result<void, ErrorSet<Detail::INVALID_BUFFER, Detail::INVALID_WINDOW, TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::GPIO_WRITE_FAILED>, TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::SPI_TRANSFER_FAILED>>>;

    /** 纯内存 framebuffer 操作的错误，不会包含硬件 cause。 */
    using FrameResult = Result<void, ErrorSet<Detail::INVALID_BUFFER, Detail::INVALID_WINDOW, Detail::INVALID_TEXT_STYLE>>;

    [[nodiscard]] SetupResult   setup();
    [[nodiscard]] ControlResult setOrientation(Orientation orientation);
    void                        releaseDmaBuffer();
    [[nodiscard]] BufferResult  allocateDmaBuffer(size_t bytes);
    [[nodiscard]] BufferResult  setDmaBuffer(uint8_t* buffer, size_t bytes);
    [[nodiscard]] ControlResult setBacklight(bool enabled);
    [[nodiscard]] DrawResult    fillScreen(uint16_t color);
    [[nodiscard]] DrawResult    fillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
    [[nodiscard]] DrawResult    drawPixel(uint16_t x, uint16_t y, uint16_t color);
    [[nodiscard]] DrawResult    drawChar(char value, uint16_t x, uint16_t y, uint16_t color);
    [[nodiscard]] DrawResult    drawChar(char value, const TextStyle& style);
    [[nodiscard]] DrawResult    drawText(const char* text, uint16_t x, uint16_t y, uint16_t color);
    [[nodiscard]] DrawResult    drawText(const char* text, const TextStyle& style);
    [[nodiscard]] DrawResult    drawFrameBuffer(const FrameBuffer& frame, uint16_t x = 0, uint16_t y = 0, uint8_t* dmaBuffer = nullptr, size_t dmaBufferBytes = 0);
    [[nodiscard]] DrawResult    pushPixels565(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* rgb565Be);
    [[nodiscard]] DrawResult    beginWriteRegion(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
    [[nodiscard]] DrawResult    writePixels565(const uint8_t* rgb565Be, size_t length);
    [[nodiscard]] DrawResult    writeColor(uint16_t color, size_t pixels);
    [[nodiscard]] DrawResult    drawFrame565(uint16_t       x,
                                             uint16_t       y,
                                             uint16_t       width,
                                             uint16_t       height,
                                             const uint8_t* rgb565Be,
                                             uint8_t*       dmaBuffer,
                                             size_t         dmaBufferBytes);
    [[nodiscard]] DrawResult    drawAnimationFrame(const AnimationView& animation,
                                                   uint16_t             frameIndex,
                                                   uint16_t             x,
                                                   uint16_t             y,
                                                   uint8_t*             dmaBuffer,
                                                   size_t               dmaBufferBytes);
    [[nodiscard]] DrawResult    drawAnimationFrameLocked(const AnimationView& animation,
                                                         AnimationPlayer&     player,
                                                         uint16_t             x,
                                                         uint16_t             y,
                                                         uint8_t*             dmaBuffer,
                                                         size_t               dmaBufferBytes);

    [[nodiscard]] bool     started() const;
    [[nodiscard]] uint16_t width() const;
    [[nodiscard]] uint16_t height() const;
    [[nodiscard]] bool     dmaReady() const;
    [[nodiscard]] size_t   dmaBufferBytes() const;

    static constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
        return static_cast<uint16_t>(((r & 0xF8U) << 8U) | ((g & 0xFCU) << 3U) | (b >> 3U));
    }

    [[nodiscard]] static FrameResult clearFrame(FrameBuffer& frame, uint16_t color);
    [[nodiscard]] static FrameResult copyFrame(FrameBuffer& frame, const uint8_t* rgb565Be, uint16_t width, uint16_t height, size_t stride);
    [[nodiscard]] static FrameResult copyAnimationFrame(FrameBuffer& frame, const AnimationView& animation, uint16_t frameIndex);
    [[nodiscard]] static FrameResult fillFrameRect(FrameBuffer& frame, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
    [[nodiscard]] static FrameResult drawFrameChar(FrameBuffer& frame, char value, const TextStyle& style);
    [[nodiscard]] static FrameResult drawFrameText(FrameBuffer& frame, const char* text, const TextStyle& style);
    static Config                    makeConfig(Orientation orientation);
    static void                      applyOrientation(Config& config, Orientation orientation);
    static TextBounds                measureText(const char* text, uint8_t scale = 1);
    static const char*               orientationName(Orientation orientation) noexcept;

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
    bool     m_started              = false;
    size_t   m_regionBytesRemaining = 0;
    uint8_t* m_dmaBuffer            = nullptr;
    size_t   m_dmaBufferSize        = 0;
    bool     m_ownsDmaBuffer        = false;

private:
    using PinResult   = Result<void, ErrorSet<TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::GPIO_CONFIG_FAILED>>>;
    using ResetResult = Result<void, ErrorSet<TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::RESET_FAILED>>>;
    using IoResult    = Result<void, ErrorSet<Detail::INVALID_BUFFER, Detail::INVALID_WINDOW, TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::GPIO_WRITE_FAILED>, TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::SPI_TRANSFER_FAILED>>>;

    [[nodiscard]] PinResult   configurePins();
    [[nodiscard]] ResetResult hardwareReset();
    [[nodiscard]] IoResult    writeCommand(uint8_t command);
    [[nodiscard]] IoResult    writeData(const uint8_t* data, size_t length);
    [[nodiscard]] IoResult    writeCommandData(uint8_t command, const uint8_t* data, size_t length);
    [[nodiscard]] IoResult    setAddressWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
    [[nodiscard]] DrawResult  writeRepeatedColor(uint16_t color, size_t pixels);
    [[nodiscard]] DrawResult  drawGlyphSolid(char value, uint16_t x, uint16_t y, const TextStyle& style);
    [[nodiscard]] DrawResult  drawGlyphTransparent(char value, uint16_t x, uint16_t y, const TextStyle& style);
    [[nodiscard]] DrawResult  fillGlyphRows(char             value,
                                            uint16_t         x,
                                            uint16_t         y,
                                            uint16_t         visibleWidth,
                                            uint16_t         rowOffset,
                                            uint16_t         rows,
                                            const TextStyle& style,
                                            uint8_t*         out);

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
