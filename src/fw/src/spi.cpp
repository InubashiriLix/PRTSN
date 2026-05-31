#include "src/fw/inc/spi.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "src/fw/inc/std_err.h"
#include <cstring>

SPI::SPI(BusConfig& busConfig)
    : m_busConfig(busConfig),
      m_deviceCnt(0) {
}

const SPI::Error SPI::setupBus() {
    const auto err = spi_bus_initialize(m_busConfig.host, &m_busConfig.bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        return {.code = StdError::FAIL, .detail = Detail::PARAM_CONFIG_FAILED, .native = err};
    }

    m_started = true;
    return {.code = StdError::OK, .detail = Detail::NONE, .native = ESP_OK};
}

const SPI::Error SPI::end() {
    for (size_t i = 0; i < m_deviceCnt; ++i) {
        spi_bus_remove_device(m_devices[i].handle);
    }
    m_deviceCnt = 0;

    if (!m_started) {
        return {.code = StdError::OK, .detail = Detail::NONE, .native = ESP_OK};
    }

    const auto err = spi_bus_free(m_busConfig.host);
    if (err != ESP_OK) {
        return {.code = toStdErr(err), .detail = Detail::DRIVER_DELETE_FAILED, .native = err};
    }

    m_started = false;
    return {.code = StdError::OK, .detail = Detail::NONE, .native = ESP_OK};
}

const SPI::Error SPI::addDevice(const DeviceConfig& deviceConfig) {
    if (!m_started) {
        return {.code   = StdError::FAIL,
                .detail = Detail::NOT_STARTED,
                .native = ESP_FAIL};
    }

    if (m_deviceCnt >= DefaultMaxDeviceNum) {
        return {
            .code   = StdError::FAIL,
            .detail = Detail::DEVICE_FULL,
            .native = ESP_FAIL,
        };
    }

    DeviceConfig& slot = m_devices[m_deviceCnt];
    slot               = deviceConfig;
    slot.handle        = nullptr;

    const auto err = spi_bus_add_device(m_busConfig.host, &deviceConfig.device, &slot.handle);
    if (err != ESP_OK) {
        return {.code = toStdErr(err), .detail = Detail::PARAM_CONFIG_FAILED, .native = err};
    }

    slot.index = m_deviceCnt;
    ++m_deviceCnt;

    return {.code = StdError::OK, .detail = Detail::NONE, .native = ESP_OK};
}

const SPI::Error SPI::removeDevice(size_t index) {
    if (!m_started)
        return {.code = StdError::FAIL, .detail = Detail::NOT_STARTED, .native = ESP_FAIL};

    if (index >= m_deviceCnt) {
        return {.code = StdError::FAIL, .detail = Detail::PARAM_CONFIG_FAILED, .native = ESP_FAIL};
    }

    const auto err = spi_bus_remove_device(m_devices[index].handle);

    if (err != ESP_OK) {
        return {.code = toStdErr(err), .detail = Detail::DRIVER_DELETE_FAILED, .native = err};
    }

    return {.code = StdError::OK, .detail = Detail::NONE, .native = ESP_OK};
}

const SPI::Error SPI::transmit(size_t dvcIndex, const Transaction& transaction) {
    if (!m_started)
        return {.code = StdError::FAIL, .detail = Detail::NOT_STARTED, .native = ESP_FAIL};

    if (dvcIndex >= m_deviceCnt || m_devices[dvcIndex].handle == nullptr) {
        return {.code = StdError::FAIL, .detail = Detail::PROBE_FAILED, .native = ESP_FAIL};
    }

    if (transaction.length == 0) {
        return {.code = StdError::OK, .detail = Detail::NONE, .native = ESP_OK};
    }

    spi_transaction_t espTrans {};
    espTrans.length = transaction.length * 8;

    if (transaction.rxOnly) {
        espTrans.rx_buffer = transaction.rxData;
        espTrans.flags     = SPI_TRANS_USE_TXDATA;
        for (int i = 0; i < 4; ++i)
            espTrans.tx_data[i] = 0xFF;
    }
    else if (transaction.txData != nullptr && transaction.rxData != nullptr) {
        espTrans.tx_buffer = transaction.txData;
        espTrans.rx_buffer = transaction.rxData;
    }
    else if (transaction.txData != nullptr) {
        espTrans.tx_buffer = transaction.txData;
    }
    else if (transaction.rxData != nullptr) {
        espTrans.rx_buffer = transaction.rxData;
    }

    const auto err = spi_device_polling_transmit(m_devices[dvcIndex].handle, &espTrans);
    if (err != ESP_OK) {
        return {.code = toStdErr(err), .detail = Detail::WRITE_FAILED, .native = err};
    }

    return {.code = StdError::OK, .detail = Detail::NONE, .native = ESP_OK};
}

const SPI::Error SPI::write(size_t dvcIndex, const uint8_t* data, size_t len) {
    return transmit(dvcIndex, {.txData = data, .length = len});
}

const SPI::Error SPI::read(size_t dvcIndex, uint8_t* buf, size_t len) {
    return transmit(dvcIndex, {.rxData = buf, .length = len, .rxOnly = true});
}

const SPI::Error SPI::writeRegister(size_t dvcIndex, uint8_t reg, uint8_t value) {
    if (!m_started)
        return {.code = StdError::FAIL, .detail = Detail::NOT_STARTED, .native = ESP_FAIL};

    if (dvcIndex >= m_deviceCnt || m_devices[dvcIndex].handle == nullptr) {
        return {.code = StdError::FAIL, .detail = Detail::PROBE_FAILED, .native = ESP_FAIL};
    }

    uint8_t buf[] {reg, value};

    spi_transaction_t espTrans {};
    espTrans.length    = sizeof(buf) * 8;
    espTrans.tx_buffer = buf;

    const auto err = spi_device_polling_transmit(m_devices[dvcIndex].handle, &espTrans);
    if (err != ESP_OK) {
        return {.code = toStdErr(err), .detail = Detail::WRITE_READ_FAILED, .native = err};
    }

    return {.code = StdError::OK, .detail = Detail::NONE, .native = ESP_OK};
}

const SPI::Error SPI::readRegister(size_t dvcIndex, uint8_t reg, uint8_t* buf, size_t len) {
    if (!m_started)
        return {.code = StdError::FAIL, .detail = Detail::NOT_STARTED, .native = ESP_FAIL};

    if (dvcIndex >= m_deviceCnt || m_devices[dvcIndex].handle == nullptr) {
        return {.code = StdError::FAIL, .detail = Detail::PROBE_FAILED, .native = ESP_FAIL};
    }

    if (buf == nullptr || len == 0) {
        return {.code = StdError::FAIL, .detail = Detail::INVALID_BUFFER, .native = ESP_FAIL};
    }

    static constexpr size_t MaxFrame = 64;
    uint8_t                 txBuf[MaxFrame] {};
    uint8_t                 rxBuf[MaxFrame] {};
    const size_t            frameSize = 1 + len;

    if (frameSize > MaxFrame) {
        return {.code = StdError::FAIL, .detail = Detail::INVALID_BUFFER, .native = ESP_FAIL};
    }

    txBuf[0] = reg;

    spi_transaction_t espTrans {};
    espTrans.length    = frameSize * 8;
    espTrans.tx_buffer = txBuf;
    espTrans.rx_buffer = rxBuf;

    const auto err = spi_device_polling_transmit(m_devices[dvcIndex].handle, &espTrans);
    if (err != ESP_OK) {
        return {.code = toStdErr(err), .detail = Detail::WRITE_READ_FAILED, .native = err};
    }

    std::memcpy(buf, rxBuf + 1, len);
    return {.code = StdError::OK, .detail = Detail::NONE, .native = ESP_OK};
}
