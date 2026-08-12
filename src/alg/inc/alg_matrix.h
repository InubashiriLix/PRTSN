#pragma once
#include "src/fw/inc/Result.h"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <type_traits>
#include "src/alg/inc/alg_err.h"

template <size_t Rows, size_t Cols, typename T>
struct Matrix
{
    static_assert(Rows > 0 && Cols > 0, "Matrix dimensions must be greater than zero");
    static_assert(std::is_trivially_copyable_v<T>,
                  "Matrix requires trivially copyable T because Result<T, E> stores values directly");

    T data[Rows][Cols];

    explicit Matrix(const T& initialValue = T()) {
        fill(initialValue);
    }

    explicit Matrix(const T* initialArray, size_t len) {
        if (initialArray != nullptr && len == Rows * Cols) {
            std::copy(initialArray, initialArray + len, begin());
        }
        else {
            fill(T());
        }
    }

    virtual ~Matrix() = default;

    Result<T, MatrixError> get(size_t i, size_t j) const {
        return getCopy(i, j);
    }

    [[deprecated("getPtr exposes internal storage; prefer get(), getCopy(), set(), or replace().")]]
    Result<T*, MatrixError> getPtr(size_t i, size_t j) {
        if (checkOutofBoundary(i, j)) {
            return Err<MatrixErrorCode::INDEX_OUT_OF_BOUNDS>("Matrix index is out of bounds");
        }
        return Ok(&data[i][j]);
    }

    [[nodiscard]] Result<const T*, MatrixError> getRef(size_t i, size_t j) const {
        if (checkOutofBoundary(i, j)) {
            return Err<MatrixErrorCode::INDEX_OUT_OF_BOUNDS>("Matrix index is out of bounds");
        }
        return Ok(&data[i][j]);
    }

    Result<T, MatrixError> getCopy(size_t i, size_t j) const {
        if (checkOutofBoundary(i, j)) {
            return Err<MatrixErrorCode::INDEX_OUT_OF_BOUNDS>("Matrix index is out of bounds");
        }
        return Ok(data[i][j]);
    }

    Result<T, MatrixError> set(size_t i, size_t j, const T value) { // move or copy dependending on user's hehaviour
        auto temp = getCopy(i, j);
        if (temp.is_err()) {
            return temp.propagate();
        }
        data[i][j] = value;
        return Ok(temp.unwrap());
    }

    // make sure you're pass in copy.
    Result<void, MatrixError> fill(const T value) {
        std::fill(&data[0][0], &data[0][0] + Rows * Cols, value);
        return Ok();
    }

    virtual Result<T, MatrixError> remove(size_t i, size_t j) { // Users can override for custom remove behavior.
        return set(i, j, T());
    }

    Result<T, MatrixError> replace(size_t i, size_t j, const T value) {
        return set(i, j, value);
    }

    Result<void, MatrixError> swap(size_t i1, size_t j1, size_t i2, size_t j2) {
        auto temp1 = getCopy(i1, j1);
        if (temp1.is_err()) {
            return temp1.propagate();
        }
        auto temp2 = getCopy(i2, j2);
        if (temp2.is_err()) {
            return temp2.propagate();
        }
        data[i1][j1] = temp2.unwrap();
        data[i2][j2] = temp1.unwrap();
        return Ok();
    }

    void clear() {
        std::memset(&data, 0, Rows * Cols * sizeof(T));
    }

    constexpr size_t getRows() const {
        return Rows;
    }

    constexpr size_t getCols() const {
        return Cols;
    }

    static inline bool checkOutofBoundary(size_t i, size_t j) {
        return (i >= Rows || j >= Cols);
    }

    // clang-format off
    // =============== imple the default iterator =================
    T* begin() { return &data[0][0]; }
    T* end() { return &data[0][0] + Rows * Cols; }
    const T* begin() const { return &data[0][0]; }
    const T* end() const { return &data[0][0] + Rows * Cols; }
    // =================== default iterator end ===================

    // ===================== proxy iterators ======================
    // === row iterator ===
    struct RowIterator {
        T* ptr;
        T& operator*() { return *ptr; }
        void operator++() { ++ptr; }
        bool operator!=(const RowIterator& other) const { return ptr != other.ptr; }
    };
    struct RowRange {
        T* start;
        size_t len;
        RowIterator begin() { return {start}; }
        RowIterator end() { return {start + len}; }
    };
    struct ConstRowIterator {
        const T* ptr;
        const T& operator*() const { return *ptr; }
        void operator++() { ++ptr; }
        bool operator!=(const ConstRowIterator& other) const { return ptr != other.ptr; }
    };
    struct ConstRowRange {
        const T* start;
        size_t len;
        ConstRowIterator begin() const { return {start}; }
        ConstRowIterator end() const { return {start + len}; }
    };
    RowRange row(size_t r) {
        return r < Rows ? RowRange{&data[r][0], Cols} : RowRange{end(), 0};
    }
    ConstRowRange row(size_t r) const {
        return r < Rows ? ConstRowRange{&data[r][0], Cols} : ConstRowRange{end(), 0};
    }
    // === col iterator ===
    struct ColIterator {
        T* ptr;
        size_t stride;
        size_t remaining;
        T& operator*() { return *ptr; }
        void operator++() {
            if (remaining > 1) {
                ptr += stride;
                --remaining;
            }
            else {
                remaining = 0;
            }
        }
        bool operator!=(const ColIterator& other) const { return remaining != other.remaining; }
    };
    struct ColRange {
        T* start;
        size_t len;
        size_t stride;
        ColIterator begin() { return {start, stride, len}; }
        ColIterator end() { return {start, stride, 0}; }
    };
    struct ConstColIterator {
        const T* ptr;
        size_t stride;
        size_t remaining;
        const T& operator*() const { return *ptr; }
        void operator++() {
            if (remaining > 1) {
                ptr += stride;
                --remaining;
            }
            else {
                remaining = 0;
            }
        }
        bool operator!=(const ConstColIterator& other) const { return remaining != other.remaining; }
    };
    struct ConstColRange {
        const T* start;
        size_t len;
        size_t stride;
        ConstColIterator begin() const { return {start, stride, len}; }
        ConstColIterator end() const { return {start, stride, 0}; }
    };
    ColRange col(size_t c) {
        return c < Cols ? ColRange{&data[0][c], Rows, Cols} : ColRange{end(), 0, Cols};
    }
    ConstColRange col(size_t c) const {
        return c < Cols ? ConstColRange{&data[0][c], Rows, Cols} : ConstColRange{end(), 0, Cols};
    }
    // =================== proxy iterators end ====================
    // clang-format on
};
