#include "src/fw/inc/iic.h"

#include "src/cfg/BuildConfig.h"

#if PRTN_ENABLE_IIC

#include <Wire.h>

IIC::IIC() : IIC(Config {}) {}

IIC::IIC(const Config& config)
    : m_config(config),
      m_status {
          false,
          config.sdaPin,
          config.sclPin,
          config.frequency,
          Error::OK,
          0,
          0,
      },
      m_wire(&Wire) {}

IIC::IIC(uint8_t sdaPin, uint8_t sclPin, uint32_t frequency)
    : IIC(Config {sdaPin, sclPin, frequency, 50}) {}

bool IIC::begin() {
    m_wire = &Wire;
    m_wire->setTimeOut(m_config.timeoutMs);

    if (!m_wire->begin(m_config.sdaPin, m_config.sclPin, m_config.frequency)) {
        setError(Error::BEGIN_FAILED);
        m_status.started = false;
        return false;
    }

    m_status.started = true;
    setError(Error::OK);
    return true;
}

void IIC::end() {
    if (m_wire != nullptr) {
        m_wire->end();
    }

    m_status.started = false;
    setError(Error::OK);
}

bool IIC::write(uint8_t address, const uint8_t* data, size_t length) {
    if (!ensureStarted() || !validBuffer(data, length)) {
        return false;
    }

    m_wire->beginTransmission(address);
    const size_t written = length > 0 ? m_wire->write(data, length) : 0;
    const uint8_t result = m_wire->endTransmission();

    return finishTransmission(address, written, result);
}

bool IIC::read(uint8_t address, uint8_t* buffer, size_t length) {
    if (!ensureStarted() || !validBuffer(buffer, length)) {
        return false;
    }

    const size_t received = m_wire->requestFrom(address, length);
    size_t       index    = 0;

    while (m_wire->available() > 0 && index < length) {
        buffer[index++] = static_cast<uint8_t>(m_wire->read());
    }

    if (received != length || index != length) {
        setError(Error::SHORT_READ, address, index);
        return false;
    }

    setError(Error::OK, address, index);
    return true;
}

bool IIC::writeRegister(uint8_t address, uint8_t reg, uint8_t value) {
    return writeRegister(address, reg, &value, 1);
}

bool IIC::writeRegister(uint8_t address, uint8_t reg, const uint8_t* data, size_t length) {
    if (!ensureStarted() || !validBuffer(data, length)) {
        return false;
    }

    m_wire->beginTransmission(address);
    size_t written = m_wire->write(reg);
    if (length > 0) {
        written += m_wire->write(data, length);
    }

    const uint8_t result = m_wire->endTransmission();
    return finishTransmission(address, written, result);
}

bool IIC::readRegister(uint8_t address, uint8_t reg, uint8_t* buffer, size_t length) {
    if (!ensureStarted() || !validBuffer(buffer, length)) {
        return false;
    }

    m_wire->beginTransmission(address);
    const size_t written = m_wire->write(reg);
    uint8_t      result  = m_wire->endTransmission(false);

    if (!finishTransmission(address, written, result)) {
        return false;
    }

    const size_t received = m_wire->requestFrom(address, length);
    size_t       index    = 0;

    while (m_wire->available() > 0 && index < length) {
        buffer[index++] = static_cast<uint8_t>(m_wire->read());
    }

    if (received != length || index != length) {
        setError(Error::SHORT_READ, address, index);
        return false;
    }

    setError(Error::OK, address, index);
    return true;
}

bool IIC::devicePresent(uint8_t address) {
    if (!ensureStarted()) {
        return false;
    }

    m_wire->beginTransmission(address);
    const uint8_t result = m_wire->endTransmission();
    return finishTransmission(address, 0, result);
}

uint8_t IIC::scan(uint8_t* addresses, size_t maxCount, uint8_t first, uint8_t last) {
    if (!ensureStarted() || addresses == nullptr || maxCount == 0 || first > last) {
        setError(Error::INVALID_ARGUMENT);
        return 0;
    }

    uint8_t count = 0;

    for (uint8_t address = first; address <= last && count < maxCount; ++address) {
        m_wire->beginTransmission(address);
        if (m_wire->endTransmission() == 0) {
            addresses[count++] = address;
        }
    }

    setError(Error::OK, 0, count);
    return count;
}

IIC::Status IIC::status() const {
    return m_status;
}

IIC::Error IIC::lastError() const {
    return m_status.lastError;
}

const char* IIC::lastErrorName() const {
    return errorName(m_status.lastError);
}

bool IIC::ensureStarted() {
    if (m_status.started && m_wire != nullptr) {
        return true;
    }

    setError(Error::NOT_STARTED);
    return false;
}

bool IIC::validBuffer(const uint8_t* data, size_t length) {
    if (length == 0) {
        return true;
    }

    if (data != nullptr) {
        return true;
    }

    setError(Error::INVALID_ARGUMENT);
    return false;
}

bool IIC::finishTransmission(uint8_t address, size_t bytesWritten, uint8_t result) {
    const Error error = mapTransmissionError(result);
    setError(error, address, bytesWritten);
    return error == Error::OK;
}

void IIC::setError(Error error, uint8_t address, size_t bytes) {
    m_status.lastError   = error;
    m_status.lastAddress = address;
    m_status.lastBytes   = bytes;
}

IIC::Error IIC::mapTransmissionError(uint8_t result) {
    switch (result) {
        case 0:
            return Error::OK;
        case 1:
            return Error::BUFFER_OVERFLOW;
        case 2:
            return Error::ADDRESS_NACK;
        case 3:
            return Error::DATA_NACK;
        case 5:
            return Error::TIMEOUT;
        case 4:
        default:
            return Error::OTHER;
    }
}

const char* IIC::errorName(Error error) {
    switch (error) {
        case Error::OK:
            return "OK";
        case Error::NOT_STARTED:
            return "NOT_STARTED";
        case Error::INVALID_ARGUMENT:
            return "INVALID_ARGUMENT";
        case Error::BEGIN_FAILED:
            return "BEGIN_FAILED";
        case Error::BUFFER_OVERFLOW:
            return "BUFFER_OVERFLOW";
        case Error::ADDRESS_NACK:
            return "ADDRESS_NACK";
        case Error::DATA_NACK:
            return "DATA_NACK";
        case Error::TIMEOUT:
            return "TIMEOUT";
        case Error::SHORT_READ:
            return "SHORT_READ";
        case Error::OTHER:
        default:
            return "OTHER";
    }
}

#endif
