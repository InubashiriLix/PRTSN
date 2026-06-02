#include "src/dvc/inc/ST7735.h"

#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <algorithm>
#include <cstring>

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
      m_config(config) {}

ST7735::~ST7735() {
    releaseDmaBuffer();
}

ST7735::Error ST7735::setup() {
    if (m_started) {
        return makeError(StdError::INVALID_STATE, Detail::ALREADY_STARTED, ESP_ERR_INVALID_STATE);
    }

    if (m_config.autoDmaBuffer && m_dmaBuffer == nullptr) {
        Error dmaErr = allocateDmaBuffer(m_config.dmaBufferBytes);
        if (!dmaErr) {
            return dmaErr;
        }
    }

    Error err = configurePins();
    if (!err) {
        return err;
    }

    err = hardwareReset();
    if (!err) {
        return err;
    }

    err = writeCommand(CmdSwReset);
    if (!err) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(150));

    err = writeCommand(CmdSlpOut);
    if (!err) {
        return err;
    }
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

    err = writeCommandData(0xB1, frmCtr, sizeof(frmCtr));
    if (!err) {
        return err;
    }
    err = writeCommandData(0xB2, frmCtr, sizeof(frmCtr));
    if (!err) {
        return err;
    }
    err = writeCommandData(0xB3, frmCtr3, sizeof(frmCtr3));
    if (!err) {
        return err;
    }
    err = writeCommandData(0xB4, invCtr, sizeof(invCtr));
    if (!err) {
        return err;
    }
    err = writeCommandData(0xC0, pwCtr1, sizeof(pwCtr1));
    if (!err) {
        return err;
    }
    err = writeCommandData(0xC1, pwCtr2, sizeof(pwCtr2));
    if (!err) {
        return err;
    }
    err = writeCommandData(0xC2, pwCtr3, sizeof(pwCtr3));
    if (!err) {
        return err;
    }
    err = writeCommandData(0xC3, pwCtr4, sizeof(pwCtr4));
    if (!err) {
        return err;
    }
    err = writeCommandData(0xC4, pwCtr5, sizeof(pwCtr5));
    if (!err) {
        return err;
    }
    err = writeCommandData(0xC5, vmCtr1, sizeof(vmCtr1));
    if (!err) {
        return err;
    }

    const uint8_t colorMode = 0x05;
    err                     = writeCommandData(CmdColMod, &colorMode, 1);
    if (!err) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    err = writeCommandData(CmdMadCtl, &m_config.madctl, 1);
    if (!err) {
        return err;
    }

    err = writeCommand(m_config.invertColors ? CmdInvOn : CmdInvOff);
    if (!err) {
        return err;
    }

    err = writeCommandData(0xE0, gammaP, sizeof(gammaP));
    if (!err) {
        return err;
    }
    err = writeCommandData(0xE1, gammaN, sizeof(gammaN));
    if (!err) {
        return err;
    }

    err = writeCommand(CmdNorOn);
    if (!err) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    err = writeCommand(CmdDispOn);
    if (!err) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(120));

    m_started = true;

    if (m_config.useBacklightPin) {
        return setBacklight(true);
    }

    return clearError();
}

void ST7735::releaseDmaBuffer() {
    if (m_ownsDmaBuffer && m_dmaBuffer != nullptr) {
        heap_caps_free(m_dmaBuffer);
    }

    m_dmaBuffer     = nullptr;
    m_dmaBufferSize = 0;
    m_ownsDmaBuffer = false;
}

ST7735::Error ST7735::allocateDmaBuffer(size_t bytes) {
    if (bytes == 0) {
        return makeError(StdError::INVALID_SIZE, Detail::INVALID_PARAM, ESP_ERR_INVALID_SIZE);
    }

    releaseDmaBuffer();

    const size_t alignedBytes = (bytes + 3U) & ~static_cast<size_t>(3U);
    m_dmaBuffer               = static_cast<uint8_t*>(heap_caps_malloc(alignedBytes, MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
    if (m_dmaBuffer == nullptr) {
        return makeError(StdError::NO_MEM, Detail::DMA_ALLOC_FAILED, ESP_ERR_NO_MEM);
    }

    m_dmaBufferSize = alignedBytes;
    m_ownsDmaBuffer = true;
    return clearError();
}

ST7735::Error ST7735::setDmaBuffer(uint8_t* buffer, size_t bytes) {
    if (buffer == nullptr || bytes == 0) {
        return makeError(StdError::INVALID_ARGS, Detail::INVALID_PARAM, ESP_ERR_INVALID_ARG);
    }

    releaseDmaBuffer();
    m_dmaBuffer     = buffer;
    m_dmaBufferSize = bytes & ~static_cast<size_t>(1U);
    m_ownsDmaBuffer = false;

    if (m_dmaBufferSize == 0) {
        return makeError(StdError::INVALID_SIZE, Detail::INVALID_PARAM, ESP_ERR_INVALID_SIZE);
    }

    return clearError();
}

ST7735::Error ST7735::setBacklight(bool enabled) {
    if (!m_config.useBacklightPin) {
        return clearError();
    }

    const esp_err_t err = gpio_set_level(m_config.backlightPin, enabled ? 1 : 0);
    if (err != ESP_OK) {
        return makeError(toStdErr(err), Detail::GPIO_WRITE_FAILED, err);
    }

    return clearError();
}

ST7735::Error ST7735::fillScreen(uint16_t color) {
    return fillRect(0, 0, m_config.width, m_config.height, color);
}

ST7735::Error ST7735::fillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
    if (!m_started) {
        return makeError(StdError::INVALID_STATE, Detail::NOT_STARTED, ESP_ERR_INVALID_STATE);
    }
    if (!validWindow(x, y, width, height)) {
        return makeError(StdError::INVALID_ARGS, Detail::INVALID_WINDOW, ESP_ERR_INVALID_ARG);
    }

    Error err = beginWriteRegion(x, y, width, height);
    if (!err) {
        return err;
    }

    return writeRepeatedColor(color, static_cast<size_t>(width) * height);
}

ST7735::Error ST7735::drawPixel(uint16_t x, uint16_t y, uint16_t color) {
    const uint8_t data[] {msb(color), lsb(color)};
    return pushPixels565(x, y, 1, 1, data);
}

ST7735::Error ST7735::drawChar(char value, uint16_t x, uint16_t y, uint16_t color) {
    return drawChar(value, TextStyle {.x = x, .y = y, .color = color});
}

ST7735::Error ST7735::drawChar(char value, const TextStyle& style) {
    if (!m_started) {
        return makeError(StdError::INVALID_STATE, Detail::NOT_STARTED, ESP_ERR_INVALID_STATE);
    }
    if (!validTextStyle(style)) {
        return makeError(StdError::INVALID_ARGS, Detail::TEXT_FAILED, ESP_ERR_INVALID_ARG);
    }

    if (style.transparent) {
        return drawGlyphTransparent(value, style.x, style.y, style);
    }

    return drawGlyphSolid(value, style.x, style.y, style);
}

ST7735::Error ST7735::drawText(const char* text, uint16_t x, uint16_t y, uint16_t color) {
    return drawText(text, TextStyle {.x = x, .y = y, .color = color});
}

ST7735::Error ST7735::drawText(const char* text, const TextStyle& style) {
    if (!m_started) {
        return makeError(StdError::INVALID_STATE, Detail::NOT_STARTED, ESP_ERR_INVALID_STATE);
    }
    if (text == nullptr || !validTextStyle(style)) {
        return makeError(StdError::INVALID_ARGS, Detail::TEXT_FAILED, ESP_ERR_INVALID_ARG);
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

        Error err = drawChar(value, glyphStyle);
        if (!err) {
            return err;
        }

        cursorX = static_cast<uint16_t>(cursorX + cellWidth);
        if (!style.wrap && cursorX >= m_config.width) {
            break;
        }
    }

    return clearError();
}

ST7735::Error ST7735::drawFrameBuffer(const FrameBuffer& frame, uint16_t x, uint16_t y, uint8_t* dmaBuffer, size_t dmaBufferBytes) {
    if (!m_started) {
        return makeError(StdError::INVALID_STATE, Detail::NOT_STARTED, ESP_ERR_INVALID_STATE);
    }
    if (!validFrameBuffer(frame) || !validWindow(x, y, frame.width, frame.height)) {
        return makeError(StdError::INVALID_ARGS, Detail::INVALID_PARAM, ESP_ERR_INVALID_ARG);
    }
    if (frame.stride != static_cast<size_t>(frame.width) * 2) {
        return makeError(StdError::INVALID_ARGS, Detail::INVALID_PARAM, ESP_ERR_INVALID_ARG);
    }

    return drawFrame565(x, y, frame.width, frame.height, frame.data, dmaBuffer, dmaBufferBytes);
}

ST7735::Error ST7735::pushPixels565(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* rgb565Be) {
    if (!m_started) {
        return makeError(StdError::INVALID_STATE, Detail::NOT_STARTED, ESP_ERR_INVALID_STATE);
    }
    if (!validWindow(x, y, width, height)) {
        return makeError(StdError::INVALID_ARGS, Detail::INVALID_WINDOW, ESP_ERR_INVALID_ARG);
    }
    if (rgb565Be == nullptr) {
        return makeError(StdError::INVALID_ARGS, Detail::DATA_FAILED, ESP_ERR_INVALID_ARG);
    }

    Error err = beginWriteRegion(x, y, width, height);
    if (!err) {
        return err;
    }

    return writePixels565(rgb565Be, frameSize(width, height));
}

ST7735::Error ST7735::beginWriteRegion(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    if (!m_started) {
        return makeError(StdError::INVALID_STATE, Detail::NOT_STARTED, ESP_ERR_INVALID_STATE);
    }
    if (!validWindow(x, y, width, height)) {
        return makeError(StdError::INVALID_ARGS, Detail::INVALID_WINDOW, ESP_ERR_INVALID_ARG);
    }

    Error err = setAddressWindow(x, y, width, height);
    if (!err) {
        return err;
    }

    err = writeCommand(CmdRamWr);
    if (!err) {
        m_regionOpen = false;
        return err;
    }

    m_regionOpen = true;
    return clearError();
}

ST7735::Error ST7735::writePixels565(const uint8_t* rgb565Be, size_t length) {
    if (!m_started || !m_regionOpen) {
        return makeError(StdError::INVALID_STATE, Detail::NOT_STARTED, ESP_ERR_INVALID_STATE);
    }

    return writeData(rgb565Be, length);
}

ST7735::Error ST7735::writeColor(uint16_t color, size_t pixels) {
    if (!m_started || !m_regionOpen) {
        return makeError(StdError::INVALID_STATE, Detail::NOT_STARTED, ESP_ERR_INVALID_STATE);
    }

    return writeRepeatedColor(color, pixels);
}

ST7735::Error ST7735::drawFrame565(uint16_t       x,
                                   uint16_t       y,
                                   uint16_t       width,
                                   uint16_t       height,
                                   const uint8_t* rgb565Be,
                                   uint8_t*       dmaBuffer,
                                   size_t         dmaBufferBytes) {
    if (!m_started) {
        return makeError(StdError::INVALID_STATE, Detail::NOT_STARTED, ESP_ERR_INVALID_STATE);
    }
    if (!validWindow(x, y, width, height)) {
        return makeError(StdError::INVALID_ARGS, Detail::INVALID_WINDOW, ESP_ERR_INVALID_ARG);
    }
    if (rgb565Be == nullptr) {
        return makeError(StdError::INVALID_ARGS, Detail::DATA_FAILED, ESP_ERR_INVALID_ARG);
    }

    Error err = beginWriteRegion(x, y, width, height);
    if (!err) {
        return err;
    }

    const size_t totalBytes = frameSize(width, height);
    dmaBuffer               = dmaBufferOr(dmaBuffer);
    dmaBufferBytes          = dmaBufferBytesOr(dmaBufferBytes);
    if (dmaBuffer == nullptr || dmaBufferBytes == 0) {
        return writePixels565(rgb565Be, totalBytes);
    }

    const size_t chunkBytes = dmaBufferBytes & ~static_cast<size_t>(1U);
    if (chunkBytes == 0) {
        return makeError(StdError::INVALID_SIZE, Detail::INVALID_PARAM, ESP_ERR_INVALID_SIZE);
    }

    size_t offset = 0;
    while (offset < totalBytes) {
        const size_t count = std::min(totalBytes - offset, chunkBytes);
        std::memcpy(dmaBuffer, rgb565Be + offset, count);

        err = writePixels565(dmaBuffer, count);
        if (!err) {
            return err;
        }

        offset += count;
    }

    return clearError();
}

ST7735::Error ST7735::drawAnimationFrame(const AnimationView& animation,
                                         uint16_t             frameIndex,
                                         uint16_t             x,
                                         uint16_t             y,
                                         uint8_t*             dmaBuffer,
                                         size_t               dmaBufferBytes) {
    if (!validAnimation(animation)) {
        return makeError(StdError::INVALID_ARGS, Detail::INVALID_PARAM, ESP_ERR_INVALID_ARG);
    }

    const uint16_t safeIndex = static_cast<uint16_t>(frameIndex % animation.frameCount);
    const uint8_t* frame     = animation.frames + static_cast<size_t>(safeIndex) * animation.frameStride;
    return drawFrame565(x, y, animation.width, animation.height, frame, dmaBuffer, dmaBufferBytes);
}

ST7735::Error ST7735::drawAnimationFrameLocked(const AnimationView& animation,
                                               AnimationPlayer&     player,
                                               uint16_t             x,
                                               uint16_t             y,
                                               uint8_t*             dmaBuffer,
                                               size_t               dmaBufferBytes) {
    if (!validAnimation(animation)) {
        return makeError(StdError::INVALID_ARGS, Detail::INVALID_PARAM, ESP_ERR_INVALID_ARG);
    }
    if (animation.frameDurationsMs == nullptr && (player.fallbackFps < MinFps || player.fallbackFps > MaxFps)) {
        return makeError(StdError::INVALID_ARGS, Detail::INVALID_PARAM, ESP_ERR_INVALID_ARG);
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

    Error err = drawAnimationFrame(animation, currentFrame, x, y, dmaBuffer, dmaBufferBytes);
    if (!err) {
        return err;
    }

    const uint16_t durationMs      = animation.frameDurationsMs != nullptr ? std::max<uint16_t>(animation.frameDurationsMs[currentFrame], 1) : static_cast<uint16_t>(std::max<int64_t>(1000LL / player.fallbackFps, 1));
    const int64_t  frameIntervalUs = static_cast<int64_t>(durationMs) * 1000LL;

    player.frameIndex = static_cast<uint16_t>((player.frameIndex + 1U) % animation.frameCount);
    player.nextFrameUs += frameIntervalUs;

    nowUs = esp_timer_get_time();
    if (player.nextFrameUs < nowUs - frameIntervalUs) {
        player.nextFrameUs = nowUs + frameIntervalUs;
    }

    return clearError();
}

bool ST7735::started() const {
    return m_started;
}

ST7735::Error ST7735::lastError() const {
    return m_lastError;
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

bool ST7735::clearFrame(FrameBuffer& frame, uint16_t color) {
    if (!validFrameBuffer(frame)) {
        return false;
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

    return true;
}

bool ST7735::copyFrame(FrameBuffer& frame, const uint8_t* rgb565Be, uint16_t width, uint16_t height, size_t stride) {
    if (!validFrameBuffer(frame) || rgb565Be == nullptr || width == 0 || height == 0 || stride < static_cast<size_t>(width) * 2) {
        return false;
    }
    if (width > frame.width || height > frame.height) {
        return false;
    }

    const size_t rowBytes = static_cast<size_t>(width) * 2;
    for (uint16_t y = 0; y < height; ++y) {
        std::memcpy(frame.data + static_cast<size_t>(y) * frame.stride, rgb565Be + static_cast<size_t>(y) * stride, rowBytes);
    }

    return true;
}

bool ST7735::copyAnimationFrame(FrameBuffer& frame, const AnimationView& animation, uint16_t frameIndex) {
    if (!validAnimation(animation) || !validFrameBuffer(frame)) {
        return false;
    }
    if (animation.width > frame.width || animation.height > frame.height) {
        return false;
    }

    const uint16_t safeIndex = static_cast<uint16_t>(frameIndex % animation.frameCount);
    const uint8_t* source    = animation.frames + static_cast<size_t>(safeIndex) * animation.frameStride;
    return copyFrame(frame, source, animation.width, animation.height, static_cast<size_t>(animation.width) * 2);
}

bool ST7735::fillFrameRect(FrameBuffer& frame, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
    if (!validFrameBuffer(frame) || width == 0 || height == 0 || x >= frame.width || y >= frame.height) {
        return false;
    }

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

    return true;
}

bool ST7735::drawFrameChar(FrameBuffer& frame, char value, const TextStyle& style) {
    if (!validFrameBuffer(frame) || style.scale == 0 || style.x >= frame.width || style.y >= frame.height) {
        return false;
    }

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

    return true;
}

bool ST7735::drawFrameText(FrameBuffer& frame, const char* text, const TextStyle& style) {
    if (!validFrameBuffer(frame) || text == nullptr || style.scale == 0 || style.x >= frame.width || style.y >= frame.height) {
        return false;
    }

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

        TextStyle glyphStyle = style;
        glyphStyle.x         = cursorX;
        glyphStyle.y         = cursorY;
        if (!drawFrameChar(frame, value, glyphStyle)) {
            return false;
        }

        cursorX = static_cast<uint16_t>(cursorX + cellWidth);
        if (!style.wrap && cursorX >= frame.width) {
            break;
        }
    }

    return true;
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

const char* ST7735::detailName(Detail detail) noexcept {
    switch (detail) {
        case Detail::NONE:
            return "NONE";
        case Detail::NOT_STARTED:
            return "NOT_STARTED";
        case Detail::ALREADY_STARTED:
            return "ALREADY_STARTED";
        case Detail::INVALID_PARAM:
            return "INVALID_PARAM";
        case Detail::INVALID_WINDOW:
            return "INVALID_WINDOW";
        case Detail::GPIO_CONFIG_FAILED:
            return "GPIO_CONFIG_FAILED";
        case Detail::GPIO_WRITE_FAILED:
            return "GPIO_WRITE_FAILED";
        case Detail::RESET_FAILED:
            return "RESET_FAILED";
        case Detail::DMA_ALLOC_FAILED:
            return "DMA_ALLOC_FAILED";
        case Detail::COMMAND_FAILED:
            return "COMMAND_FAILED";
        case Detail::DATA_FAILED:
            return "DATA_FAILED";
        case Detail::TEXT_FAILED:
            return "TEXT_FAILED";
        case Detail::SPI_FAILED:
            return "SPI_FAILED";
    }

    return "UNKNOWN";
}

ST7735::Error ST7735::configurePins() {
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
        return makeError(toStdErr(err), Detail::GPIO_CONFIG_FAILED, err);
    }

    if (m_config.useBacklightPin) {
        gpio_set_level(m_config.backlightPin, 0);
    }
    gpio_set_level(m_config.dcPin, 1);
    if (m_config.useResetPin) {
        gpio_set_level(m_config.resetPin, 1);
    }

    return clearError();
}

ST7735::Error ST7735::hardwareReset() {
    if (!m_config.useResetPin) {
        return clearError();
    }

    if (gpio_set_level(m_config.resetPin, 1) != ESP_OK ||
        gpio_set_level(m_config.resetPin, 0) != ESP_OK) {
        return makeError(StdError::FAIL, Detail::RESET_FAILED, ESP_FAIL);
    }
    vTaskDelay(pdMS_TO_TICKS(20));

    if (gpio_set_level(m_config.resetPin, 1) != ESP_OK) {
        return makeError(StdError::FAIL, Detail::RESET_FAILED, ESP_FAIL);
    }
    vTaskDelay(pdMS_TO_TICKS(120));

    return clearError();
}

ST7735::Error ST7735::writeCommand(uint8_t command) {
    m_regionOpen = false;

    const esp_err_t err = gpio_set_level(m_config.dcPin, 0);
    if (err != ESP_OK) {
        return makeError(toStdErr(err), Detail::GPIO_WRITE_FAILED, err);
    }

    SPI::Error spiErr = m_spi.write(m_deviceIndex, &command, 1);
    if (!spiErr) {
        return mapSpiError(spiErr, Detail::COMMAND_FAILED);
    }

    return clearError();
}

ST7735::Error ST7735::writeData(const uint8_t* data, size_t length) {
    if (length == 0) {
        return clearError();
    }
    if (data == nullptr) {
        return makeError(StdError::INVALID_ARGS, Detail::DATA_FAILED, ESP_ERR_INVALID_ARG);
    }

    const esp_err_t err = gpio_set_level(m_config.dcPin, 1);
    if (err != ESP_OK) {
        return makeError(toStdErr(err), Detail::GPIO_WRITE_FAILED, err);
    }

    size_t offset = 0;
    while (offset < length) {
        const size_t chunk  = std::min(length - offset, DataChunk);
        SPI::Error   spiErr = m_spi.write(m_deviceIndex, data + offset, chunk);
        if (!spiErr) {
            return mapSpiError(spiErr, Detail::DATA_FAILED);
        }
        offset += chunk;
    }

    return clearError();
}

ST7735::Error ST7735::writeCommandData(uint8_t command, const uint8_t* data, size_t length) {
    Error err = writeCommand(command);
    if (!err) {
        return err;
    }

    return writeData(data, length);
}

ST7735::Error ST7735::setAddressWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    if (!validWindow(x, y, width, height)) {
        return makeError(StdError::INVALID_ARGS, Detail::INVALID_WINDOW, ESP_ERR_INVALID_ARG);
    }

    const uint16_t x0 = static_cast<uint16_t>(x + m_config.columnOffset);
    const uint16_t x1 = static_cast<uint16_t>(x + width - 1 + m_config.columnOffset);
    const uint16_t y0 = static_cast<uint16_t>(y + m_config.rowOffset);
    const uint16_t y1 = static_cast<uint16_t>(y + height - 1 + m_config.rowOffset);

    const uint8_t columnData[] {msb(x0), lsb(x0), msb(x1), lsb(x1)};
    const uint8_t rowData[] {msb(y0), lsb(y0), msb(y1), lsb(y1)};

    Error err = writeCommandData(CmdCaseT, columnData, sizeof(columnData));
    if (!err) {
        return err;
    }

    return writeCommandData(CmdRaseT, rowData, sizeof(rowData));
}

ST7735::Error ST7735::writeRepeatedColor(uint16_t color, size_t pixels) {
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
        const size_t chunkPixels = std::min(pixels, chunkBytes / 2);
        Error        err         = writeData(chunk, chunkPixels * 2);
        if (!err) {
            return err;
        }
        pixels -= chunkPixels;
    }

    return clearError();
}

ST7735::Error ST7735::drawGlyphSolid(char value, uint16_t x, uint16_t y, const TextStyle& style) {
    if (x >= m_config.width || y >= m_config.height) {
        return clearError();
    }

    uint8_t* dmaBuffer = dmaBufferOr(nullptr);
    size_t   dmaBytes  = dmaBufferBytesOr(0);
    if (dmaBuffer == nullptr || dmaBytes < 2) {
        return makeError(StdError::INVALID_STATE, Detail::DMA_ALLOC_FAILED, ESP_ERR_INVALID_STATE);
    }

    const uint16_t glyphWidth    = static_cast<uint16_t>(CellWidth * style.scale);
    const uint16_t glyphHeight   = static_cast<uint16_t>(CellHeight * style.scale);
    const uint16_t visibleWidth  = std::min<uint16_t>(glyphWidth, static_cast<uint16_t>(m_config.width - x));
    const uint16_t visibleHeight = std::min<uint16_t>(glyphHeight, static_cast<uint16_t>(m_config.height - y));
    const size_t   bytesPerRow   = static_cast<size_t>(visibleWidth) * 2;
    const uint16_t rowsPerChunk  = static_cast<uint16_t>(std::max<size_t>(1, dmaBytes / bytesPerRow));

    Error err = beginWriteRegion(x, y, visibleWidth, visibleHeight);
    if (!err) {
        return err;
    }

    uint16_t row = 0;
    while (row < visibleHeight) {
        const uint16_t rows = std::min<uint16_t>(static_cast<uint16_t>(visibleHeight - row), rowsPerChunk);
        err                 = fillGlyphRows(value, x, y, visibleWidth, row, rows, style, dmaBuffer);
        if (!err) {
            return err;
        }

        err = writePixels565(dmaBuffer, static_cast<size_t>(rows) * bytesPerRow);
        if (!err) {
            return err;
        }

        row = static_cast<uint16_t>(row + rows);
    }

    return clearError();
}

ST7735::Error ST7735::drawGlyphTransparent(char value, uint16_t x, uint16_t y, const TextStyle& style) {
    if (x >= m_config.width || y >= m_config.height) {
        return clearError();
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
                Error err = fillRect(static_cast<uint16_t>(x + runStart), static_cast<uint16_t>(y + py), runLen, 1, style.color);
                if (!err) {
                    return err;
                }
                runLen = 0;
            }
        }

        if (runLen > 0) {
            Error err = fillRect(static_cast<uint16_t>(x + runStart), static_cast<uint16_t>(y + py), runLen, 1, style.color);
            if (!err) {
                return err;
            }
        }
    }

    return clearError();
}

ST7735::Error ST7735::fillGlyphRows(char value,
                                    uint16_t,
                                    uint16_t,
                                    uint16_t         visibleWidth,
                                    uint16_t         rowOffset,
                                    uint16_t         rows,
                                    const TextStyle& style,
                                    uint8_t*         out) {
    if (out == nullptr || visibleWidth == 0 || rows == 0) {
        return makeError(StdError::INVALID_ARGS, Detail::TEXT_FAILED, ESP_ERR_INVALID_ARG);
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

    return clearError();
}

ST7735::Error ST7735::makeError(StdError code, Detail detail, esp_err_t native, SPI::Error spi) {
    m_lastError = Error {.code = code, .detail = detail, .spi = spi, .native = native};
    return m_lastError;
}

ST7735::Error ST7735::mapSpiError(SPI::Error spi, Detail detail) {
    return makeError(spi.code, detail, spi.native, spi);
}

ST7735::Error ST7735::clearError() {
    m_lastError = Error {};
    return m_lastError;
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
