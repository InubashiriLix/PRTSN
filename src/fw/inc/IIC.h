#pragma once

#include "src/fw/inc/std_err.h"

#include <driver/i2c.h>

#include <cstddef>
#include <cstdint>

class IIC
{
public:
    static constexpr gpio_num_t DefaultSclPin    = GPIO_NUM_5;
    static constexpr gpio_num_t DefaultSdaPin    = GPIO_NUM_4;
    static constexpr uint32_t   DefaultFrequency = 400000;
    static constexpr uint16_t   DefaultTimeoutMs = 50;

    struct Config
    {
        i2c_port_t   port         = I2C_NUM_0;
        size_t       slvRxBufSize = 0;
        size_t       slvTxBufSize = 0;
        i2c_config_t driverCfg {
            .mode          = I2C_MODE_MASTER,
            .sda_io_num    = DefaultSdaPin,
            .scl_io_num    = DefaultSclPin,
            .sda_pullup_en = true,
            .scl_pullup_en = true,
            .master        = {.clk_speed = DefaultFrequency},
            .clk_flags     = 0,
        };
        int      intrAllocFlags = 0;
        uint16_t timeoutMs      = DefaultTimeoutMs;
    };

    enum class Detail : uint8_t
    {
        NONE = 0,
        NOT_STARTED,
        ALREADY_STARTED,
        INVALID_BUFFER,
        PARAM_CONFIG_FAILED,
        DRIVER_INSTALL_FAILED,
        DRIVER_DELETE_FAILED,
        WRITE_FAILED,
        READ_FAILED,
        WRITE_READ_FAILED,
        PROBE_FAILED,
    };

    struct Error
    {
        StdError  code    = StdError::OK;
        Detail    detail  = Detail::NONE;
        esp_err_t native  = ESP_OK;
        uint8_t   address = 0;
        size_t    bytes   = 0;

        constexpr bool ok() const noexcept {
            return code == StdError::OK && detail == Detail::NONE && native == ESP_OK;
        }

        constexpr explicit operator bool() const noexcept {
            return ok();
        }
    };

    struct WriteBuffer
    {
        const uint8_t* data   = nullptr;
        size_t         length = 0;
    };

public:
    IIC();
    explicit IIC(Config config);
    IIC(gpio_num_t sdaPin, gpio_num_t sclPin, uint32_t frequency = DefaultFrequency, i2c_port_t port = I2C_NUM_0);
    ~IIC();

    Error setup();
    Error end();

public:
    Error write(uint8_t address, const uint8_t* data, size_t length);
    Error write(uint8_t address, const WriteBuffer* buffers, size_t count);
    Error read(uint8_t address, uint8_t* buffer, size_t length);

    Error writeRegister(uint8_t address, uint8_t reg, uint8_t value);
    Error writeRegister(uint8_t address, uint8_t reg, const uint8_t* data, size_t length);
    Error readRegister(uint8_t address, uint8_t reg, uint8_t* buffer, size_t length);

    Error   devicePresent(uint8_t address);
    uint8_t scan(uint8_t* addresses, size_t maxCount, uint8_t first = 0x03, uint8_t last = 0x77);

    bool          started() const;
    Error         lastError() const;
    const Config& config() const;

    static const char* detailName(Detail detail) noexcept;

private:
    Config m_config;
    bool   m_started = false;
    Error  m_lastError {};

    Error      makeError(StdError code, Detail detail, esp_err_t native, uint8_t address = 0, size_t bytes = 0);
    Error      clearError(uint8_t address = 0, size_t bytes = 0);
    Error      ensureStarted();
    bool       validBuffer(const uint8_t* data, size_t length);
    TickType_t timeoutTicks() const;

    static Config makeMasterConfig(gpio_num_t sdaPin, gpio_num_t sclPin, uint32_t frequency, i2c_port_t port);
};
