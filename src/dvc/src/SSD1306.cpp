#include "src/dvc/inc/SSD1306.h"

#include <cstring>

namespace
{
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
}

SSD1306::SSD1306(IIC& iic, uint8_t address)
    : m_iic(&iic),
      m_ownsIic(false),
      m_address(address) {}

SSD1306::SSD1306(uint8_t sdaPin, uint8_t sclPin, uint32_t frequency, uint8_t address)
    : m_ownedIic(sdaPin, sclPin, frequency),
      m_iic(&m_ownedIic),
      m_ownsIic(true),
      m_address(address) {}

bool SSD1306::begin() {
    if (m_iic == nullptr || !m_iic->begin() || !m_iic->devicePresent(m_address)) {
        m_started = false;
        return false;
    }

    static constexpr uint8_t init[] = {
        0xAE,       // display off
        0xD5, 0x80, // clock divide
        0xA8, 0x3F, // multiplex for 64 rows
        0xD3, 0x00, // display offset
        0x40,       // start line 0
        0x8D, 0x14, // charge pump on
        0x20, 0x00, // horizontal addressing
        0xA1,       // segment remap
        0xC8,       // COM scan descending
        0xDA, 0x12, // COM pins for 128x64 panel
        0x81, 0xCF, // contrast
        0xD9, 0xF1, // pre-charge
        0xDB, 0x40, // VCOMH deselect
        0xA4,       // resume RAM display
        0xA6,       // normal display
        0x2E,       // stop scroll
        0xAF,       // display on
    };

    if (!command(init, sizeof(init))) {
        m_started = false;
        return false;
    }

    m_started = true;
    if (m_inverted && !setInverted(true)) {
        m_started = false;
        return false;
    }

    return clear(true);
}

void SSD1306::end() {
    if (m_started) {
        static constexpr uint8_t off[] = {0xAE};
        command(off, sizeof(off));
    }

    if (m_ownsIic && m_iic != nullptr) {
        m_iic->end();
    }

    m_started = false;
}

bool SSD1306::clear(bool flush) {
    std::memset(m_buffer, 0, sizeof(m_buffer));
    return !flush || display();
}

bool SSD1306::setTextScale(uint8_t scale) {
    if (scale == 0 || scale > Height / CellHeight) {
        return false;
    }

    m_textScale = scale;
    return true;
}

uint8_t SSD1306::textScale() const {
    return m_textScale;
}

bool SSD1306::setInverted(bool inverted) {
    m_inverted = inverted;

    if (!m_started) {
        return true;
    }

    const uint8_t invertCommand = inverted ? 0xA7 : 0xA6;
    return command(&invertCommand, 1);
}

bool SSD1306::inverted() const {
    return m_inverted;
}

bool SSD1306::display() {
    if (!m_started) {
        return false;
    }

    static constexpr uint8_t window[] = {
        0x21, 0x00, Width - 1,
        0x22, 0x00, Pages - 1,
    };

    return command(window, sizeof(window)) && data(m_buffer, sizeof(m_buffer));
}

bool SSD1306::setPixel(uint8_t x, uint8_t y, Color color) {
    if (x >= Width || y >= Height) {
        return false;
    }

    const size_t  index = x + (y / 8) * Width;
    const uint8_t mask  = static_cast<uint8_t>(1U << (y & 0x07));

    switch (color) {
        case Color::WHITE:
            m_buffer[index] |= mask;
            break;
        case Color::BLACK:
            m_buffer[index] &= static_cast<uint8_t>(~mask);
            break;
        case Color::INVERT:
            m_buffer[index] ^= mask;
            break;
    }

    return true;
}

bool SSD1306::drawText(const char* text, uint8_t column, uint8_t line, Color color) {
    if (m_textScale == 0 || column >= Width / (CellWidth * m_textScale) ||
        line >= Height / (CellHeight * m_textScale)) {
        return false;
    }

    TextStyle style {
        static_cast<uint8_t>(column * CellWidth * m_textScale),
        static_cast<uint8_t>(line * CellHeight * m_textScale),
        m_textScale,
        color,
        Color::BLACK,
        false,
    };

    return drawText(text, style);
}

bool SSD1306::drawText(const char* text, const TextStyle& style) {
    if (text == nullptr || !validTextStyle(style)) {
        return false;
    }

    uint8_t x = style.x;

    while (*text != '\0' && x < Width) {
        const uint8_t* bitmap = glyph(*text++);

        for (uint8_t glyphX = 0; glyphX < CellWidth && x < Width; ++glyphX) {
            const uint8_t columnBits = glyphX < FontWidth ? bitmap[glyphX] : 0;

            for (uint8_t glyphY = 0; glyphY < CellHeight; ++glyphY) {
                const bool on = glyphY < FontHeight && (columnBits & (1U << glyphY)) != 0;

                for (uint8_t sx = 0; sx < style.scale; ++sx) {
                    for (uint8_t sy = 0; sy < style.scale; ++sy) {
                        const uint8_t px = static_cast<uint8_t>(x + sx);
                        const uint8_t py = static_cast<uint8_t>(style.y + glyphY * style.scale + sy);

                        if (px >= Width || py >= Height) {
                            continue;
                        }

                        drawGlyphPixel(px, py, on, style);
                    }
                }
            }

            x = static_cast<uint8_t>(x + style.scale);
        }
    }

    return true;
}

bool SSD1306::displayText(const char* text) {
    return clear(false) && drawText(text, 0, 0) && display();
}

bool SSD1306::displayText(const char* text, uint8_t line) {
    return displayText(text, 0, line);
}

bool SSD1306::displayText(const char* text, uint8_t column, uint8_t line) {
    if (m_textScale == 0 || line >= Height / (CellHeight * m_textScale) ||
        column >= Width / (CellWidth * m_textScale)) {
        return false;
    }

    TextStyle style {
        static_cast<uint8_t>(column * CellWidth * m_textScale),
        static_cast<uint8_t>(line * CellHeight * m_textScale),
        m_textScale,
        Color::WHITE,
        Color::BLACK,
        false,
    };

    const uint16_t cellHeight = static_cast<uint16_t>(CellHeight * style.scale);
    const uint16_t yEnd       = static_cast<uint16_t>(style.y + cellHeight);

    for (uint16_t y = style.y; y < yEnd && y < Height; ++y) {
        std::memset(&m_buffer[(y / 8) * Width], 0, Width);
    }

    return drawText(text, style) && display();
}

bool SSD1306::displayText(const char* text, const TextStyle& style) {
    return drawText(text, style) && display();
}

bool SSD1306::command(const uint8_t* bytes, size_t length) {
    if (m_iic == nullptr || bytes == nullptr || length == 0) {
        return false;
    }

    while (length > 0) {
        uint8_t packet[17] = {CommandControlByte};
        const size_t chunk = length > sizeof(packet) - 1 ? sizeof(packet) - 1 : length;

        std::memcpy(&packet[1], bytes, chunk);
        if (!m_iic->write(m_address, packet, chunk + 1)) {
            return false;
        }

        bytes += chunk;
        length -= chunk;
    }

    return true;
}

bool SSD1306::data(const uint8_t* bytes, size_t length) {
    if (m_iic == nullptr || bytes == nullptr || length == 0) {
        return false;
    }

    while (length > 0) {
        uint8_t packet[17] = {DataControlByte};
        const size_t chunk = length > sizeof(packet) - 1 ? sizeof(packet) - 1 : length;

        std::memcpy(&packet[1], bytes, chunk);
        if (!m_iic->write(m_address, packet, chunk + 1)) {
            return false;
        }

        bytes += chunk;
        length -= chunk;
    }

    return true;
}

bool SSD1306::validTextStyle(const TextStyle& style) const {
    return style.scale > 0 &&
           style.scale <= Height / CellHeight &&
           style.x < Width &&
           style.y < Height;
}

void SSD1306::drawGlyphPixel(uint8_t x, uint8_t y, bool on, const TextStyle& style) {
    Color foreground = style.color;
    Color background = style.background;

    if (style.inverted) {
        const Color tmp = foreground;
        foreground = background;
        background = tmp;
    }

    const Color target = on ? foreground : background;
    if (target == Color::INVERT) {
        setPixel(x, y, Color::INVERT);
    } else {
        setPixel(x, y, target);
    }
}

const uint8_t* SSD1306::glyph(char value) {
    if (value < ' ' || value > '~') {
        value = '?';
    }

    return Font5x7[value - ' '];
}
