#pragma once

#include <cstddef>
#include <cstdint>

struct Color
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;

    [[nodiscard]] constexpr bool operator==(
        const Color&) const noexcept = default;

    /**
     * Parse a compile-time CSS hexadecimal color.
     *
     * 将编译期 CSS 颜色字符串转换为 Color。
     *
     * @code
     * constexpr Color orange = Color::fromHex("#FF8800");
     * @endcode
     *
     * @param text 必须使用 "#RRGGBB" 格式。
     * @return Alpha 默认为 255 的 Color。
     */
    template <std::size_t Size>
    [[nodiscard]] static consteval Color fromHex(
        const char (&text)[Size]) {
        static_assert(
            Size == 8,
            "Color must use the form \"#RRGGBB\"");

        if (text[0] != '#')
            throw "Color must start with '#'";

        return {
            .r = makeByte(text[1], text[2]),
            .g = makeByte(text[3], text[4]),
            .b = makeByte(text[5], text[6]),
            .a = 255,
        };
    }

private:
    [[nodiscard]] static consteval uint8_t hexDigit(char value) {
        if (value >= '0' && value <= '9')
            return static_cast<uint8_t>(value - '0');

        if (value >= 'A' && value <= 'F')
            return static_cast<uint8_t>(value - 'A' + 10);

        if (value >= 'a' && value <= 'f')
            return static_cast<uint8_t>(value - 'a' + 10);

        throw "Invalid hexadecimal color digit";
    }

    [[nodiscard]] static consteval uint8_t makeByte(
        char high,
        char low) {
        return static_cast<uint8_t>(
            (hexDigit(high) << 4) | hexDigit(low));
    }
};
