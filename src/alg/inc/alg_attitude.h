#pragma once

#include <cmath>

struct EulerAngles
{
    float pitch = 0.0f;
    float roll  = 0.0f;
    float yaw   = 0.0f;
};

struct Quaternion
{
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Orientation
{
    float      pitch = 0.0f;
    float      roll  = 0.0f;
    float      yaw   = 0.0f;
    Quaternion quat;
};

inline Quaternion eulerToQuaternion(float pitchDeg, float rollDeg, float yawDeg) {
    const float halfRad = 0.008726646f;
    const float cp      = std::cos(pitchDeg * halfRad);
    const float sp      = std::sin(pitchDeg * halfRad);
    const float cr      = std::cos(rollDeg * halfRad);
    const float sr      = std::sin(rollDeg * halfRad);
    const float cy      = std::cos(yawDeg * halfRad);
    const float sy      = std::sin(yawDeg * halfRad);

    Quaternion q;
    q.w = cr * cp * cy + sr * sp * sy;
    q.x = sr * cp * cy - cr * sp * sy;
    q.y = cr * sp * cy + sr * cp * sy;
    q.z = cr * cp * sy - sr * sp * cy;
    return q;
}

struct AttitudeEstimator
{
    float       alpha = 0.98f;
    EulerAngles angle;

    void configure(float a) {
        alpha = a;
    }

    void update(float gx, float gy, float gz, float ax, float ay, float az, float dtSec) {
        const float accelPitch = std::atan2(ax, std::sqrt(ay * ay + az * az)) * 57.29578f;
        const float accelRoll  = std::atan2(ay, std::sqrt(ax * ax + az * az)) * 57.29578f;

        const float gyroPitch = angle.pitch + gx * dtSec;
        const float gyroRoll  = angle.roll + gy * dtSec;
        const float gyroYaw   = angle.yaw + gz * dtSec;

        angle.pitch = alpha * gyroPitch + (1.0f - alpha) * accelPitch;
        angle.roll  = alpha * gyroRoll + (1.0f - alpha) * accelRoll;
        angle.yaw   = gyroYaw;
    }

    void reset() {
        angle = {};
    }
};
