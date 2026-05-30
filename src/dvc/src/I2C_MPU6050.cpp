#include "src/dvc/inc/I2C_MPU6050.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

I2C_MPU6050::I2C_MPU6050(uint8_t address, IIC& iic) : I2C_MPU6050(address, iic, Config {}) {}

I2C_MPU6050::I2C_MPU6050(uint8_t address, IIC& iic, Config config)
    : m_iic(iic),
      m_addr(address),
      m_config(config) {}

I2C_MPU6050::Error I2C_MPU6050::setup() {
    if (m_started)
        return makeError(StdError::INVALID_STATE, Detail::ALREADY_STARTED);
    if (!validAddress(m_addr))
        return makeError(StdError::INVALID_ARGS, Detail::INVALID_ADDRESS);
    if (!m_iic.started())
        return makeError(StdError::INVALID_STATE, Detail::IIC_NOT_STARTED);

    IIC::Error iicErr = m_iic.devicePresent(m_addr);
    if (!iicErr)
        return mapIicError(iicErr, Detail::DEVICE_NOT_FOUND);

    uint8_t whoAmI = 0;
    iicErr         = m_iic.readRegister(m_addr, REG_WHO_AM_I, &whoAmI, 1);
    if (!iicErr)
        return mapIicError(iicErr, Detail::WHO_AM_I_READ_FAILED);
    if (whoAmI != MPU6050_ADDR_LOW)
        return makeError(StdError::INVALID_RESPONSE, Detail::WHO_AM_I_MISMATCH);

    Error err = writeChecked(REG_PWR_MGMT_1, 0x80, Detail::RESET_FAILED);
    if (!err) {
        return err;
    }
    vTaskDelay(RESET_DELAY_TICKS);

    err = writeChecked(REG_PWR_MGMT_1, 0x01, Detail::WAKE_FAILED);
    if (!err)
        return err;
    vTaskDelay(WAKE_DELAY_TICKS);

    err = writeChecked(REG_CONFIG, static_cast<uint8_t>(m_config.dlpfCfg & 0x07), Detail::CONFIG_FAILED);
    if (!err)
        return err;

    err = writeChecked(REG_SMPLRT_DIV, m_config.sampleRateDiv, Detail::CONFIG_FAILED);
    if (!err)
        return err;

    err = writeChecked(REG_GYRO_CONFIG, static_cast<uint8_t>(static_cast<uint8_t>(m_config.gyroRange) << 3), Detail::CONFIG_FAILED);
    if (!err)
        return err;

    err = writeChecked(REG_ACCEL_CONFIG, static_cast<uint8_t>(static_cast<uint8_t>(m_config.accelRange) << 3), Detail::CONFIG_FAILED);
    if (!err)
        return err;

    err = writeChecked(REG_INT_ENABLE, m_config.dataReadyInt ? 0x01 : 0x00, Detail::CONFIG_FAILED);
    if (!err)
        return err;

    m_started = true;
    return clearError();
}

I2C_MPU6050::Error I2C_MPU6050::readRawData() {
    if (!m_started) {
        return makeError(StdError::INVALID_STATE, Detail::NOT_STARTED);
    }

    uint8_t    buf[sizeof(RawSample)] {};
    IIC::Error iicErr = m_iic.readRegister(m_addr, REG_ACCEL_XOUT_H, buf, sizeof(buf));
    if (!iicErr) {
        return mapIicError(iicErr, Detail::READ_FAILED);
    }

    m_data.accelX = be16(buf[0], buf[1]);
    m_data.accelY = be16(buf[2], buf[3]);
    m_data.accelZ = be16(buf[4], buf[5]);
    m_data.temp   = be16(buf[6], buf[7]);
    m_data.gyroX  = be16(buf[8], buf[9]);
    m_data.gyroY  = be16(buf[10], buf[11]);
    m_data.gyroZ  = be16(buf[12], buf[13]);

    m_lastUpdate = xTaskGetTickCount();
    m_hasSample  = true;
    return clearError();
}

I2C_MPU6050::Result3DStamped I2C_MPU6050::getAccel() const {
    const Error err = !m_started     ? Error {.code = StdError::INVALID_STATE, .detail = Detail::NOT_STARTED}
                      : !m_hasSample ? Error {.code = StdError::NOT_FINISHED, .detail = Detail::NO_SAMPLE}
                                     : m_lastError;
    if (!err) {
        return {err, m_lastUpdate, 0, 0, 0};
    }

    const float scale = accelScale();
    return {err, m_lastUpdate, m_data.accelX / scale, m_data.accelY / scale, m_data.accelZ / scale};
}

I2C_MPU6050::Result3DStamped I2C_MPU6050::getGyro() const {
    const Error err = !m_started     ? Error {.code = StdError::INVALID_STATE, .detail = Detail::NOT_STARTED}
                      : !m_hasSample ? Error {.code = StdError::NOT_FINISHED, .detail = Detail::NO_SAMPLE}
                                     : m_lastError;
    if (!err) {
        return {err, m_lastUpdate, 0, 0, 0};
    }

    const float scale = gyroScale();
    return {err, m_lastUpdate, m_data.gyroX / scale, m_data.gyroY / scale, m_data.gyroZ / scale};
}

I2C_MPU6050::ResultFloatStamped I2C_MPU6050::getTemp() const {
    const Error err = !m_started     ? Error {.code = StdError::INVALID_STATE, .detail = Detail::NOT_STARTED}
                      : !m_hasSample ? Error {.code = StdError::NOT_FINISHED, .detail = Detail::NO_SAMPLE}
                                     : m_lastError;
    if (!err) {
        return {err, m_lastUpdate, 0};
    }

    return {err, m_lastUpdate, m_data.temp / 340.0f + 36.53f};
}

bool I2C_MPU6050::started() const {
    return m_started;
}

I2C_MPU6050::Error I2C_MPU6050::lastError() const {
    return m_lastError;
}

TickType_t I2C_MPU6050::lastUpdate() const {
    return m_lastUpdate;
}

I2C_MPU6050::RawSample I2C_MPU6050::rawSample() const {
    return m_data;
}

const char* I2C_MPU6050::detailName(Detail detail) noexcept {
    switch (detail) {
        case Detail::NONE:
            return "NONE";
        case Detail::IIC_NOT_STARTED:
            return "IIC_NOT_STARTED";
        case Detail::ALREADY_STARTED:
            return "ALREADY_STARTED";
        case Detail::NOT_STARTED:
            return "NOT_STARTED";
        case Detail::INVALID_PARAM:
            return "INVALID_PARAM";
        case Detail::INVALID_ADDRESS:
            return "INVALID_ADDRESS";
        case Detail::DEVICE_NOT_FOUND:
            return "DEVICE_NOT_FOUND";
        case Detail::WHO_AM_I_READ_FAILED:
            return "WHO_AM_I_READ_FAILED";
        case Detail::WHO_AM_I_MISMATCH:
            return "WHO_AM_I_MISMATCH";
        case Detail::RESET_FAILED:
            return "RESET_FAILED";
        case Detail::WAKE_FAILED:
            return "WAKE_FAILED";
        case Detail::CONFIG_FAILED:
            return "CONFIG_FAILED";
        case Detail::READ_FAILED:
            return "READ_FAILED";
        case Detail::NO_SAMPLE:
            return "NO_SAMPLE";
    }

    return "UNKNOWN";
}

I2C_MPU6050::Error I2C_MPU6050::makeError(StdError code, Detail detail, IIC::Error iic) {
    m_lastError = Error {.code = code, .detail = detail, .iic = iic};
    return m_lastError;
}

I2C_MPU6050::Error I2C_MPU6050::clearError() {
    m_lastError = Error {};
    return m_lastError;
}

I2C_MPU6050::Error I2C_MPU6050::mapIicError(IIC::Error iic, Detail detail) {
    return makeError(iic.code, detail, iic);
}

I2C_MPU6050::Error I2C_MPU6050::writeChecked(uint8_t reg, uint8_t value, Detail detail) {
    IIC::Error iicErr = m_iic.writeRegister(m_addr, reg, value);
    if (!iicErr) {
        return mapIicError(iicErr, detail);
    }

    return Error {};
}

float I2C_MPU6050::accelScale() const {
    switch (m_config.accelRange) {
        case AccelRange::G2:
            return 16384.0f;
        case AccelRange::G4:
            return 8192.0f;
        case AccelRange::G8:
            return 4096.0f;
        case AccelRange::G16:
            return 2048.0f;
    }

    return 16384.0f;
}

float I2C_MPU6050::gyroScale() const {
    switch (m_config.gyroRange) {
        case GyroRange::DPS250:
            return 131.0f;
        case GyroRange::DPS500:
            return 65.5f;
        case GyroRange::DPS1000:
            return 32.8f;
        case GyroRange::DPS2000:
            return 16.4f;
    }

    return 131.0f;
}

bool I2C_MPU6050::validAddress(uint8_t address) {
    return address == MPU6050_ADDR_LOW || address == MPU6050_ADDR_HIGH;
}

int16_t I2C_MPU6050::be16(uint8_t hi, uint8_t lo) {
    return static_cast<int16_t>((static_cast<uint16_t>(hi) << 8) | lo);
}
