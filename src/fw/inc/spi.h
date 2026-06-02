#pragma once

#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "hal/spi_types.h"
#include "src/fw/inc/std_err.h"

#include <cstddef>
#include <cstdint>

class SPI
{
public:
    constexpr static spi_host_device_t DefaultHost         = SPI2_HOST;
    constexpr static size_t            DefaultMaxDeviceNum = 4;
    constexpr static spi_bus_config_t  DefaultBusConfig {
        .mosi_io_num     = 23,
        .miso_io_num     = 19,
        .sclk_io_num     = 18,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4096,
        .flags           = SPICOMMON_BUSFLAG_MASTER,
        .intr_flags      = 0};

public:
    struct BusConfig
    {
        spi_host_device_t host = SPI2_HOST;
        spi_bus_config_t  bus  = DefaultBusConfig;
    };

    struct DeviceConfig
    {
        size_t                        index  = 0;
        spi_device_interface_config_t device = {};
        spi_device_handle_t           handle = nullptr;
    };

    enum class Detail : uint8_t
    {
        NONE = 0,
        NOT_STARTED,
        ALREADY_STARTED,
        DEVICE_FULL,
        INVALID_BUFFER,
        PARAM_CONFIG_FAILED,
        DRIVER_INSTALL_FAILED,
        DRIVER_DELETE_FAILED,
        WRITE_FAILED,
        READ_FAILED,
        WRITE_READ_FAILED,
        PROBE_FAILED,
        DEVICE_REMOVE_FAILED,
    };

    struct Error
    {
        StdError    code   = StdError::OK;
        SPI::Detail detail = Detail::NONE;
        esp_err_t   native = ESP_OK;

        constexpr bool ok() const noexcept {
            return code == StdError::OK && detail == Detail::NONE && native == ESP_OK;
        }

        constexpr explicit operator bool() const noexcept {
            return ok();
        }
    };

    struct Transaction
    {
        const uint8_t* txData = nullptr;
        uint8_t*       rxData = nullptr;
        size_t         length = 0;
        bool           rxOnly = false;
    };

    SPI();
    explicit SPI(BusConfig busConfig);
    ~SPI();

    Error setupBus();
    Error end();
    Error addDevice(const DeviceConfig& config);
    Error removeDevice(size_t index);
    Error transmit(size_t dvcIndex, const Transaction& transaction);

    Error write(size_t dvcIndex, const uint8_t* data, size_t len);
    Error read(size_t dvcIndex, uint8_t* buf, size_t len);
    Error writeRegister(size_t dvcIndex, uint8_t reg, uint8_t value);
    Error readRegister(size_t dvcIndex, uint8_t reg, uint8_t* buf, size_t len);

    bool             started() const;
    Error            lastError() const;
    const BusConfig& config() const;

    static const char* detailName(Detail detail) noexcept;

private:
    BusConfig    m_busConfig {};
    size_t       m_deviceCnt                    = 0;
    DeviceConfig m_devices[DefaultMaxDeviceNum] = {};
    bool         m_started                      = false;
    Error        m_lastError                    = {};

private:
    Error makeError(StdError code, Detail detail, esp_err_t native);
    Error clearError();
    Error ensureStarted();
    bool  validDevice(size_t index) const;
    bool  validBuffer(const uint8_t* data, size_t len) const;
};
