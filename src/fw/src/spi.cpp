#include "src/fw/inc/spi.h"

#include "driver/spi_master.h"

#include <cstring>

SPI::SPI() : SPI(BusConfig {}) {}

SPI::SPI(BusConfig busConfig) : m_busConfig(busConfig) {}

SPI::~SPI() {
    end();
}

SPI::Error SPI::setupBus() {
    if (m_started) {
        return makeError(StdError::INVALID_STATE, Detail::ALREADY_STARTED, ESP_ERR_INVALID_STATE);
    }

    const esp_err_t err = spi_bus_initialize(m_busConfig.host, &m_busConfig.bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        return makeError(toStdErr(err), Detail::DRIVER_INSTALL_FAILED, err);
    }

    m_started = true;
    return clearError();
}

SPI::Error SPI::end() {
    if (!m_started) {
        return clearError();
    }

    for (size_t i = 0; i < m_deviceCnt; ++i) {
        if (m_devices[i].handle == nullptr) {
            continue;
        }

        const esp_err_t removeErr = spi_bus_remove_device(m_devices[i].handle);
        if (removeErr != ESP_OK) {
            return makeError(toStdErr(removeErr), Detail::DEVICE_REMOVE_FAILED, removeErr);
        }
        m_devices[i].handle = nullptr;
    }
    m_deviceCnt = 0;

    const esp_err_t err = spi_bus_free(m_busConfig.host);
    if (err != ESP_OK) {
        return makeError(toStdErr(err), Detail::DRIVER_DELETE_FAILED, err);
    }

    m_started = false;
    return clearError();
}

SPI::Error SPI::addDevice(const DeviceConfig& deviceConfig) {
    Error err = ensureStarted();
    if (!err) {
        return err;
    }

    if (m_deviceCnt >= DefaultMaxDeviceNum) {
        return makeError(StdError::INVALID_STATE, Detail::DEVICE_FULL, ESP_ERR_INVALID_STATE);
    }

    DeviceConfig& slot = m_devices[m_deviceCnt];
    slot               = deviceConfig;
    slot.index         = m_deviceCnt;
    slot.handle        = nullptr;

    const esp_err_t native = spi_bus_add_device(m_busConfig.host, &slot.device, &slot.handle);
    if (native != ESP_OK) {
        slot = {};
        return makeError(toStdErr(native), Detail::PARAM_CONFIG_FAILED, native);
    }

    ++m_deviceCnt;
    return clearError();
}

SPI::Error SPI::removeDevice(size_t index) {
    Error err = ensureStarted();
    if (!err) {
        return err;
    }

    if (!validDevice(index)) {
        return makeError(StdError::INVALID_ARGS, Detail::PROBE_FAILED, ESP_ERR_INVALID_ARG);
    }

    const esp_err_t native = spi_bus_remove_device(m_devices[index].handle);
    if (native != ESP_OK) {
        return makeError(toStdErr(native), Detail::DEVICE_REMOVE_FAILED, native);
    }

    for (size_t i = index; i + 1 < m_deviceCnt; ++i) {
        m_devices[i]       = m_devices[i + 1];
        m_devices[i].index = i;
    }

    --m_deviceCnt;
    m_devices[m_deviceCnt] = {};
    return clearError();
}

SPI::Error SPI::transmit(size_t dvcIndex, const Transaction& transaction) {
    Error err = ensureStarted();
    if (!err) {
        return err;
    }

    if (!validDevice(dvcIndex)) {
        return makeError(StdError::INVALID_ARGS, Detail::PROBE_FAILED, ESP_ERR_INVALID_ARG);
    }

    if (transaction.length == 0) {
        return clearError();
    }

    if (transaction.txData == nullptr && transaction.rxData == nullptr) {
        return makeError(StdError::INVALID_ARGS, Detail::INVALID_BUFFER, ESP_ERR_INVALID_ARG);
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
        const Detail detail = transaction.rxData != nullptr && transaction.txData == nullptr ? Detail::READ_FAILED : Detail::WRITE_FAILED;
        return makeError(toStdErr(native), detail, native);
    }

    return clearError();
}

SPI::Error SPI::write(size_t dvcIndex, const uint8_t* data, size_t len) {
    if (!validBuffer(data, len)) {
        return makeError(StdError::INVALID_ARGS, Detail::INVALID_BUFFER, ESP_ERR_INVALID_ARG);
    }

    return transmit(dvcIndex, {.txData = data, .length = len});
}

SPI::Error SPI::read(size_t dvcIndex, uint8_t* buf, size_t len) {
    if (!validBuffer(buf, len)) {
        return makeError(StdError::INVALID_ARGS, Detail::INVALID_BUFFER, ESP_ERR_INVALID_ARG);
    }

    return transmit(dvcIndex, {.rxData = buf, .length = len, .rxOnly = true});
}

SPI::Error SPI::writeRegister(size_t dvcIndex, uint8_t reg, uint8_t value) {
    const uint8_t buf[] {reg, value};
    Error         err = transmit(dvcIndex, {.txData = buf, .length = sizeof(buf)});
    if (!err) {
        return makeError(err.code, Detail::WRITE_READ_FAILED, err.native);
    }

    return clearError();
}

SPI::Error SPI::readRegister(size_t dvcIndex, uint8_t reg, uint8_t* buf, size_t len) {
    if (!validBuffer(buf, len)) {
        return makeError(StdError::INVALID_ARGS, Detail::INVALID_BUFFER, ESP_ERR_INVALID_ARG);
    }

    static constexpr size_t MaxFrame = 64;
    uint8_t                 txBuf[MaxFrame] {};
    uint8_t                 rxBuf[MaxFrame] {};
    const size_t            frameSize = 1 + len;

    if (frameSize > MaxFrame) {
        return makeError(StdError::INVALID_SIZE, Detail::INVALID_BUFFER, ESP_ERR_INVALID_SIZE);
    }

    txBuf[0]  = reg;
    Error err = transmit(dvcIndex, {.txData = txBuf, .rxData = rxBuf, .length = frameSize});
    if (!err) {
        return makeError(err.code, Detail::WRITE_READ_FAILED, err.native);
    }

    std::memcpy(buf, rxBuf + 1, len);
    return clearError();
}

bool SPI::started() const {
    return m_started;
}

SPI::Error SPI::lastError() const {
    return m_lastError;
}

const SPI::BusConfig& SPI::config() const {
    return m_busConfig;
}

const char* SPI::detailName(Detail detail) noexcept {
    switch (detail) {
        case Detail::NONE:
            return "NONE";
        case Detail::NOT_STARTED:
            return "NOT_STARTED";
        case Detail::ALREADY_STARTED:
            return "ALREADY_STARTED";
        case Detail::DEVICE_FULL:
            return "DEVICE_FULL";
        case Detail::INVALID_BUFFER:
            return "INVALID_BUFFER";
        case Detail::PARAM_CONFIG_FAILED:
            return "PARAM_CONFIG_FAILED";
        case Detail::DRIVER_INSTALL_FAILED:
            return "DRIVER_INSTALL_FAILED";
        case Detail::DRIVER_DELETE_FAILED:
            return "DRIVER_DELETE_FAILED";
        case Detail::WRITE_FAILED:
            return "WRITE_FAILED";
        case Detail::READ_FAILED:
            return "READ_FAILED";
        case Detail::WRITE_READ_FAILED:
            return "WRITE_READ_FAILED";
        case Detail::PROBE_FAILED:
            return "PROBE_FAILED";
        case Detail::DEVICE_REMOVE_FAILED:
            return "DEVICE_REMOVE_FAILED";
    }

    return "UNKNOWN";
}

SPI::Error SPI::makeError(StdError code, Detail detail, esp_err_t native) {
    m_lastError = Error {.code = code, .detail = detail, .native = native};
    return m_lastError;
}

SPI::Error SPI::clearError() {
    m_lastError = Error {};
    return m_lastError;
}

SPI::Error SPI::ensureStarted() {
    if (m_started) {
        return Error {};
    }

    return makeError(StdError::INVALID_STATE, Detail::NOT_STARTED, ESP_ERR_INVALID_STATE);
}

bool SPI::validDevice(size_t index) const {
    return index < m_deviceCnt && m_devices[index].handle != nullptr;
}

bool SPI::validBuffer(const uint8_t* data, size_t len) const {
    return len == 0 || data != nullptr;
}
