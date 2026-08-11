#include "src/dvc/inc/WS2812.h"

#include <algorithm>
#include <cstring>
#include <new>

WS2812::WS2812(gpio_num_t pin, size_t pixelCount)
    : WS2812(pin, Config {.pixelCount = pixelCount}) {}

WS2812::WS2812(gpio_num_t pin, Config config)
    : m_pin(pin),
      m_config(config),
      m_rmt(makeRmtConfig(pin, config)) {}

WS2812::~WS2812() {
    end();
}

WS2812::SetupResult WS2812::setup() {
    if (m_started)
        return Err<Detail::ALREADY_STARTED>();

    if (!validConfig())
        return Err<Detail::INVALID_CONFIG>();

    const AllocateBufferResult allocateResult = allocateBuffers();
    if (allocateResult.is_err())
        return Err<Detail::NO_BUFFER>();

    const RMT::Error rmtResult = m_rmt.setup();
    if (!rmtResult) {
        releaseBuffers();
        return Err<Detail::SETUP_RMT_FAILED>();
    }

    const ClearResult clearResult = clear();
    if (clearResult.is_err()) {
        (void)m_rmt.end();
        releaseBuffers();
        return Err(clearResult.error());
    }

    m_started = true;
    return Ok();
}

WS2812::EndResult WS2812::end() {
    if (!m_started) {
        releaseBuffers();
        return Err<Detail::NOT_STARTED>();
    }

    RMT::Error rmtErr = m_rmt.end();
    if (!rmtErr) {
        return Err<Detail::END_RMT_FAILED>();
    }

    m_started = false;
    releaseBuffers();
    clearError();
    return Ok();
}

WS2812::ShowResult WS2812::show() {
    if (!m_started)
        return Err<Detail::NOT_STARTED>();
    if (m_pixels == nullptr || m_symbols == nullptr || m_symbolCount == 0)
        return Err<Detail::NO_BUFFER>();

    encode();

    RMT::Error rmtErr = m_rmt.transmit(m_symbols, m_symbolCount);
    if (!rmtErr)
        return Err<Detail::RMT_TRANSMIT_FAILED>();

    rmtErr = m_rmt.waitDone(m_config.showTimeoutMs);
    if (!rmtErr)
        return Err<Detail::RMT_WAIT_FAILED>();

    clearError();
    return Ok();
}

WS2812::ClearResult WS2812::clear() {
    if (m_pixels == nullptr) {
        return Err<Detail::NO_BUFFER>();
    }

    std::memset(m_pixels, 0, sizeof(Color) * m_config.pixelCount);
    clearError();
    return Ok();
}

WS2812::ClearShowResult WS2812::clearShow() {
    const ClearResult clearResult = clear();
    if (clearResult.is_err()) {
        return Err(clearResult.error());
    }

    return show();
}

WS2812::SetPixelResult WS2812::setPixel(size_t index, Color color) {
    if (!validIndex(index))
        return Err<Detail::INVALID_INDEX>();
    if (m_pixels == nullptr)
        return Err<Detail::NO_BUFFER>();

    m_pixels[index] = color;
    clearError();
    return Ok();
}

WS2812::SetColorResult WS2812::setColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a, size_t index) {
    return setPixel(index, Color {.r = r, .g = g, .b = b, .a = a});
}

WS2812::SetAllColorsResult WS2812::setAllColors(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (m_pixels == nullptr)
        return Err<Detail::NO_BUFFER>();

    const Color color {.r = r, .g = g, .b = b, .a = a};
    for (size_t i = 0; i < m_config.pixelCount; ++i) {
        m_pixels[i] = color;
    }

    clearError();
    return Ok();
}

WS2812::SetSingleColorResult WS2812::setRed(uint8_t r, size_t index) {
    if (!validIndex(index)) {
        return Err<Detail::INVALID_INDEX>();
    }
    m_pixels[index].r = r;
    clearError();
    return Ok();
}

WS2812::SetSingleColorResult WS2812::setGreen(uint8_t g, size_t index) {
    if (!validIndex(index)) {
        return Err<Detail::INVALID_INDEX>();
    }
    m_pixels[index].g = g;
    clearError();
    return Ok();
}

WS2812::SetSingleColorResult WS2812::setBlue(uint8_t b, size_t index) {
    if (!validIndex(index)) {
        return Err<Detail::INVALID_INDEX>();
    }
    m_pixels[index].b = b;
    clearError();
    return Ok();
}

WS2812::SetAlphaResult WS2812::setAlpha(uint8_t a, size_t index) {
    if (!validIndex(index))
        return Err<Detail::INVALID_INDEX>();
    if (m_pixels == nullptr)
        return Err<Detail::NO_BUFFER>();

    m_pixels[index].a = a;
    clearError();
    return Ok();
}

WS2812::SetBrightnessResult WS2812::setBrightness(uint8_t brightness, bool flush) {
    m_config.brightness = brightness;
    if (flush)
        return show();

    clearError();
    return Ok();
}

bool WS2812::started() const {
    return m_started;
}

size_t WS2812::pixelCount() const {
    return m_config.pixelCount;
}

uint8_t WS2812::brightness() const {
    return m_config.brightness;
}

WS2812::Color WS2812::pixel(size_t index) const {
    if (!validIndex(index) || m_pixels == nullptr) {
        return {};
    }

    return m_pixels[index];
}

WS2812::Error WS2812::lastError() const {
    return m_lastError;
}

const char* WS2812::detailName(Detail detail) noexcept {
    switch (detail) {
        case Detail::NONE:
            return "NONE";
        case Detail::NOT_STARTED:
            return "NOT_STARTED";
        case Detail::ALREADY_STARTED:
            return "ALREADY_STARTED";
        case Detail::INVALID_CONFIG:
            return "INVALID_CONFIG";
        case Detail::INVALID_INDEX:
            return "INVALID_INDEX";
        case Detail::NO_BUFFER:
            return "NO_BUFFER";
        case Detail::SETUP_RMT_FAILED:
            return "SETUP_RMT_FAILED";
        case Detail::END_RMT_FAILED:
            return "END_RMT_FAILED";
        case Detail::RMT_TRANSMIT_FAILED:
            return "RMT_TRANSMIT_FAILED";
        case Detail::RMT_WAIT_FAILED:
            return "RMT_WAIT_FAILED";
    }

    return "UNKNOWN";
}

const char* WS2812::colorOrderName(ColorOrder order) noexcept {
    switch (order) {
        case ColorOrder::RGB:
            return "RGB";
        case ColorOrder::GRB:
            return "GRB";
        case ColorOrder::BRG:
            return "BRG";
        case ColorOrder::BGR:
            return "BGR";
    }

    return "UNKNOWN";
}

WS2812::AllocateBufferResult WS2812::allocateBuffers() {
    releaseBuffers();

    const size_t pixelBytes = sizeof(Color) * m_config.pixelCount;
    m_symbolCount           = m_config.pixelCount * 24 + 1;

    m_pixels = new (std::nothrow) Color[m_config.pixelCount] {};
    if (m_pixels == nullptr) {
        m_symbolCount = 0;
        return Err<StdError::NO_MEM>();
    }

    m_symbols = new (std::nothrow) RMT::Symbol[m_symbolCount] {};
    if (m_symbols == nullptr) {
        releaseBuffers();
        return Err<StdError::NO_MEM>();
    }

    std::memset(m_pixels, 0, pixelBytes);
    clearError();
    return Ok();
}

void WS2812::releaseBuffers() {
    delete[] m_pixels;
    delete[] m_symbols;
    m_pixels      = nullptr;
    m_symbols     = nullptr;
    m_symbolCount = 0;
}

void WS2812::encode() {
    size_t symbolIndex = 0;
    for (size_t i = 0; i < m_config.pixelCount; ++i) {
        encodePixel(i, symbolIndex);
    }

    const uint16_t resetTicks        = RMT::ticksFromNs(m_config.resolutionHz, m_config.resetUs * 1000U);
    m_symbols[symbolIndex].level0    = 0;
    m_symbols[symbolIndex].duration0 = resetTicks;
    m_symbols[symbolIndex].level1    = 0;
    m_symbols[symbolIndex].duration1 = resetTicks;
}

void WS2812::encodePixel(size_t pixelIndex, size_t& symbolIndex) {
    uint8_t first  = 0;
    uint8_t second = 0;
    uint8_t third  = 0;
    orderedBytes(scaled(m_pixels[pixelIndex]), first, second, third);

    encodeByte(first, symbolIndex);
    encodeByte(second, symbolIndex);
    encodeByte(third, symbolIndex);
}

void WS2812::encodeByte(uint8_t value, size_t& symbolIndex) {
    for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
        encodeBit((value & mask) != 0, symbolIndex);
    }
}

void WS2812::encodeBit(bool one, size_t& symbolIndex) {
    RMT::Symbol& symbol = m_symbols[symbolIndex++];
    symbol.level0       = 1;
    symbol.duration0    = RMT::ticksFromNs(m_config.resolutionHz, one ? m_config.t1hNs : m_config.t0hNs);
    symbol.level1       = 0;
    symbol.duration1    = RMT::ticksFromNs(m_config.resolutionHz, one ? m_config.t1lNs : m_config.t0lNs);
}

void WS2812::orderedBytes(Color color, uint8_t& first, uint8_t& second, uint8_t& third) const {
    switch (m_config.colorOrder) {
        case ColorOrder::RGB:
            first  = color.r;
            second = color.g;
            third  = color.b;
            return;
        case ColorOrder::GRB:
            first  = color.g;
            second = color.r;
            third  = color.b;
            return;
        case ColorOrder::BRG:
            first  = color.b;
            second = color.r;
            third  = color.g;
            return;
        case ColorOrder::BGR:
            first  = color.b;
            second = color.g;
            third  = color.r;
            return;
    }
}

WS2812::Color WS2812::scaled(Color color) const {
    const uint16_t scale = static_cast<uint16_t>(color.a) * m_config.brightness;
    color.r              = static_cast<uint8_t>((static_cast<uint16_t>(color.r) * scale) / 65025U);
    color.g              = static_cast<uint8_t>((static_cast<uint16_t>(color.g) * scale) / 65025U);
    color.b              = static_cast<uint8_t>((static_cast<uint16_t>(color.b) * scale) / 65025U);
    return color;
}

WS2812::Error WS2812::makeError(StdError code, Detail detail, esp_err_t native, RMT::Error rmt) {
    m_lastError = Error {.code = code, .detail = detail, .rmt = rmt, .native = native};
    return m_lastError;
}

WS2812::Error WS2812::mapRmtError(RMT::Error rmt, Detail detail) {
    return makeError(rmt.code, detail, rmt.native, rmt);
}

void WS2812::clearError() {
    m_lastError = Error {};
}

bool WS2812::validConfig() const {
    return m_pin != GPIO_NUM_NC &&
           m_config.pixelCount > 0 &&
           m_config.resolutionHz > 0 &&
           m_config.t0hNs > 0 &&
           m_config.t0lNs > 0 &&
           m_config.t1hNs > 0 &&
           m_config.t1lNs > 0 &&
           m_config.resetUs >= 50;
}

bool WS2812::validIndex(size_t index) const {
    return index < m_config.pixelCount;
}

RMT::Config WS2812::makeRmtConfig(gpio_num_t pin, const Config& config) {
    return RMT::Config {
        .gpio            = pin,
        .resolutionHz    = config.resolutionHz,
        .memBlockSymbols = RMT::DefaultMemBlockSymbols,
        .transQueueDepth = RMT::DefaultTransQueueDepth,
        .invertOut       = false,
        .withDma         = false,
        .openDrain       = false,
        .initLevel       = false,
    };
}
