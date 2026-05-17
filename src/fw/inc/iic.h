#pragma once

#include "../../cfg/BoardConfig.h"

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

class TwoWire;

class IIC
{
public:
    enum class Error : uint8_t
    {
        OK = 0,
        NOT_STARTED,
        INVALID_ARGUMENT,
        BEGIN_FAILED,
        BUFFER_OVERFLOW,
        ADDRESS_NACK,
        DATA_NACK,
        TIMEOUT,
        SHORT_READ,
        OTHER,
    };

    struct Config
    {
        uint8_t  sdaPin    = PRTN_IIC_SDA_PIN;
        uint8_t  sclPin    = PRTN_IIC_SCL_PIN;
        uint32_t frequency = PRTN_IIC_FREQUENCY;
        uint16_t timeoutMs = PRTN_IIC_TIMEOUT_MS;
    };

    struct Status
    {
        bool     started     = false;
        uint8_t  sdaPin      = PRTN_IIC_SDA_PIN;
        uint8_t  sclPin      = PRTN_IIC_SCL_PIN;
        uint32_t frequency   = PRTN_IIC_FREQUENCY;
        Error    lastError   = Error::OK;
        uint8_t  lastAddress = 0;
        size_t   lastBytes   = 0;
    };

public:
    IIC();
    explicit IIC(const Config& config);
    IIC(uint8_t sdaPin, uint8_t sclPin, uint32_t frequency = PRTN_IIC_FREQUENCY);

    bool begin();
    void end();

    bool write(uint8_t address, const uint8_t* data, size_t length);
    bool read(uint8_t address, uint8_t* buffer, size_t length);

    bool writeRegister(uint8_t address, uint8_t reg, uint8_t value);
    bool writeRegister(uint8_t address, uint8_t reg, const uint8_t* data, size_t length);
    bool readRegister(uint8_t address, uint8_t reg, uint8_t* buffer, size_t length);

    bool    devicePresent(uint8_t address);
    uint8_t scan(uint8_t* addresses, size_t maxCount, uint8_t first = 0x03, uint8_t last = 0x77);

    Status      status() const;
    Error       lastError() const;
    const char* lastErrorName() const;

private:
    Config   m_config;
    Status   m_status;
    TwoWire* m_wire = nullptr;

    bool ensureStarted();
    bool validBuffer(const uint8_t* data, size_t length);
    bool finishTransmission(uint8_t address, size_t bytesWritten, uint8_t result);
    void setError(Error error, uint8_t address = 0, size_t bytes = 0);

    static Error       mapTransmissionError(uint8_t result);
    static const char* errorName(Error error);
};
