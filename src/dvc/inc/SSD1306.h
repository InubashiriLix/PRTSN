#pragma once

#include "src/fw/inc/IIC.h"

#include <cstddef>
#include <cstdint>

class SSD1306
{
public:
    static constexpr uint8_t Width          = 128;
    static constexpr uint8_t Height         = 64;
    static constexpr uint8_t Pages          = Height / 8;
    static constexpr uint8_t DefaultAddress = 0x3C;

    enum class Color : uint8_t
    {
        BLACK = 0,
        WHITE,
        INVERT,
    };

    enum class ScrollDirection : uint8_t
    {
        RIGHT = 0x26,
        LEFT  = 0x27,
    };

    enum class ScrollInterval : uint8_t
    {
        FRAME_5   = 0x00,
        FRAME_64  = 0x01,
        FRAME_128 = 0x02,
        FRAME_256 = 0x03,
        FRAME_3   = 0x04,
        FRAME_4   = 0x05,
        FRAME_25  = 0x06,
        FRAME_2   = 0x07,
    };

    struct TextStyle
    {
        uint8_t x          = 0;
        uint8_t y          = 0;
        uint8_t scale      = 1;
        Color   color      = Color::WHITE;
        Color   background = Color::BLACK;
        bool    inverted   = false;
    };

public:
    explicit SSD1306(IIC& iic, uint8_t address = DefaultAddress);

    SSD1306(const SSD1306&)            = delete;
    SSD1306& operator=(const SSD1306&) = delete;

    bool setup();
    void end();

    bool clear(bool flush = true);
    bool display();
    bool displayRegion(uint8_t x, uint8_t y, uint8_t width, uint8_t height);
    bool displayPages(uint8_t pageStart, uint8_t pageCount, uint8_t columnStart = 0, uint8_t columnCount = Width);

    bool    setTextScale(uint8_t scale);
    uint8_t textScale() const;
    bool    setInverted(bool inverted);
    bool    inverted() const;
    bool    startHorizontalScroll(ScrollDirection direction,
                                  uint8_t         startPage,
                                  uint8_t         endPage,
                                  ScrollInterval  interval = ScrollInterval::FRAME_5);
    bool    stopScroll();

    bool setPixel(uint8_t x, uint8_t y, Color color = Color::WHITE);
    bool drawText(const char* text, uint8_t column = 0, uint8_t line = 0, Color color = Color::WHITE);
    bool drawText(const char* text, const TextStyle& style);

    bool displayText(const char* text);
    bool displayText(const char* text, uint8_t line);
    bool displayText(const char* text, uint8_t column, uint8_t line);
    bool displayText(const char* text, const TextStyle& style);

private:
    static constexpr size_t  BufferSize         = Width * Pages;
    static constexpr uint8_t FontWidth          = 5;
    static constexpr uint8_t FontHeight         = 7;
    static constexpr uint8_t CellWidth          = 6;
    static constexpr uint8_t CellHeight         = 8;
    static constexpr uint8_t CommandControlByte = 0x00;
    static constexpr uint8_t DataControlByte    = 0x40;

private:
    IIC*            m_iic             = nullptr;
    bool            m_started         = false;
    uint8_t         m_address         = DefaultAddress;
    uint8_t         m_textScale       = 1;
    bool            m_inverted        = false;
    bool            m_scrollActive    = false;
    ScrollDirection m_scrollDirection = ScrollDirection::RIGHT;
    ScrollInterval  m_scrollInterval  = ScrollInterval::FRAME_5;
    uint8_t         m_scrollStartPage = 0;
    uint8_t         m_scrollEndPage   = 0;
    uint8_t         m_buffer[BufferSize] {};

private:
    bool command(const uint8_t* bytes, size_t length);
    bool data(const uint8_t* bytes, size_t length);
    bool setScrollActive(bool active);
    bool scrollOverlaps(uint8_t pageStart, uint8_t pageCount) const;

    bool validTextStyle(const TextStyle& style) const;
    void drawGlyphPixel(uint8_t x, uint8_t y, bool on, const TextStyle& style);

    static const uint8_t* glyph(char value);
};
