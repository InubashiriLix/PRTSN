#pragma once

#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "hal/spi_types.h"
#include "src/fw/inc/std_err.h"
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
        size_t                        index;
        spi_device_interface_config_t device = {};
        spi_device_handle_t           handle = nullptr;
        // int      csPin;
        // uint32_t clockHz;
        // uint8_t  queueSize;
        // uint8_t  addrBits;
        // bool     csActiveHigh = false;
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
    };

    struct Error
    {
        StdError    code;
        SPI::Detail detail;
        esp_err_t   native;

        constexpr bool ok() const noexcept {
            return code == StdError::OK && detail == Detail::NONE && native == ESP_OK;
        }

        constexpr explicit operator bool() const noexcept {
            return ok();
        }
    };

    struct Transaction
    {
        const uint8_t* txData;
        uint8_t*       rxData;
        size_t         length;
        bool           rxOnly = false;
    };

    explicit SPI(BusConfig& busConfig);

    const Error setupBus();
    const Error end();
    const Error addDevice(const DeviceConfig& config);
    const Error removeDevice(size_t index);
    const Error transmit(size_t dvcIndex, const Transaction& transaction);

    const Error write(size_t dvcIndex, const uint8_t* data, size_t len);
    const Error read(size_t dvcIndex, uint8_t* buf, size_t len);
    const Error writeRegister(size_t dvcIndex, uint8_t reg, uint8_t value);
    const Error readRegister(size_t dvcIndex, uint8_t reg, uint8_t* buf, size_t len);

private:
    inline static Error checkDeviceIndex(size_t index) {
        if (index >= DefaultMaxDeviceNum) {
            return {.code = StdError::FAIL, .detail = Detail::PARAM_CONFIG_FAILED, .native = ESP_FAIL};
        }
        return {.code = StdError::OK, .detail = Detail::NONE, .native = ESP_OK};
    }

    BusConfig&   m_busConfig;
    size_t       m_deviceCnt                    = 0;
    DeviceConfig m_devices[DefaultMaxDeviceNum] = {};
    bool         m_started                      = false;
    Error        m_lastError                    = {};
};
