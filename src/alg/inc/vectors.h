#pragma once

#include "portmacro.h"
#include <cmath>

template <typename T>
struct Vector2D
{
    T x = 0;
    T y = 0;
    T magnitude() const {
        return std::sqrt(x * x + y * y);
    }
};

template <typename T>
struct Vector2DStamped
{
    TickType_t  timestamp = 0;
    Vector2D<T> vector {};
};

template <typename T>
struct Vector3D
{
    T x = 0;
    T y = 0;
    T z = 0;

    T magnitude() const {
        return std::sqrt(x * x + y * y + z * z);
    }
};

template <typename T>
struct Vector3DStamped
{
    TickType_t  timestamp = 0;
    Vector3D<T> vector {};
};

template <typename T, int N>
struct VectorN
{
    T data[N] = {};

    T magnitude() const {
        T sum = 0;
        for (int i = 0; i < N; ++i) {
            sum += data[i] * data[i];
        }
        return std::sqrt(sum);
    }
};
