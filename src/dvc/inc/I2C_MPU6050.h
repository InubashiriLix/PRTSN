#pragma once

#include "src/alg/inc/alg_attitude.h"
#include "src/alg/inc/alg_filter.h"
#include "src/alg/inc/vectors.h"
#include "src/fw/inc/IIC.h"

#include "freertos/projdefs.h"

#include <cstdint>

class I2C_MPU6050
{
public:
    static constexpr uint8_t MPU6050_ADDR_LOW  = 0x68;
    static constexpr uint8_t MPU6050_ADDR_HIGH = 0x69;

    static constexpr uint8_t REG_SMPLRT_DIV   = 0x19;
    static constexpr uint8_t REG_CONFIG       = 0x1A;
    static constexpr uint8_t REG_GYRO_CONFIG  = 0x1B;
    static constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
    static constexpr uint8_t REG_INT_ENABLE   = 0x38;
    static constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
    static constexpr uint8_t REG_PWR_MGMT_1   = 0x6B;
    static constexpr uint8_t REG_PWR_MGMT_2   = 0x6C;
    static constexpr uint8_t REG_WHO_AM_I     = 0x75;

    static constexpr TickType_t RESET_DELAY_TICKS = pdMS_TO_TICKS(100);
    static constexpr TickType_t WAKE_DELAY_TICKS  = pdMS_TO_TICKS(30);
    static constexpr uint8_t    FilterSmaWindow   = 4;

    enum class CalibrationState : uint8_t
    {
        NONE = 0,
        IN_PROGRESS,
        DONE,
        FAILED,
    };

    enum class Detail : uint8_t
    {
        NONE = 0,
        IIC_NOT_STARTED,
        ALREADY_STARTED,
        NOT_STARTED,
        INVALID_PARAM,
        INVALID_ADDRESS,
        DEVICE_NOT_FOUND,
        WHO_AM_I_READ_FAILED,
        WHO_AM_I_MISMATCH,
        RESET_FAILED,
        WAKE_FAILED,
        CONFIG_FAILED,
        READ_FAILED,
        NO_SAMPLE,
        CALIB_FAILED,
        CALIB_NOT_DONE,
    };

    struct Error
    {
        StdError   code   = StdError::OK;
        Detail     detail = Detail::NONE;
        IIC::Error iic {};

        constexpr bool ok() const noexcept {
            return code == StdError::OK && detail == Detail::NONE && iic.ok();
        }

        constexpr explicit operator bool() const noexcept {
            return ok();
        }
    };

    struct RawSample
    {
        int16_t accelX = 0;
        int16_t accelY = 0;
        int16_t accelZ = 0;
        int16_t temp   = 0;
        int16_t gyroX  = 0;
        int16_t gyroY  = 0;
        int16_t gyroZ  = 0;
    };

    struct Result3DStamped
    {
        Error                  error;
        Vector3DStamped<float> vec;

        Result3DStamped(Error err, TickType_t timestamp, float x, float y, float z)
            : error(err),
              vec({.timestamp = timestamp, .vector = {.x = x, .y = y, .z = z}}) {}
    };

    struct ResultFloatStamped
    {
        Error      error;
        TickType_t timestamp = 0;
        float      value     = 0;

        ResultFloatStamped(Error err, TickType_t timestamp, float value)
            : error(err),
              timestamp(timestamp),
              value(value) {}
    };

    enum class AccelRange : uint8_t
    {
        G2  = 0,
        G4  = 1,
        G8  = 2,
        G16 = 3,
    };

    enum class GyroRange : uint8_t
    {
        DPS250  = 0,
        DPS500  = 1,
        DPS1000 = 2,
        DPS2000 = 3,
    };

    struct FilterConfig
    {
        float    emaCutoffHz   = 2.0f;
        float    attitudeAlpha = 0.98f;
        uint16_t calibSamples  = 40;
        uint8_t  calibDelayMs  = 20;
    };

    struct Config
    {
        AccelRange   accelRange    = AccelRange::G2;
        GyroRange    gyroRange     = GyroRange::DPS250;
        uint8_t      dlpfCfg       = 3;
        uint8_t      sampleRateDiv = 9;
        bool         dataReadyInt  = false;
        FilterConfig filter {};
    };

public:
    explicit I2C_MPU6050(uint8_t address, IIC& iic);
    I2C_MPU6050(uint8_t address, IIC& iic, Config config);

    [[nodiscard]] Error setup();
    [[nodiscard]] Error readRawData();
    [[nodiscard]] Error calibrateGyro();

    [[nodiscard]] Result3DStamped    getAccel() const;
    [[nodiscard]] Result3DStamped    getGyro() const;
    [[nodiscard]] ResultFloatStamped getTemp() const;

    [[nodiscard]] Result3DStamped getFilteredAccel() const;
    [[nodiscard]] Result3DStamped getFilteredGyro() const;

    [[nodiscard]] Orientation getOrientation() const;
    [[nodiscard]] Quaternion  getQuaternion() const;
    [[nodiscard]] float       getPitch() const;
    [[nodiscard]] float       getRoll() const;
    [[nodiscard]] float       getYaw() const;

    [[nodiscard]] CalibrationState calibrationState() const;
    [[nodiscard]] bool             started() const;
    [[nodiscard]] Error            lastError() const;
    [[nodiscard]] TickType_t       lastUpdate() const;
    [[nodiscard]] RawSample        rawSample() const;

    static const char* detailName(Detail detail) noexcept;
    static const char* calibrationStateName(CalibrationState state) noexcept;

private:
    IIC&       m_iic;
    uint8_t    m_addr;
    Config     m_config {};
    bool       m_started   = false;
    bool       m_hasSample = false;
    Error      m_lastError {};
    RawSample  m_data {};
    TickType_t m_lastUpdate = 0;

    Vector3DSMAEMACascade<FilterSmaWindow> m_accelFilter;
    Vector3DSMAEMACascade<FilterSmaWindow> m_gyroFilter;
    AttitudeEstimator                      m_attitude;
    Vector3D<float>                        m_gyroBias {};
    CalibrationState                       m_calibState     = CalibrationState::NONE;
    TickType_t                             m_lastFilterTick = 0;

private:
    [[nodiscard]] Error makeError(StdError code, Detail detail, IIC::Error iic = {});
    [[nodiscard]] Error clearError();
    [[nodiscard]] Error mapIicError(IIC::Error iic, Detail detail);
    [[nodiscard]] Error writeChecked(uint8_t reg, uint8_t value, Detail detail);

    [[nodiscard]] float accelScale() const;
    [[nodiscard]] float gyroScale() const;

    [[nodiscard]] static bool    validAddress(uint8_t address);
    [[nodiscard]] static int16_t be16(uint8_t hi, uint8_t lo);

    void applyFilters();
};
