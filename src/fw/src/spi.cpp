#include "src/fw/inc/spi.h"

#include "driver/spi_master.h"

#include <cstring>

SPI::SPI() : SPI(BusConfig {}) {}

SPI::SPI(BusConfig busConfig) : m_busConfig(busConfig) {}

SPI::~SPI() {
    (void)end();
}

SPI::SetupResult SPI::setupBus() {
    if (m_started) {
        return Err<Detail::ALREADY_STARTED>("SPI bus is already initialized");
    }

    const esp_err_t err = spi_bus_initialize(m_busConfig.host, &m_busConfig.bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        return NativeErr<Detail::DRIVER_INSTALL_FAILED>(err, "SPI bus initialization failed");
    }

    m_started = true;
    return Ok();
}

SPI::EndResult SPI::end() {
    if (!m_started) {
        return Ok();
    }

    for (size_t i = 0; i < m_deviceCnt; ++i) {
        if (m_devices[i].handle == nullptr) {
            continue;
        }

        const esp_err_t removeErr = spi_bus_remove_device(m_devices[i].handle);
        if (removeErr != ESP_OK) {
            return NativeErr<Detail::DEVICE_REMOVE_FAILED>(removeErr, "SPI device removal failed while shutting down the bus");
        }
        m_devices[i].handle = nullptr;
    }
    m_deviceCnt = 0;

    const esp_err_t err = spi_bus_free(m_busConfig.host);
    if (err != ESP_OK) {
        return NativeErr<Detail::DRIVER_DELETE_FAILED>(err, "SPI bus shutdown failed");
    }

    m_started = false;
    return Ok();
}

SPI::DeviceResult SPI::addDevice(const DeviceConfig& deviceConfig) {
    if (!m_started)
        return Err<Detail::NOT_STARTED>("SPI bus must be initialized before adding a device");

    if (m_deviceCnt >= DefaultMaxDeviceNum) {
        return Err<Detail::DEVICE_FULL>("SPI device table is full");
    }

    DeviceConfig& slot = m_devices[m_deviceCnt];
    slot               = deviceConfig;
    slot.index         = m_deviceCnt;
    slot.handle        = nullptr;

    const esp_err_t native = spi_bus_add_device(m_busConfig.host, &slot.device, &slot.handle);
    if (native != ESP_OK) {
        slot = {};
        return NativeErr<Detail::PARAM_CONFIG_FAILED>(native, "SPI device configuration failed");
    }

    ++m_deviceCnt;
    return Ok();
}

SPI::DeviceResult SPI::removeDevice(size_t index) {
    if (!m_started)
        return Err<Detail::NOT_STARTED>("SPI bus must be initialized before removing a device");

    if (!validDevice(index)) {
        return Err<Detail::PROBE_FAILED>("SPI device index is invalid");
    }

    const esp_err_t native = spi_bus_remove_device(m_devices[index].handle);
    if (native != ESP_OK) {
        return NativeErr<Detail::DEVICE_REMOVE_FAILED>(native, "SPI device removal failed");
    }

    for (size_t i = index; i + 1 < m_deviceCnt; ++i) {
        m_devices[i]       = m_devices[i + 1];
        m_devices[i].index = i;
    }

    --m_deviceCnt;
    m_devices[m_deviceCnt] = {};
    return Ok();
}

SPI::TransferResult SPI::transmit(size_t dvcIndex, const Transaction& transaction) {
    if (!m_started)
        return Err<Detail::NOT_STARTED>("SPI bus is not initialized");

    if (!validDevice(dvcIndex)) {
        return Err<Detail::PROBE_FAILED>("SPI device index is invalid");
    }

    if (transaction.length == 0) {
        return Ok();
    }

    if (transaction.txData == nullptr && transaction.rxData == nullptr) {
        return Err<Detail::INVALID_BUFFER>("SPI transaction has no input or output buffer");
    }

    spi_transaction_t espTrans {};
    espTrans.length = transaction.length * 8;

    if (transaction.txData != nullptr) {
        espTrans.tx_buffer = transaction.txData;
    }
    if (transaction.rxData != nullptr) {
        espTrans.rx_buffer = transaction.rxData;
    }

    const esp_err_t native = spi_device_polling_transmit(m_devices[dvcIndex].handle, &espTrans);
    if (native != ESP_OK) {
        if (transaction.rxData != nullptr && transaction.txData == nullptr)
            return NativeErr<Detail::READ_FAILED>(native, "SPI read transaction failed");
        if (transaction.rxData != nullptr && transaction.txData != nullptr)
            return NativeErr<Detail::WRITE_READ_FAILED>(native, "SPI full-duplex transaction failed");
        return NativeErr<Detail::WRITE_FAILED>(native, "SPI write transaction failed");
    }

    return Ok();
}

SPI::TransferResult SPI::write(size_t dvcIndex, const uint8_t* data, size_t len) {
    if (!validBuffer(data, len)) {
        return Err<Detail::INVALID_BUFFER>("SPI write buffer is null");
    }

    return transmit(dvcIndex, {.txData = data, .length = len});
}

SPI::TransferResult SPI::read(size_t dvcIndex, uint8_t* buf, size_t len) {
    if (!validBuffer(buf, len)) {
        return Err<Detail::INVALID_BUFFER>("SPI read buffer is null");
    }

    return transmit(dvcIndex, {.rxData = buf, .length = len, .rxOnly = true});
}

SPI::TransferResult SPI::writeRegister(size_t dvcIndex, uint8_t reg, uint8_t value) {
    const uint8_t buf[] {reg, value};
    return transmit(dvcIndex, {.txData = buf, .length = sizeof(buf)});
}

SPI::TransferResult SPI::readRegister(size_t dvcIndex, uint8_t reg, uint8_t* buf, size_t len) {
    if (!validBuffer(buf, len)) {
        return Err<Detail::INVALID_BUFFER>("SPI register read buffer is null");
    }

    static constexpr size_t MaxFrame = 64;
    uint8_t                 txBuf[MaxFrame] {};
    uint8_t                 rxBuf[MaxFrame] {};
    const size_t            frameSize = 1 + len;

    if (frameSize > MaxFrame) {
        return Err<Detail::INVALID_BUFFER>("SPI register read exceeds the local frame buffer");
    }

    txBuf[0]                    = reg;
    const TransferResult result = transmit(dvcIndex, {.txData = txBuf, .rxData = rxBuf, .length = frameSize});
    if (result.is_err())
        return result;

    std::memcpy(buf, rxBuf + 1, len);
    return Ok();
}

bool SPI::started() const {
    return m_started;
}

const SPI::BusConfig& SPI::config() const {
    return m_busConfig;
}

bool SPI::validDevice(size_t index) const {
    return index < m_deviceCnt && m_devices[index].handle != nullptr;
}

bool SPI::validBuffer(const uint8_t* data, size_t len) const {
    return len == 0 || data != nullptr;
}
