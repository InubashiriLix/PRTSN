#pragma once

#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "hal/spi_types.h"
#include "src/fw/inc/Result.h"
#include "src/fw/inc/std_err.h"

#include <cstddef>
#include <cstdint>

class SPI
{
public:
    constexpr static spi_host_device_t DefaultHost         = SPI2_HOST;
    constexpr static size_t            DefaultMaxDeviceNum = 4;
    constexpr static spi_bus_config_t  DefaultBusConfig {
        .mosi_io_num           = 23,
        .miso_io_num           = 19,
        .sclk_io_num           = 18,
        .quadwp_io_num         = -1,
        .quadhd_io_num         = -1,
        .data4_io_num          = -1,
        .data5_io_num          = -1,
        .data6_io_num          = -1,
        .data7_io_num          = -1,
        .data_io_default_level = false,
        .max_transfer_sz       = 4096,
        .flags                 = SPICOMMON_BUSFLAG_MASTER,
        .isr_cpu_id            = ESP_INTR_CPU_AFFINITY_AUTO,
        .intr_flags            = 0};

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

    SPI(const SPI&)            = delete;
    SPI& operator=(const SPI&) = delete;

    /**
     * @brief SPI 总线初始化结果。Result of initializing the SPI bus.
     *
     * ESP-IDF 的原生错误会自动保存在 `DRIVER_INSTALL_FAILED` 的 cause chain 中。
     * The native ESP-IDF failure is retained automatically as a cause.
     *
     * @code
     * const auto result = spi.setupBus();
     * if (result.is_err())
     *     logError(result.error());
     * @endcode
     */
    using SetupResult = Result<void, ErrorSet<Detail::ALREADY_STARTED, TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::DRIVER_INSTALL_FAILED>>>;

    /** `end()` 是幂等的，只报告真实的驱动清理失败。 */
    using EndResult = Result<void, ErrorSet<TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::DEVICE_REMOVE_FAILED>, TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::DRIVER_DELETE_FAILED>>>;

    /** 添加和移除设备共用一组设备管理错误，降低调用端的类型数量。 */
    using DeviceResult = Result<void, ErrorSet<Detail::NOT_STARTED, Detail::DEVICE_FULL, Detail::PROBE_FAILED, TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::PARAM_CONFIG_FAILED>, TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::DEVICE_REMOVE_FAILED>>>;

    /** 所有同步传输 API 共用此结果；读写方向仍由具体错误码区分。 */
    using TransferResult = Result<void, ErrorSet<Detail::NOT_STARTED, Detail::INVALID_BUFFER, Detail::PROBE_FAILED, TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::WRITE_FAILED>, TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::READ_FAILED>, TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Detail::WRITE_READ_FAILED>>>;

    [[nodiscard]] SetupResult    setupBus();
    [[nodiscard]] EndResult      end();
    [[nodiscard]] DeviceResult   addDevice(const DeviceConfig& config);
    [[nodiscard]] DeviceResult   removeDevice(size_t index);
    [[nodiscard]] TransferResult transmit(size_t dvcIndex, const Transaction& transaction);

    [[nodiscard]] TransferResult write(size_t dvcIndex, const uint8_t* data, size_t len);
    [[nodiscard]] TransferResult read(size_t dvcIndex, uint8_t* buf, size_t len);
    [[nodiscard]] TransferResult writeRegister(size_t dvcIndex, uint8_t reg, uint8_t value);
    [[nodiscard]] TransferResult readRegister(size_t dvcIndex, uint8_t reg, uint8_t* buf, size_t len);

    bool             started() const;
    const BusConfig& config() const;

private:
    BusConfig    m_busConfig {};
    size_t       m_deviceCnt                    = 0;
    DeviceConfig m_devices[DefaultMaxDeviceNum] = {};
    bool         m_started                      = false;

private:
    bool validDevice(size_t index) const;
    bool validBuffer(const uint8_t* data, size_t len) const;
};
