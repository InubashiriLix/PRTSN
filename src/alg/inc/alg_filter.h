#pragma once

#include "src/alg/inc/vectors.h"

#include <cmath>
#include <cstdint>

struct EMAFilter
{
    float tau    = 0.0f;
    float value  = 0.0f;
    bool  primed = false;

    void configure(float cutoffHz) {
        tau = 1.0f / (2.0f * 3.14159265f * cutoffHz);
    }

    float updateDt(float raw, float dtSeconds) {
        if (!primed) {
            value  = raw;
            primed = true;
            return value;
        }
        const float a = 1.0f - std::exp(-dtSeconds / tau);
        value         = a * raw + (1.0f - a) * value;
        return value;
    }

    float current() const {
        return value;
    }

    void reset() {
        value  = 0.0f;
        primed = false;
    }

private:
    float alpha = 0.0f;
};

template <uint8_t N>
struct SMAFilter
{
    static_assert(N > 1, "SMA window must be > 1");

    float   buf[N] {};
    float   sum   = 0.0f;
    uint8_t count = 0;
    uint8_t pos   = 0;

    float update(float raw) {
        if (count < N) {
            buf[count++] = raw;
            sum += raw;
            return sum / static_cast<float>(count);
        }
        sum -= buf[pos];
        buf[pos] = raw;
        sum += raw;
        pos = static_cast<uint8_t>((pos + 1) % N);
        return sum / static_cast<float>(N);
    }

    float current() const {
        if (count == 0) {
            return 0.0f;
        }
        return sum / static_cast<float>(count);
    }

    bool full() const {
        return count >= N;
    }

    void reset() {
        sum   = 0.0f;
        count = 0;
        pos   = 0;
    }
};

template <uint8_t N>
struct Vector3DSMA
{
    SMAFilter<N> x, y, z;

    Vector3D<float> update(float rawX, float rawY, float rawZ) {
        return {x.update(rawX), y.update(rawY), z.update(rawZ)};
    }

    Vector3D<float> current() const {
        return {x.current(), y.current(), z.current()};
    }

    bool full() const {
        return x.full();
    }

    void reset() {
        x.reset();
        y.reset();
        z.reset();
    }
};

template <uint8_t N>
struct SMAEMACascade
{
    SMAFilter<N> sma;
    EMAFilter    ema;

    void configure(float cutoffHz) {
        ema.configure(cutoffHz);
    }

    float update(float raw, float dtSeconds) {
        return ema.updateDt(sma.update(raw), dtSeconds);
    }

    float current() const {
        return ema.current();
    }

    bool smaFull() const {
        return sma.full();
    }

    void reset() {
        sma.reset();
        ema.reset();
    }
};

struct Vector3DEMA
{
    EMAFilter x, y, z;

    void configure(float cutoffHz) {
        x.configure(cutoffHz);
        y.configure(cutoffHz);
        z.configure(cutoffHz);
    }

    Vector3D<float> updateDt(float rawX, float rawY, float rawZ, float dtSeconds) {
        return {x.updateDt(rawX, dtSeconds), y.updateDt(rawY, dtSeconds), z.updateDt(rawZ, dtSeconds)};
    }

    Vector3D<float> current() const {
        return {x.current(), y.current(), z.current()};
    }

    void reset() {
        x.reset();
        y.reset();
        z.reset();
    }
};

template <uint8_t N>
struct Vector3DSMAEMACascade
{
    Vector3DSMA<N> sma;
    Vector3DEMA    ema;

    void configure(float cutoffHz) {
        ema.configure(cutoffHz);
    }

    Vector3D<float> update(float rawX, float rawY, float rawZ, float dtSeconds) {
        const auto smaVal = sma.update(rawX, rawY, rawZ);
        return ema.updateDt(smaVal.x, smaVal.y, smaVal.z, dtSeconds);
    }

    Vector3D<float> current() const {
        return ema.current();
    }

    bool smaFull() const {
        return sma.full();
    }

    void reset() {
        sma.reset();
        ema.reset();
    }
};
