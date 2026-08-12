#include "src/dvc/inc/ST7735.h"

#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace
{
    // clang-format off
    constexpr uint8_t Font5x7[][5] = {
        {0x00, 0x00, 0x00, 0x00, 0x00}, // space
        {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
        {0x00, 0x07, 0x00, 0x07, 0x00}, // "
        {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
        {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
        {0x23, 0x13, 0x08, 0x64, 0x62}, // %
        {0x36, 0x49, 0x55, 0x22, 0x50}, // &
        {0x00, 0x05, 0x03, 0x00, 0x00}, // '
        {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
        {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
        {0x14, 0x08, 0x3E, 0x08, 0x14}, // *
        {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
        {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
        {0x08, 0x08, 0x08, 0x08, 0x08}, // -
        {0x00, 0x60, 0x60, 0x00, 0x00}, // .
        {0x20, 0x10, 0x08, 0x04, 0x02}, // /
        {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
        {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
        {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
        {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
        {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
        {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
        {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
        {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
        {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
        {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
        {0x00, 0x36, 0x36, 0x00, 0x00}, // :
        {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
        {0x08, 0x14, 0x22, 0x41, 0x00}, // <
        {0x14, 0x14, 0x14, 0x14, 0x14}, // =
        {0x00, 0x41, 0x22, 0x14, 0x08}, // >
        {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
        {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
        {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
        {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
        {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
        {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
        {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
        {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
        {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
        {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
        {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
        {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
        {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
        {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
        {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
        {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
        {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
        {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
        {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
        {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
        {0x46, 0x49, 0x49, 0x49, 0x31}, // S
        {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
        {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
        {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
        {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
        {0x63, 0x14, 0x08, 0x14, 0x63}, // X
        {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
        {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
        {0x00, 0x7F, 0x41, 0x41, 0x00}, // [
        {0x02, 0x04, 0x08, 0x10, 0x20}, // backslash
        {0x00, 0x41, 0x41, 0x7F, 0x00}, // ]
        {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
        {0x40, 0x40, 0x40, 0x40, 0x40}, // _
        {0x00, 0x01, 0x02, 0x04, 0x00}, // `
        {0x20, 0x54, 0x54, 0x54, 0x78}, // a
        {0x7F, 0x48, 0x44, 0x44, 0x38}, // b
        {0x38, 0x44, 0x44, 0x44, 0x20}, // c
        {0x38, 0x44, 0x44, 0x48, 0x7F}, // d
        {0x38, 0x54, 0x54, 0x54, 0x18}, // e
        {0x08, 0x7E, 0x09, 0x01, 0x02}, // f
        {0x0C, 0x52, 0x52, 0x52, 0x3E}, // g
        {0x7F, 0x08, 0x04, 0x04, 0x78}, // h
        {0x00, 0x44, 0x7D, 0x40, 0x00}, // i
        {0x20, 0x40, 0x44, 0x3D, 0x00}, // j
        {0x7F, 0x10, 0x28, 0x44, 0x00}, // k
        {0x00, 0x41, 0x7F, 0x40, 0x00}, // l
        {0x7C, 0x04, 0x18, 0x04, 0x78}, // m
        {0x7C, 0x08, 0x04, 0x04, 0x78}, // n
        {0x38, 0x44, 0x44, 0x44, 0x38}, // o
        {0x7C, 0x14, 0x14, 0x14, 0x08}, // p
        {0x08, 0x14, 0x14, 0x18, 0x7C}, // q
        {0x7C, 0x08, 0x04, 0x04, 0x08}, // r
        {0x48, 0x54, 0x54, 0x54, 0x20}, // s
        {0x04, 0x3F, 0x44, 0x40, 0x20}, // t
        {0x3C, 0x40, 0x40, 0x20, 0x7C}, // u
        {0x1C, 0x20, 0x40, 0x20, 0x1C}, // v
        {0x3C, 0x40, 0x30, 0x40, 0x3C}, // w
        {0x44, 0x28, 0x10, 0x28, 0x44}, // x
        {0x0C, 0x50, 0x50, 0x50, 0x3C}, // y
        {0x44, 0x64, 0x54, 0x4C, 0x44}, // z
        {0x00, 0x08, 0x36, 0x41, 0x00}, // {
        {0x00, 0x00, 0x7F, 0x00, 0x00}, // |
        {0x00, 0x41, 0x36, 0x08, 0x00}, // }
        {0x08, 0x04, 0x08, 0x10, 0x08}, // ~
    };
    // clang-format on
}

ST7735::ST7735(SPI& spi, size_t deviceIndex) : ST7735(spi, deviceIndex, Config {}) {}

ST7735::ST7735(SPI& spi, size_t deviceIndex, Config config)
    : m_spi(spi),
      m_deviceIndex(deviceIndex),
      m_config(config) {
    if (m_config.useOrientationPreset) {
        applyOrientation(m_config, m_config.orientation);
    }
}

ST7735::~ST7735() {
    releaseDmaBuffer();
}

ST7735::SetupResult ST7735::setup() {
    if (m_started) {
        return Err<Detail::ALREADY_STARTED>("ST7735 is already initialized");
    }

    if (m_config.useOrientationPreset) {
        applyOrientation(m_config, m_config.orientation);
    }
    if (!validWindow(0, 0, m_config.width, m_config.height) ||
        m_config.width > std::numeric_limits<uint16_t>::max() - m_config.columnOffset ||
        m_config.height > std::numeric_limits<uint16_t>::max() - m_config.rowOffset ||
        (m_config.autoDmaBuffer && m_config.dmaBufferBytes == 0) ||
        !GPIO_IS_VALID_OUTPUT_GPIO(m_config.dcPin) ||
        (m_config.useResetPin && !GPIO_IS_VALID_OUTPUT_GPIO(m_config.resetPin)) ||
        (m_config.useBacklightPin && !GPIO_IS_VALID_OUTPUT_GPIO(m_config.backlightPin))) {
        return Err<Detail::INVALID_CONFIG>("ST7735 configuration is invalid");
    }

    if (m_config.autoDmaBuffer && m_dmaBuffer == nullptr) {
        const BufferResult dmaResult = allocateDmaBuffer(m_config.dmaBufferBytes);
        if (dmaResult.is_err())
            return dmaResult.propagate();
    }

    const PinResult pinResult = configurePins();
    if (pinResult.is_err())
        return pinResult.propagate();

    const ResetResult resetResult = hardwareReset();
    if (resetResult.is_err())
        return resetResult.propagate();

    IoResult ioResult = writeCommand(CmdSwReset);
    if (ioResult.is_err())
        return ioResult.propagate();
    vTaskDelay(pdMS_TO_TICKS(150));

    ioResult = writeCommand(CmdSlpOut);
    if (ioResult.is_err())
        return ioResult.propagate();
    vTaskDelay(pdMS_TO_TICKS(120));

    static constexpr uint8_t frmCtr[]  = {0x05, 0x3A, 0x3A};
    static constexpr uint8_t frmCtr3[] = {0x05, 0x3A, 0x3A, 0x05, 0x3A, 0x3A};
    static constexpr uint8_t invCtr[]  = {0x03};
    static constexpr uint8_t pwCtr1[]  = {0x62, 0x02, 0x04};
    static constexpr uint8_t pwCtr2[]  = {0xC0};
    static constexpr uint8_t pwCtr3[]  = {0x0D, 0x00};
    static constexpr uint8_t pwCtr4[]  = {0x8D, 0x6A};
    static constexpr uint8_t pwCtr5[]  = {0x8D, 0xEE};
    static constexpr uint8_t vmCtr1[]  = {0x0E};
    static constexpr uint8_t gammaP[]  = {0x10, 0x0E, 0x02, 0x03, 0x0E, 0x07, 0x02, 0x07, 0x0A, 0x12, 0x27, 0x37, 0x00, 0x0D, 0x0E, 0x10};
    static constexpr uint8_t gammaN[]  = {0x10, 0x0E, 0x03, 0x03, 0x0F, 0x06, 0x02, 0x08, 0x0A, 0x13, 0x26, 0x36, 0x00, 0x0D, 0x0E, 0x10};

    ioResult = writeCommandData(0xB1, frmCtr, sizeof(frmCtr));
    if (ioResult.is_err())
        return ioResult.propagate();
    ioResult = writeCommandData(0xB2, frmCtr, sizeof(frmCtr));
    if (ioResult.is_err())
        return ioResult.propagate();
    ioResult = writeCommandData(0xB3, frmCtr3, sizeof(frmCtr3));
    if (ioResult.is_err())
        return ioResult.propagate();
    ioResult = writeCommandData(0xB4, invCtr, sizeof(invCtr));
    if (ioResult.is_err())
        return ioResult.propagate();
    ioResult = writeCommandData(0xC0, pwCtr1, sizeof(pwCtr1));
    if (ioResult.is_err())
        return ioResult.propagate();
    ioResult = writeCommandData(0xC1, pwCtr2, sizeof(pwCtr2));
    if (ioResult.is_err())
        return ioResult.propagate();
    ioResult = writeCommandData(0xC2, pwCtr3, sizeof(pwCtr3));
    if (ioResult.is_err())
        return ioResult.propagate();
    ioResult = writeCommandData(0xC3, pwCtr4, sizeof(pwCtr4));
    if (ioResult.is_err())
        return ioResult.propagate();
    ioResult = writeCommandData(0xC4, pwCtr5, sizeof(pwCtr5));
    if (ioResult.is_err())
        return ioResult.propagate();
    ioResult = writeCommandData(0xC5, vmCtr1, sizeof(vmCtr1));
    if (ioResult.is_err())
        return ioResult.propagate();

    const uint8_t colorMode = 0x05;
    ioResult                = writeCommandData(CmdColMod, &colorMode, 1);
    if (ioResult.is_err())
        return ioResult.propagate();
    vTaskDelay(pdMS_TO_TICKS(10));

    ioResult = writeCommandData(CmdMadCtl, &m_config.madctl, 1);
    if (ioResult.is_err())
        return ioResult.propagate();

    ioResult = writeCommand(m_config.invertColors ? CmdInvOn : CmdInvOff);
    if (ioResult.is_err())
        return ioResult.propagate();

    ioResult = writeCommandData(0xE0, gammaP, sizeof(gammaP));
    if (ioResult.is_err())
        return ioResult.propagate();
    ioResult = writeCommandData(0xE1, gammaN, sizeof(gammaN));
    if (ioResult.is_err())
        return ioResult.propagate();

    ioResult = writeCommand(CmdNorOn);
    if (ioResult.is_err())
        return ioResult.propagate();
    vTaskDelay(pdMS_TO_TICKS(10));

    ioResult = writeCommand(CmdDispOn);
    if (ioResult.is_err())
        return ioResult.propagate();
    vTaskDelay(pdMS_TO_TICKS(120));

    if (m_config.useBacklightPin) {
        const ControlResult backlightResult = setBacklight(true);
        if (backlightResult.is_err())
            return backlightResult.propagate();
    }

    m_started = true;
    return Ok();
}

ST7735::ControlResult ST7735::setOrientation(Orientation orientation) {
    const Config previousConfig = m_config;
    applyOrientation(m_config, orientation);

    if (!m_started) {
        return Ok();
    }

    const IoResult result = writeCommandData(CmdMadCtl, &m_config.madctl, 1);
    if (result.is_err()) {
        m_config = previousConfig;
        return result.propagate();
    }
    return Ok();
}

void ST7735::releaseDmaBuffer() {
    if (m_ownsDmaBuffer && m_dmaBuffer != nullptr) {
        heap_caps_free(m_dmaBuffer);
    }

    m_dmaBuffer     = nullptr;
    m_dmaBufferSize = 0;
    m_ownsDmaBuffer = false;
}

ST7735::BufferResult ST7735::allocateDmaBuffer(size_t bytes) {
    if (bytes == 0 || bytes > std::numeric_limits<size_t>::max() - 3U) {
        return Err<Detail::INVALID_BUFFER>("ST7735 DMA buffer size is invalid");
    }

    releaseDmaBuffer();

    const size_t alignedBytes = (bytes + 3U) & ~static_cast<size_t>(3U);
    m_dmaBuffer               = static_cast<uint8_t*>(heap_caps_malloc(alignedBytes, MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
    if (m_dmaBuffer == nullptr) {
        return Err<Detail::DMA_ALLOCATE_FAILED>("ST7735 DMA buffer allocation failed");
    }

    m_dmaBufferSize = alignedBytes;
    m_ownsDmaBuffer = true;
    return Ok();
}

ST7735::BufferResult ST7735::setDmaBuffer(uint8_t* buffer, size_t bytes) {
    if (buffer == nullptr || bytes == 0 || !esp_ptr_dma_capable(buffer)) {
        return Err<Detail::INVALID_BUFFER>("ST7735 external buffer is null, empty, or not DMA-capable");
    }

    releaseDmaBuffer();
    m_dmaBuffer     = buffer;
    m_dmaBufferSize = bytes & ~static_cast<size_t>(1U);
    m_ownsDmaBuffer = false;

    if (m_dmaBufferSize == 0) {
        return Err<Detail::INVALID_BUFFER>("ST7735 external DMA buffer must contain at least one RGB565 pixel");
    }

    return Ok();
}

ST7735::ControlResult ST7735::setBacklight(bool enabled) {
    if (!m_config.useBacklightPin) {
        return Ok();
    }

    const esp_err_t err = gpio_set_level(m_config.backlightPin, enabled ? 1 : 0);
    if (err != ESP_OK) {
        return NativeErr<Detail::GPIO_WRITE_FAILED>(err, "ST7735 backlight GPIO write failed");
    }

    return Ok();
}

ST7735::DrawResult ST7735::fillScreen(uint16_t color) {
    return fillRect(0, 0, m_config.width, m_config.height, color);
}

ST7735::DrawResult ST7735::fillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
    if (!m_started) {
        return Err<Detail::NOT_STARTED>("ST7735 is not initialized");
    }
    if (!validWindow(x, y, width, height)) {
        return Err<Detail::INVALID_WINDOW>("ST7735 fill rectangle is outside the display");
    }

    const DrawResult regionResult = beginWriteRegion(x, y, width, height);
    if (regionResult.is_err())
        return regionResult;

    return writeRepeatedColor(color, static_cast<size_t>(width) * height);
}

ST7735::DrawResult ST7735::drawPixel(uint16_t x, uint16_t y, uint16_t color) {
    const uint8_t data[] {msb(color), lsb(color)};
    return pushPixels565(x, y, 1, 1, data);
}

ST7735::DrawResult ST7735::drawChar(char value, uint16_t x, uint16_t y, uint16_t color) {
    return drawChar(value, TextStyle {.x = x, .y = y, .color = color});
}

ST7735::DrawResult ST7735::drawChar(char value, const TextStyle& style) {
    if (!m_started) {
        return Err<Detail::NOT_STARTED>("ST7735 is not initialized");
    }
    if (!validTextStyle(style)) {
        return Err<Detail::INVALID_TEXT_STYLE>("ST7735 text style is invalid");
    }

    if (style.transparent) {
        return drawGlyphTransparent(value, style.x, style.y, style);
    }

    return drawGlyphSolid(value, style.x, style.y, style);
}

ST7735::DrawResult ST7735::drawText(const char* text, uint16_t x, uint16_t y, uint16_t color) {
    return drawText(text, TextStyle {.x = x, .y = y, .color = color});
}

ST7735::DrawResult ST7735::drawText(const char* text, const TextStyle& style) {
    if (!m_started) {
        return Err<Detail::NOT_STARTED>("ST7735 is not initialized");
    }
    if (text == nullptr || !validTextStyle(style)) {
        return Err<Detail::INVALID_TEXT_STYLE>("ST7735 text or text style is invalid");
    }

    const uint16_t cellWidth  = static_cast<uint16_t>(CellWidth * style.scale);
    const uint16_t cellHeight = static_cast<uint16_t>(CellHeight * style.scale);
    uint16_t       cursorX    = style.x;
    uint16_t       cursorY    = style.y;

    while (*text != '\0' && cursorY < m_config.height) {
        const char value = *text++;
        if (value == '\n') {
            cursorX = style.x;
            cursorY = static_cast<uint16_t>(cursorY + cellHeight);
            continue;
        }

        if (style.wrap && cursorX + cellWidth > m_config.width) {
            cursorX = style.x;
            cursorY = static_cast<uint16_t>(cursorY + cellHeight);
        }

        if (cursorY >= m_config.height) {
            break;
        }

        TextStyle glyphStyle = style;
        glyphStyle.x         = cursorX;
        glyphStyle.y         = cursorY;

        const DrawResult result = drawChar(value, glyphStyle);
        if (result.is_err())
            return result;

        cursorX = static_cast<uint16_t>(cursorX + cellWidth);
        if (!style.wrap && cursorX >= m_config.width) {
            break;
        }
    }

    return Ok();
}

ST7735::DrawResult ST7735::drawFrameBuffer(const FrameBuffer& frame, uint16_t x, uint16_t y, uint8_t* dmaBuffer, size_t dmaBufferBytes) {
    if (!m_started) {
        return Err<Detail::NOT_STARTED>("ST7735 is not initialized");
    }
    if (!validFrameBuffer(frame) || !validWindow(x, y, frame.width, frame.height)) {
        return Err<Detail::INVALID_BUFFER>("ST7735 framebuffer or destination window is invalid");
    }
    if (frame.stride != static_cast<size_t>(frame.width) * 2) {
        return Err<Detail::INVALID_BUFFER>("ST7735 direct framebuffer drawing requires a packed RGB565 stride");
    }

    return drawFrame565(x, y, frame.width, frame.height, frame.data, dmaBuffer, dmaBufferBytes);
}

ST7735::DrawResult ST7735::pushPixels565(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* rgb565Be) {
    if (!m_started) {
        return Err<Detail::NOT_STARTED>("ST7735 is not initialized");
    }
    if (!validWindow(x, y, width, height)) {
        return Err<Detail::INVALID_WINDOW>("ST7735 pixel window is invalid");
    }
    if (rgb565Be == nullptr) {
        return Err<Detail::INVALID_BUFFER>("ST7735 RGB565 pixel buffer is null");
    }

    const DrawResult regionResult = beginWriteRegion(x, y, width, height);
    if (regionResult.is_err())
        return regionResult;

    return writePixels565(rgb565Be, frameSize(width, height));
}

ST7735::DrawResult ST7735::beginWriteRegion(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    if (!m_started) {
        return Err<Detail::NOT_STARTED>("ST7735 is not initialized");
    }
    if (!validWindow(x, y, width, height)) {
        return Err<Detail::INVALID_WINDOW>("ST7735 write region is invalid");
    }

    IoResult ioResult = setAddressWindow(x, y, width, height);
    if (ioResult.is_err())
        return ioResult.propagate();

    ioResult = writeCommand(CmdRamWr);
    if (ioResult.is_err()) {
        m_regionBytesRemaining = 0;
        return ioResult.propagate();
    }

    m_regionBytesRemaining = frameSize(width, height);
    return Ok();
}

ST7735::DrawResult ST7735::writePixels565(const uint8_t* rgb565Be, size_t length) {
    if (!m_started)
        return Err<Detail::NOT_STARTED>("ST7735 is not initialized");
    if (m_regionBytesRemaining == 0)
        return Err<Detail::REGION_NOT_OPEN>("ST7735 has no active write region");
    if ((length & 1U) != 0)
        return Err<Detail::INVALID_BUFFER>("ST7735 RGB565 byte count must be even");
    if (length > m_regionBytesRemaining)
        return Err<Detail::REGION_OVERFLOW>("ST7735 pixel data exceeds the active write region");

    const IoResult ioResult = writeData(rgb565Be, length);
    if (ioResult.is_err())
        return ioResult.propagate();
    m_regionBytesRemaining -= length;
    return Ok();
}

ST7735::DrawResult ST7735::writeColor(uint16_t color, size_t pixels) {
    if (!m_started)
        return Err<Detail::NOT_STARTED>("ST7735 is not initialized");
    if (m_regionBytesRemaining == 0)
        return Err<Detail::REGION_NOT_OPEN>("ST7735 has no active write region");
    if (pixels > m_regionBytesRemaining / 2)
        return Err<Detail::REGION_OVERFLOW>("ST7735 color data exceeds the active write region");

    return writeRepeatedColor(color, pixels);
}

ST7735::DrawResult ST7735::drawFrame565(uint16_t       x,
                                        uint16_t       y,
                                        uint16_t       width,
                                        uint16_t       height,
                                        const uint8_t* rgb565Be,
                                        uint8_t*       dmaBuffer,
                                        size_t         dmaBufferBytes) {
    if (!m_started) {
        return Err<Detail::NOT_STARTED>("ST7735 is not initialized");
    }
    if (!validWindow(x, y, width, height)) {
        return Err<Detail::INVALID_WINDOW>("ST7735 frame destination window is invalid");
    }
    if (rgb565Be == nullptr) {
        return Err<Detail::INVALID_BUFFER>("ST7735 frame buffer is null");
    }

    DrawResult drawResult = beginWriteRegion(x, y, width, height);
    if (drawResult.is_err())
        return drawResult;

    const size_t totalBytes = frameSize(width, height);
    dmaBuffer               = dmaBufferOr(dmaBuffer);
    dmaBufferBytes          = dmaBufferBytesOr(dmaBufferBytes);
    if (dmaBuffer == nullptr || dmaBufferBytes == 0) {
        return writePixels565(rgb565Be, totalBytes);
    }

    const size_t chunkBytes = dmaBufferBytes & ~static_cast<size_t>(1U);
    if (chunkBytes == 0) {
        return Err<Detail::INVALID_BUFFER>("ST7735 DMA buffer is too small for one RGB565 pixel");
    }

    size_t offset = 0;
    while (offset < totalBytes) {
        const size_t count = std::min(totalBytes - offset, chunkBytes);
        std::memcpy(dmaBuffer, rgb565Be + offset, count);

        drawResult = writePixels565(dmaBuffer, count);
        if (drawResult.is_err())
            return drawResult;

        offset += count;
    }

    return Ok();
}

ST7735::DrawResult ST7735::drawAnimationFrame(const AnimationView& animation,
                                              uint16_t             frameIndex,
                                              uint16_t             x,
                                              uint16_t             y,
                                              uint8_t*             dmaBuffer,
                                              size_t               dmaBufferBytes) {
    if (!validAnimation(animation)) {
        return Err<Detail::INVALID_BUFFER>("ST7735 animation view is invalid");
    }

    const uint16_t safeIndex = static_cast<uint16_t>(frameIndex % animation.frameCount);
    const uint8_t* frame     = animation.frames + static_cast<size_t>(safeIndex) * animation.frameStride;
    return drawFrame565(x, y, animation.width, animation.height, frame, dmaBuffer, dmaBufferBytes);
}

ST7735::DrawResult ST7735::drawAnimationFrameLocked(const AnimationView& animation,
                                                    AnimationPlayer&     player,
                                                    uint16_t             x,
                                                    uint16_t             y,
                                                    uint8_t*             dmaBuffer,
                                                    size_t               dmaBufferBytes) {
    if (!validAnimation(animation)) {
        return Err<Detail::INVALID_BUFFER>("ST7735 animation view is invalid");
    }
    if (animation.frameDurationsMs == nullptr && (player.fallbackFps < MinFps || player.fallbackFps > MaxFps)) {
        return Err<Detail::INVALID_BUFFER>("ST7735 animation fallback FPS is invalid");
    }

    const uint16_t currentFrame = static_cast<uint16_t>(player.frameIndex % animation.frameCount);
    int64_t        nowUs        = esp_timer_get_time();
    if (player.nextFrameUs == 0) {
        player.nextFrameUs = nowUs;
    }

    const int64_t waitUs = player.nextFrameUs - nowUs;
    if (waitUs > 0) {
        if (waitUs > 2000) {
            vTaskDelay(pdMS_TO_TICKS(static_cast<uint32_t>(waitUs / 1000)));
        }
        while (esp_timer_get_time() < player.nextFrameUs) {
            taskYIELD();
        }
    }

    const DrawResult drawResult = drawAnimationFrame(animation, currentFrame, x, y, dmaBuffer, dmaBufferBytes);
    if (drawResult.is_err())
        return drawResult;

    const uint16_t durationMs      = animation.frameDurationsMs != nullptr ? std::max<uint16_t>(animation.frameDurationsMs[currentFrame], 1) : static_cast<uint16_t>(std::max<int64_t>(1000LL / player.fallbackFps, 1));
    const int64_t  frameIntervalUs = static_cast<int64_t>(durationMs) * 1000LL;

    player.frameIndex = static_cast<uint16_t>((player.frameIndex + 1U) % animation.frameCount);
    player.nextFrameUs += frameIntervalUs;

    nowUs = esp_timer_get_time();
    if (player.nextFrameUs < nowUs - frameIntervalUs) {
        player.nextFrameUs = nowUs + frameIntervalUs;
    }

    return Ok();
}

bool ST7735::started() const {
    return m_started;
}

uint16_t ST7735::width() const {
    return m_config.width;
}

uint16_t ST7735::height() const {
    return m_config.height;
}

bool ST7735::dmaReady() const {
    return m_dmaBuffer != nullptr && m_dmaBufferSize > 0;
}

size_t ST7735::dmaBufferBytes() const {
    return m_dmaBufferSize;
}

ST7735::FrameResult ST7735::clearFrame(FrameBuffer& frame, uint16_t color) {
    if (!validFrameBuffer(frame)) {
        return Err<Detail::INVALID_BUFFER>("ST7735 framebuffer is invalid");
    }

    const uint8_t hi = msb(color);
    const uint8_t lo = lsb(color);
    for (uint16_t y = 0; y < frame.height; ++y) {
        uint8_t* row = frame.data + static_cast<size_t>(y) * frame.stride;
        for (uint16_t x = 0; x < frame.width; ++x) {
            row[x * 2]     = hi;
            row[x * 2 + 1] = lo;
        }
    }

    return Ok();
}

ST7735::FrameResult ST7735::copyFrame(FrameBuffer& frame, const uint8_t* rgb565Be, uint16_t width, uint16_t height, size_t stride) {
    if (!validFrameBuffer(frame) || rgb565Be == nullptr || width == 0 || height == 0 || stride < static_cast<size_t>(width) * 2) {
        return Err<Detail::INVALID_BUFFER>("ST7735 source or destination framebuffer is invalid");
    }
    if (width > frame.width || height > frame.height) {
        return Err<Detail::INVALID_WINDOW>("ST7735 source frame does not fit the destination framebuffer");
    }

    const size_t rowBytes = static_cast<size_t>(width) * 2;
    for (uint16_t y = 0; y < height; ++y) {
        std::memcpy(frame.data + static_cast<size_t>(y) * frame.stride, rgb565Be + static_cast<size_t>(y) * stride, rowBytes);
    }

    return Ok();
}

ST7735::FrameResult ST7735::copyAnimationFrame(FrameBuffer& frame, const AnimationView& animation, uint16_t frameIndex) {
    if (!validAnimation(animation) || !validFrameBuffer(frame)) {
        return Err<Detail::INVALID_BUFFER>("ST7735 animation or destination framebuffer is invalid");
    }
    if (animation.width > frame.width || animation.height > frame.height) {
        return Err<Detail::INVALID_WINDOW>("ST7735 animation frame does not fit the destination framebuffer");
    }

    const uint16_t safeIndex = static_cast<uint16_t>(frameIndex % animation.frameCount);
    const uint8_t* source    = animation.frames + static_cast<size_t>(safeIndex) * animation.frameStride;
    return copyFrame(frame, source, animation.width, animation.height, static_cast<size_t>(animation.width) * 2);
}

ST7735::FrameResult ST7735::fillFrameRect(FrameBuffer& frame, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
    if (!validFrameBuffer(frame))
        return Err<Detail::INVALID_BUFFER>("ST7735 framebuffer is invalid");
    if (width == 0 || height == 0 || x >= frame.width || y >= frame.height)
        return Err<Detail::INVALID_WINDOW>("ST7735 framebuffer rectangle is invalid");

    const uint16_t clippedWidth  = std::min<uint16_t>(width, static_cast<uint16_t>(frame.width - x));
    const uint16_t clippedHeight = std::min<uint16_t>(height, static_cast<uint16_t>(frame.height - y));
    const uint8_t  hi            = msb(color);
    const uint8_t  lo            = lsb(color);

    for (uint16_t rowIndex = 0; rowIndex < clippedHeight; ++rowIndex) {
        uint8_t* row = frame.data + static_cast<size_t>(y + rowIndex) * frame.stride + static_cast<size_t>(x) * 2;
        for (uint16_t col = 0; col < clippedWidth; ++col) {
            row[col * 2]     = hi;
            row[col * 2 + 1] = lo;
        }
    }

    return Ok();
}

ST7735::FrameResult ST7735::drawFrameChar(FrameBuffer& frame, char value, const TextStyle& style) {
    if (!validFrameBuffer(frame))
        return Err<Detail::INVALID_BUFFER>("ST7735 framebuffer is invalid");
    if (style.scale == 0 || style.x >= frame.width || style.y >= frame.height)
        return Err<Detail::INVALID_TEXT_STYLE>("ST7735 framebuffer text style is invalid");

    const uint16_t glyphWidth  = static_cast<uint16_t>(CellWidth * style.scale);
    const uint16_t glyphHeight = static_cast<uint16_t>(CellHeight * style.scale);
    const uint16_t xLimit      = std::min<uint16_t>(glyphWidth, static_cast<uint16_t>(frame.width - style.x));
    const uint16_t yLimit      = std::min<uint16_t>(glyphHeight, static_cast<uint16_t>(frame.height - style.y));

    for (uint16_t py = 0; py < yLimit; ++py) {
        for (uint16_t px = 0; px < xLimit; ++px) {
            const bool on = glyphPixelOn(value, px, py, style.scale);
            if (on) {
                writeFramePixel(frame, static_cast<uint16_t>(style.x + px), static_cast<uint16_t>(style.y + py), style.color);
            }
            else if (!style.transparent) {
                writeFramePixel(frame, static_cast<uint16_t>(style.x + px), static_cast<uint16_t>(style.y + py), style.background);
            }
        }
    }

    return Ok();
}

ST7735::FrameResult ST7735::drawFrameText(FrameBuffer& frame, const char* text, const TextStyle& style) {
    if (!validFrameBuffer(frame))
        return Err<Detail::INVALID_BUFFER>("ST7735 framebuffer is invalid");
    if (text == nullptr || style.scale == 0 || style.x >= frame.width || style.y >= frame.height)
        return Err<Detail::INVALID_TEXT_STYLE>("ST7735 framebuffer text or style is invalid");

    const uint16_t cellWidth  = static_cast<uint16_t>(CellWidth * style.scale);
    const uint16_t cellHeight = static_cast<uint16_t>(CellHeight * style.scale);
    uint16_t       cursorX    = style.x;
    uint16_t       cursorY    = style.y;

    while (*text != '\0' && cursorY < frame.height) {
        const char value = *text++;
        if (value == '\n') {
            cursorX = style.x;
            cursorY = static_cast<uint16_t>(cursorY + cellHeight);
            continue;
        }

        if (style.wrap && cursorX + cellWidth > frame.width) {
            cursorX = style.x;
            cursorY = static_cast<uint16_t>(cursorY + cellHeight);
        }

        if (cursorY >= frame.height) {
            break;
        }

        TextStyle glyphStyle          = style;
        glyphStyle.x                  = cursorX;
        glyphStyle.y                  = cursorY;
        const FrameResult glyphResult = drawFrameChar(frame, value, glyphStyle);
        if (glyphResult.is_err())
            return glyphResult;

        cursorX = static_cast<uint16_t>(cursorX + cellWidth);
        if (!style.wrap && cursorX >= frame.width) {
            break;
        }
    }

    return Ok();
}

ST7735::Config ST7735::makeConfig(Orientation orientation) {
    Config config {};
    applyOrientation(config, orientation);
    return config;
}

void ST7735::applyOrientation(Config& config, Orientation orientation) {
    config.orientation = orientation;

    switch (orientation) {
        case Orientation::Portrait:
            config.width        = DefaultWidth;
            config.height       = DefaultHeight;
            config.columnOffset = 26;
            config.rowOffset    = 1;
            config.madctl       = 0xC8;
            return;

        case Orientation::Landscape:
            config.width        = LandscapeWidth;
            config.height       = LandscapeHeight;
            config.columnOffset = 1;
            config.rowOffset    = 26;
            config.madctl       = 0xA8;
            return;

        case Orientation::PortraitInverted:
            config.width        = DefaultWidth;
            config.height       = DefaultHeight;
            config.columnOffset = 26;
            config.rowOffset    = 1;
            config.madctl       = 0x08;
            return;

        case Orientation::LandscapeInverted:
            config.width        = LandscapeWidth;
            config.height       = LandscapeHeight;
            config.columnOffset = 1;
            config.rowOffset    = 26;
            config.madctl       = 0x68;
            return;
    }
}

ST7735::TextBounds ST7735::measureText(const char* text, uint8_t scale) {
    if (text == nullptr || scale == 0) {
        return {};
    }

    const uint16_t cellWidth  = static_cast<uint16_t>(CellWidth * scale);
    const uint16_t cellHeight = static_cast<uint16_t>(CellHeight * scale);
    uint16_t       lineWidth  = 0;
    uint16_t       maxWidth   = 0;
    uint16_t       lines      = 1;

    while (*text != '\0') {
        const char value = *text++;
        if (value == '\n') {
            maxWidth  = std::max(maxWidth, lineWidth);
            lineWidth = 0;
            ++lines;
            continue;
        }

        lineWidth = static_cast<uint16_t>(lineWidth + cellWidth);
    }

    maxWidth = std::max(maxWidth, lineWidth);
    return TextBounds {.width = maxWidth, .height = static_cast<uint16_t>(lines * cellHeight)};
}

const char* ST7735::orientationName(Orientation orientation) noexcept {
    switch (orientation) {
        case Orientation::Portrait:
            return "Portrait";
        case Orientation::Landscape:
            return "Landscape";
        case Orientation::PortraitInverted:
            return "PortraitInverted";
        case Orientation::LandscapeInverted:
            return "LandscapeInverted";
    }

    return "Unknown";
}

ST7735::PinResult ST7735::configurePins() {
    uint64_t pinMask = 1ULL << static_cast<uint8_t>(m_config.dcPin);
    if (m_config.useResetPin) {
        pinMask |= 1ULL << static_cast<uint8_t>(m_config.resetPin);
    }
    if (m_config.useBacklightPin) {
        pinMask |= 1ULL << static_cast<uint8_t>(m_config.backlightPin);
    }

    gpio_config_t config {};
    config.pin_bit_mask = pinMask;
    config.mode         = GPIO_MODE_OUTPUT;
    config.pull_up_en   = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type    = GPIO_INTR_DISABLE;

    const esp_err_t err = gpio_config(&config);
    if (err != ESP_OK) {
        return NativeErr<Detail::GPIO_CONFIG_FAILED>(err, "ST7735 GPIO configuration failed");
    }

    if (m_config.useBacklightPin) {
        const esp_err_t backlightResult = gpio_set_level(m_config.backlightPin, 0);
        if (backlightResult != ESP_OK)
            return NativeErr<Detail::GPIO_CONFIG_FAILED>(backlightResult, "ST7735 initial backlight GPIO write failed");
    }
    const esp_err_t dcResult = gpio_set_level(m_config.dcPin, 1);
    if (dcResult != ESP_OK)
        return NativeErr<Detail::GPIO_CONFIG_FAILED>(dcResult, "ST7735 initial data/command GPIO write failed");
    if (m_config.useResetPin) {
        const esp_err_t resetResult = gpio_set_level(m_config.resetPin, 1);
        if (resetResult != ESP_OK)
            return NativeErr<Detail::GPIO_CONFIG_FAILED>(resetResult, "ST7735 initial reset GPIO write failed");
    }

    return Ok();
}

ST7735::ResetResult ST7735::hardwareReset() {
    if (!m_config.useResetPin) {
        return Ok();
    }

    esp_err_t result = gpio_set_level(m_config.resetPin, 1);
    if (result != ESP_OK)
        return NativeErr<Detail::RESET_FAILED>(result, "ST7735 reset GPIO could not be driven high");
    result = gpio_set_level(m_config.resetPin, 0);
    if (result != ESP_OK)
        return NativeErr<Detail::RESET_FAILED>(result, "ST7735 reset GPIO could not be driven low");
    vTaskDelay(pdMS_TO_TICKS(20));

    result = gpio_set_level(m_config.resetPin, 1);
    if (result != ESP_OK)
        return NativeErr<Detail::RESET_FAILED>(result, "ST7735 reset GPIO could not be released");
    vTaskDelay(pdMS_TO_TICKS(120));

    return Ok();
}

ST7735::IoResult ST7735::writeCommand(uint8_t command) {
    m_regionBytesRemaining = 0;

    const esp_err_t err = gpio_set_level(m_config.dcPin, 0);
    if (err != ESP_OK) {
        return NativeErr<Detail::GPIO_WRITE_FAILED>(err, "ST7735 data/command GPIO write failed");
    }

    const SPI::TransferResult spiResult = m_spi.write(m_deviceIndex, &command, 1);
    if (spiResult.is_err())
        return spiResult.propagate<Detail::SPI_TRANSFER_FAILED>("ST7735 command transfer failed");

    return Ok();
}

ST7735::IoResult ST7735::writeData(const uint8_t* data, size_t length) {
    if (length == 0) {
        return Ok();
    }
    if (data == nullptr) {
        return Err<Detail::INVALID_BUFFER>("ST7735 transfer buffer is null");
    }

    const esp_err_t err = gpio_set_level(m_config.dcPin, 1);
    if (err != ESP_OK) {
        return NativeErr<Detail::GPIO_WRITE_FAILED>(err, "ST7735 data/command GPIO write failed");
    }

    size_t offset = 0;
    while (offset < length) {
        const size_t              chunk     = std::min(length - offset, DataChunk);
        const SPI::TransferResult spiResult = m_spi.write(m_deviceIndex, data + offset, chunk);
        if (spiResult.is_err())
            return spiResult.propagate<Detail::SPI_TRANSFER_FAILED>("ST7735 data transfer failed");
        offset += chunk;
    }

    return Ok();
}

ST7735::IoResult ST7735::writeCommandData(uint8_t command, const uint8_t* data, size_t length) {
    const IoResult commandResult = writeCommand(command);
    if (commandResult.is_err())
        return commandResult;

    return writeData(data, length);
}

ST7735::IoResult ST7735::setAddressWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    if (!validWindow(x, y, width, height)) {
        return Err<Detail::INVALID_WINDOW>("ST7735 address window is invalid");
    }

    const uint16_t x0 = static_cast<uint16_t>(x + m_config.columnOffset);
    const uint16_t x1 = static_cast<uint16_t>(x + width - 1 + m_config.columnOffset);
    const uint16_t y0 = static_cast<uint16_t>(y + m_config.rowOffset);
    const uint16_t y1 = static_cast<uint16_t>(y + height - 1 + m_config.rowOffset);

    const uint8_t columnData[] {msb(x0), lsb(x0), msb(x1), lsb(x1)};
    const uint8_t rowData[] {msb(y0), lsb(y0), msb(y1), lsb(y1)};

    const IoResult columnResult = writeCommandData(CmdCaseT, columnData, sizeof(columnData));
    if (columnResult.is_err())
        return columnResult;

    return writeCommandData(CmdRaseT, rowData, sizeof(rowData));
}

ST7735::DrawResult ST7735::writeRepeatedColor(uint16_t color, size_t pixels) {
    constexpr size_t StackFallbackBytes = 128;
    uint8_t          stackChunk[StackFallbackBytes] {};
    uint8_t*         chunk      = dmaBufferOr(nullptr);
    size_t           chunkBytes = dmaBufferBytesOr(0);

    if (chunk == nullptr || chunkBytes == 0) {
        chunk      = stackChunk;
        chunkBytes = sizeof(stackChunk);
    }
    chunkBytes &= ~static_cast<size_t>(1U);

    for (size_t i = 0; i < chunkBytes / 2; ++i) {
        chunk[i * 2]     = msb(color);
        chunk[i * 2 + 1] = lsb(color);
    }

    while (pixels > 0) {
        const size_t     chunkPixels = std::min(pixels, chunkBytes / 2);
        const DrawResult result      = writePixels565(chunk, chunkPixels * 2);
        if (result.is_err())
            return result;
        pixels -= chunkPixels;
    }

    return Ok();
}

ST7735::DrawResult ST7735::drawGlyphSolid(char value, uint16_t x, uint16_t y, const TextStyle& style) {
    if (x >= m_config.width || y >= m_config.height) {
        return Ok();
    }

    uint8_t* dmaBuffer = dmaBufferOr(nullptr);
    size_t   dmaBytes  = dmaBufferBytesOr(0);
    if (dmaBuffer == nullptr || dmaBytes < 2) {
        return Err<Detail::DMA_ALLOCATE_FAILED>("ST7735 solid text drawing requires a DMA buffer");
    }

    const uint16_t glyphWidth    = static_cast<uint16_t>(CellWidth * style.scale);
    const uint16_t glyphHeight   = static_cast<uint16_t>(CellHeight * style.scale);
    const uint16_t visibleWidth  = std::min<uint16_t>(glyphWidth, static_cast<uint16_t>(m_config.width - x));
    const uint16_t visibleHeight = std::min<uint16_t>(glyphHeight, static_cast<uint16_t>(m_config.height - y));
    const size_t   bytesPerRow   = static_cast<size_t>(visibleWidth) * 2;
    const uint16_t rowsPerChunk  = static_cast<uint16_t>(std::max<size_t>(1, dmaBytes / bytesPerRow));

    DrawResult drawResult = beginWriteRegion(x, y, visibleWidth, visibleHeight);
    if (drawResult.is_err())
        return drawResult;

    uint16_t row = 0;
    while (row < visibleHeight) {
        const uint16_t rows = std::min<uint16_t>(static_cast<uint16_t>(visibleHeight - row), rowsPerChunk);
        drawResult          = fillGlyphRows(value, x, y, visibleWidth, row, rows, style, dmaBuffer);
        if (drawResult.is_err())
            return drawResult;

        drawResult = writePixels565(dmaBuffer, static_cast<size_t>(rows) * bytesPerRow);
        if (drawResult.is_err())
            return drawResult;

        row = static_cast<uint16_t>(row + rows);
    }

    return Ok();
}

ST7735::DrawResult ST7735::drawGlyphTransparent(char value, uint16_t x, uint16_t y, const TextStyle& style) {
    if (x >= m_config.width || y >= m_config.height) {
        return Ok();
    }

    const uint16_t glyphWidth  = static_cast<uint16_t>(CellWidth * style.scale);
    const uint16_t glyphHeight = static_cast<uint16_t>(CellHeight * style.scale);
    const uint16_t xLimit      = std::min<uint16_t>(glyphWidth, static_cast<uint16_t>(m_config.width - x));
    const uint16_t yLimit      = std::min<uint16_t>(glyphHeight, static_cast<uint16_t>(m_config.height - y));

    for (uint16_t py = 0; py < yLimit; ++py) {
        uint16_t runStart = 0;
        uint16_t runLen   = 0;

        for (uint16_t px = 0; px < xLimit; ++px) {
            const bool on = glyphPixelOn(value, px, py, style.scale);
            if (on) {
                if (runLen == 0) {
                    runStart = px;
                }
                ++runLen;
                continue;
            }

            if (runLen > 0) {
                const DrawResult result = fillRect(static_cast<uint16_t>(x + runStart), static_cast<uint16_t>(y + py), runLen, 1, style.color);
                if (result.is_err())
                    return result;
                runLen = 0;
            }
        }

        if (runLen > 0) {
            const DrawResult result = fillRect(static_cast<uint16_t>(x + runStart), static_cast<uint16_t>(y + py), runLen, 1, style.color);
            if (result.is_err())
                return result;
        }
    }

    return Ok();
}

ST7735::DrawResult ST7735::fillGlyphRows(char value,
                                         uint16_t,
                                         uint16_t,
                                         uint16_t         visibleWidth,
                                         uint16_t         rowOffset,
                                         uint16_t         rows,
                                         const TextStyle& style,
                                         uint8_t*         out) {
    if (out == nullptr || visibleWidth == 0 || rows == 0) {
        return Err<Detail::INVALID_TEXT_STYLE>("ST7735 glyph output buffer or dimensions are invalid");
    }

    size_t offset = 0;
    for (uint16_t row = 0; row < rows; ++row) {
        const uint16_t py = static_cast<uint16_t>(rowOffset + row);
        for (uint16_t px = 0; px < visibleWidth; ++px) {
            const uint16_t color = glyphPixelOn(value, px, py, style.scale) ? style.color : style.background;
            out[offset++]        = msb(color);
            out[offset++]        = lsb(color);
        }
    }

    return Ok();
}

bool ST7735::validWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height) const {
    return width > 0 &&
           height > 0 &&
           x < m_config.width &&
           y < m_config.height &&
           width <= m_config.width - x &&
           height <= m_config.height - y;
}

bool ST7735::validTextStyle(const TextStyle& style) const {
    return style.scale > 0 &&
           style.x < m_config.width &&
           style.y < m_config.height;
}

uint8_t* ST7735::dmaBufferOr(uint8_t* buffer) const {
    return buffer != nullptr ? buffer : m_dmaBuffer;
}

size_t ST7735::dmaBufferBytesOr(size_t bytes) const {
    return bytes != 0 ? bytes : m_dmaBufferSize;
}

bool ST7735::validFrameBuffer(const FrameBuffer& frame) {
    return frame.data != nullptr &&
           frame.width > 0 &&
           frame.height > 0 &&
           frame.stride >= static_cast<size_t>(frame.width) * 2;
}

uint8_t* ST7735::framePixel(FrameBuffer& frame, uint16_t x, uint16_t y) {
    if (!validFrameBuffer(frame) || x >= frame.width || y >= frame.height) {
        return nullptr;
    }

    return frame.data + static_cast<size_t>(y) * frame.stride + static_cast<size_t>(x) * 2;
}

void ST7735::writeFramePixel(FrameBuffer& frame, uint16_t x, uint16_t y, uint16_t color) {
    uint8_t* pixel = framePixel(frame, x, y);
    if (pixel == nullptr) {
        return;
    }

    pixel[0] = msb(color);
    pixel[1] = lsb(color);
}

bool ST7735::validAnimation(const AnimationView& animation) {
    const size_t minStride = frameSize(animation.width, animation.height);
    return animation.frames != nullptr &&
           animation.width > 0 &&
           animation.height > 0 &&
           animation.frameCount > 0 &&
           minStride > 0 &&
           animation.frameStride >= minStride;
}

bool ST7735::glyphPixelOn(char value, uint16_t glyphX, uint16_t glyphY, uint8_t scale) {
    if (scale == 0 || glyphX >= CellWidth * scale || glyphY >= CellHeight * scale) {
        return false;
    }

    const uint16_t fontX = static_cast<uint16_t>(glyphX / scale);
    const uint16_t fontY = static_cast<uint16_t>(glyphY / scale);
    if (fontX >= FontWidth || fontY >= FontHeight) {
        return false;
    }

    return (glyph(value)[fontX] & (1U << fontY)) != 0;
}

size_t ST7735::frameSize(uint16_t width, uint16_t height) {
    return static_cast<size_t>(width) * height * 2;
}

size_t ST7735::textLength(const char* text) {
    return text == nullptr ? 0 : std::strlen(text);
}

const uint8_t* ST7735::glyph(char value) {
    if (value < ' ' || value > '~') {
        value = '?';
    }

    return Font5x7[value - ' '];
}

uint8_t ST7735::msb(uint16_t value) {
    return static_cast<uint8_t>(value >> 8U);
}

uint8_t ST7735::lsb(uint16_t value) {
    return static_cast<uint8_t>(value & 0xFFU);
}
