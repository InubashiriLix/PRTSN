#include "src/fw/inc/IIC.h"

#include "src/cfg/BuildConfig.h"

#if PRTN_ENABLE_IIC

#include "driver/i2c.h"

IIC::IIC() : IIC(Config {}) {}

IIC::IIC(Config config) : m_config(config) {}

IIC::IIC(uint8_t sdaPin, uint8_t sclPin, uint32_t frequency, i2c_port_t port)
    : IIC(makeMasterConfig(sdaPin, sclPin, frequency, port)) {}

IIC::~IIC() {
    end();
}

IIC::Error IIC::setup() {
    if (m_started) {
        return makeError(StdError::INVALID_STATE, Detail::ALREADY_STARTED, ESP_ERR_INVALID_STATE);
    }

    if (m_config.driverCfg.mode != I2C_MODE_MASTER) {
        return makeError(StdError::INVALID_ARGS, Detail::PARAM_CONFIG_FAILED, ESP_ERR_INVALID_ARG);
    }

    esp_err_t err = i2c_param_config(m_config.port, &m_config.driverCfg);
    if (err != ESP_OK) {
        return makeError(toStdErr(err), Detail::PARAM_CONFIG_FAILED, err);
    }

    err = i2c_driver_install(
        m_config.port,
        m_config.driverCfg.mode,
        m_config.slvRxBufSize,
        m_config.slvTxBufSize,
        m_config.intrAllocFlags);
    if (err != ESP_OK) {
        return makeError(toStdErr(err), Detail::DRIVER_INSTALL_FAILED, err);
    }

    m_started = true;
    return clearError();
}

IIC::Error IIC::end() {
    if (!m_started) {
        return clearError();
    }

    const esp_err_t err = i2c_driver_delete(m_config.port);
    if (err != ESP_OK) {
        return makeError(toStdErr(err), Detail::DRIVER_DELETE_FAILED, err);
    }

    m_started = false;
    return clearError();
}

IIC::Error IIC::write(uint8_t address, const uint8_t* data, size_t length) {
    Error err = ensureStarted();
    if (!err) {
        return err;
    }

    if (!validBuffer(data, length)) {
        return makeError(StdError::INVALID_ARGS, Detail::INVALID_BUFFER, ESP_ERR_INVALID_ARG, address, length);
    }

    if (length == 0) {
        return devicePresent(address);
    }

    const esp_err_t native = i2c_master_write_to_device(m_config.port, address, data, length, timeoutTicks());
    if (native != ESP_OK) {
        return makeError(toStdErr(native), Detail::WRITE_FAILED, native, address, length);
    }

    return clearError(address, length);
}

IIC::Error IIC::write(uint8_t address, const WriteBuffer* buffers, size_t count) {
    Error err = ensureStarted();
    if (!err) {
        return err;
    }

    if (buffers == nullptr || count == 0) {
        return makeError(StdError::INVALID_ARGS, Detail::INVALID_BUFFER, ESP_ERR_INVALID_ARG, address);
    }

    size_t totalBytes = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!validBuffer(buffers[i].data, buffers[i].length)) {
            return makeError(StdError::INVALID_ARGS, Detail::INVALID_BUFFER, ESP_ERR_INVALID_ARG, address, totalBytes);
        }
        totalBytes += buffers[i].length;
    }

    if (totalBytes == 0) {
        return devicePresent(address);
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == nullptr) {
        return makeError(StdError::NO_MEM, Detail::WRITE_FAILED, ESP_ERR_NO_MEM, address, totalBytes);
    }

    esp_err_t native = i2c_master_start(cmd);
    if (native == ESP_OK) {
        native = i2c_master_write_byte(cmd, static_cast<uint8_t>((address << 1) | I2C_MASTER_WRITE), true);
    }

    for (size_t i = 0; native == ESP_OK && i < count; ++i) {
        if (buffers[i].length == 0) {
            continue;
        }
        native = i2c_master_write(cmd, const_cast<uint8_t*>(buffers[i].data), buffers[i].length, true);
    }

    if (native == ESP_OK) {
        native = i2c_master_stop(cmd);
    }
    if (native == ESP_OK) {
        native = i2c_master_cmd_begin(m_config.port, cmd, timeoutTicks());
    }

    i2c_cmd_link_delete(cmd);

    if (native != ESP_OK) {
        return makeError(toStdErr(native), Detail::WRITE_FAILED, native, address, totalBytes);
    }

    return clearError(address, totalBytes);
}

IIC::Error IIC::read(uint8_t address, uint8_t* buffer, size_t length) {
    Error err = ensureStarted();
    if (!err) {
        return err;
    }

    if (!validBuffer(buffer, length)) {
        return makeError(StdError::INVALID_ARGS, Detail::INVALID_BUFFER, ESP_ERR_INVALID_ARG, address, length);
    }

    if (length == 0) {
        return clearError(address, 0);
    }

    const esp_err_t native = i2c_master_read_from_device(m_config.port, address, buffer, length, timeoutTicks());
    if (native != ESP_OK) {
        return makeError(toStdErr(native), Detail::READ_FAILED, native, address, length);
    }

    return clearError(address, length);
}

IIC::Error IIC::writeRegister(uint8_t address, uint8_t reg, uint8_t value) {
    return writeRegister(address, reg, &value, 1);
}

IIC::Error IIC::writeRegister(uint8_t address, uint8_t reg, const uint8_t* data, size_t length) {
    Error err = ensureStarted();
    if (!err) {
        return err;
    }

    if (!validBuffer(data, length)) {
        return makeError(StdError::INVALID_ARGS, Detail::INVALID_BUFFER, ESP_ERR_INVALID_ARG, address, length);
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == nullptr) {
        return makeError(StdError::NO_MEM, Detail::WRITE_FAILED, ESP_ERR_NO_MEM, address, length);
    }

    esp_err_t native = i2c_master_start(cmd);
    if (native == ESP_OK) {
        native = i2c_master_write_byte(cmd, static_cast<uint8_t>((address << 1) | I2C_MASTER_WRITE), true);
    }
    if (native == ESP_OK) {
        native = i2c_master_write_byte(cmd, reg, true);
    }
    if (native == ESP_OK && length > 0) {
        native = i2c_master_write(cmd, const_cast<uint8_t*>(data), length, true);
    }
    if (native == ESP_OK) {
        native = i2c_master_stop(cmd);
    }
    if (native == ESP_OK) {
        native = i2c_master_cmd_begin(m_config.port, cmd, timeoutTicks());
    }

    i2c_cmd_link_delete(cmd);

    if (native != ESP_OK) {
        return makeError(toStdErr(native), Detail::WRITE_FAILED, native, address, length + 1);
    }

    return clearError(address, length + 1);
}

IIC::Error IIC::readRegister(uint8_t address, uint8_t reg, uint8_t* buffer, size_t length) {
    Error err = ensureStarted();
    if (!err) {
        return err;
    }

    if (!validBuffer(buffer, length)) {
        return makeError(StdError::INVALID_ARGS, Detail::INVALID_BUFFER, ESP_ERR_INVALID_ARG, address, length);
    }

    if (length == 0) {
        return clearError(address, 0);
    }

    const esp_err_t native = i2c_master_write_read_device(
        m_config.port,
        address,
        &reg,
        1,
        buffer,
        length,
        timeoutTicks());
    if (native != ESP_OK) {
        return makeError(toStdErr(native), Detail::WRITE_READ_FAILED, native, address, length);
    }

    return clearError(address, length);
}

IIC::Error IIC::devicePresent(uint8_t address) {
    Error err = ensureStarted();
    if (!err) {
        return err;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == nullptr) {
        return makeError(StdError::NO_MEM, Detail::PROBE_FAILED, ESP_ERR_NO_MEM, address);
    }

    esp_err_t native = i2c_master_start(cmd);
    if (native == ESP_OK) {
        native = i2c_master_write_byte(cmd, static_cast<uint8_t>((address << 1) | I2C_MASTER_WRITE), true);
    }
    if (native == ESP_OK) {
        native = i2c_master_stop(cmd);
    }
    if (native == ESP_OK) {
        native = i2c_master_cmd_begin(m_config.port, cmd, timeoutTicks());
    }

    i2c_cmd_link_delete(cmd);

    if (native != ESP_OK) {
        return makeError(toStdErr(native), Detail::PROBE_FAILED, native, address);
    }

    return clearError(address);
}

uint8_t IIC::scan(uint8_t* addresses, size_t maxCount, uint8_t first, uint8_t last) {
    Error err = ensureStarted();
    if (!err) {
        return 0;
    }

    if (addresses == nullptr || maxCount == 0 || first > last) {
        makeError(StdError::INVALID_ARGS, Detail::INVALID_BUFFER, ESP_ERR_INVALID_ARG);
        return 0;
    }

    uint8_t count = 0;
    for (uint8_t address = first; address <= last && count < maxCount; ++address) {
        if (devicePresent(address)) {
            addresses[count++] = address;
        }
    }

    return count;
}

bool IIC::started() const {
    return m_started;
}

IIC::Error IIC::lastError() const {
    return m_lastError;
}

const IIC::Config& IIC::config() const {
    return m_config;
}

const char* IIC::detailName(Detail detail) noexcept {
    switch (detail) {
        case Detail::NONE:
            return "NONE";
        case Detail::NOT_STARTED:
            return "NOT_STARTED";
        case Detail::ALREADY_STARTED:
            return "ALREADY_STARTED";
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
    }

    return "UNKNOWN";
}

IIC::Error IIC::makeError(StdError code, Detail detail, esp_err_t native, uint8_t address, size_t bytes) {
    m_lastError = Error {
        .code    = code,
        .detail  = detail,
        .native  = native,
        .address = address,
        .bytes   = bytes,
    };

    return m_lastError;
}

IIC::Error IIC::clearError(uint8_t address, size_t bytes) {
    m_lastError = Error {
        .code    = StdError::OK,
        .detail  = Detail::NONE,
        .native  = ESP_OK,
        .address = address,
        .bytes   = bytes,
    };

    return m_lastError;
}

IIC::Error IIC::ensureStarted() {
    if (m_started) {
        return Error {};
    }

    return makeError(StdError::INVALID_STATE, Detail::NOT_STARTED, ESP_ERR_INVALID_STATE);
}

bool IIC::validBuffer(const uint8_t* data, size_t length) {
    return length == 0 || data != nullptr;
}

TickType_t IIC::timeoutTicks() const {
    return pdMS_TO_TICKS(m_config.timeoutMs);
}

IIC::Config IIC::makeMasterConfig(uint8_t sdaPin, uint8_t sclPin, uint32_t frequency, i2c_port_t port) {
    Config config {};
    config.port                       = port;
    config.driverCfg.mode             = I2C_MODE_MASTER;
    config.driverCfg.sda_io_num       = sdaPin;
    config.driverCfg.scl_io_num       = sclPin;
    config.driverCfg.sda_pullup_en    = true;
    config.driverCfg.scl_pullup_en    = true;
    config.driverCfg.master.clk_speed = frequency;
    config.driverCfg.clk_flags        = 0;
    config.timeoutMs                  = DefaultTimeoutMs;
    return config;
}

#endif
