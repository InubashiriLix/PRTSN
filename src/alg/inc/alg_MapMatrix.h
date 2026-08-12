#pragma once

#include "src/fw/inc/Result.h"
#include <algorithm>
#include <cstdint>
#include <type_traits>
#include "src/alg/inc/alg_err.h"
#include "src/alg/inc/alg_matrix.h"

template <size_t Rows, size_t Cols, typename K, typename V>
struct MapMatrix
{
    static_assert(Rows > 0 && Cols > 0, "MapMatrix dimensions must be greater than zero");
    static_assert(std::is_trivially_copyable_v<K>,
                  "MapMatrix requires trivially copyable K because Result<K, E> stores values directly");
    static_assert(std::is_trivially_copyable_v<V>,
                  "MapMatrix requires trivially copyable V because Result<V, E> stores values directly");

    struct Entry
    {
        K key;
        V value;
    };

    Matrix<Rows, Cols, K>
                          keys;
    Matrix<Rows, Cols, V> vals;

    explicit MapMatrix(const K& initialKey = K(), const V& initialVal = V())
        : keys(initialKey), vals(initialVal) {}

    explicit MapMatrix(const K* initialKeys, const V* initialVals, size_t len) {
        if (initialKeys != nullptr && initialVals != nullptr && len == Rows * Cols) {
            std::copy(initialKeys, initialKeys + len, keys.begin());
            std::copy(initialVals, initialVals + len, vals.begin());
        }
        else {
            keys.fill(K());
            vals.fill(V());
        }
    }

    explicit MapMatrix(const Matrix<Rows, Cols, K>& initialKeys, const Matrix<Rows, Cols, V>& initialVals)
        : keys(initialKeys), vals(initialVals) {}

    Result<void, MapMatrixError>
    set(size_t i, size_t j, const K& key, const V& val) {
        if (i >= Rows || j >= Cols) {
            return Err<MapMatrixErrorCode::INDEX_OUT_OF_BOUNDS>("MapMatrix index is out of bounds");
        }
        keys.data[i][j] = key;
        vals.data[i][j] = val;
        return Ok();
    }

    // ================== CRUD methods ====================
    Result<Entry, MapMatrixError> get(size_t i, size_t j) const {
        if (i >= Rows || j >= Cols) {
            return Err<MapMatrixErrorCode::INDEX_OUT_OF_BOUNDS>("MapMatrix index is out of bounds");
        }
        return Ok(Entry {keys.data[i][j], vals.data[i][j]});
    }

    Result<K, MapMatrixError> getKey(size_t i, size_t j) const {
        if (i >= Rows || j >= Cols) {
            return Err<MapMatrixErrorCode::INDEX_OUT_OF_BOUNDS>("MapMatrix index is out of bounds");
        }
        return Ok(keys.data[i][j]);
    }

    Result<V, MapMatrixError> getValue(size_t i, size_t j) const {
        if (i >= Rows || j >= Cols) {
            return Err<MapMatrixErrorCode::INDEX_OUT_OF_BOUNDS>("MapMatrix index is out of bounds");
        }
        return Ok(vals.data[i][j]);
    }

    // =================== setters ====================
    Result<K, MapMatrixError> setKey(size_t i, size_t j, const K& key) {
        if (i >= Rows || j >= Cols) {
            return Err<MapMatrixErrorCode::INDEX_OUT_OF_BOUNDS>("MapMatrix index is out of bounds");
        }
        auto temp       = keys.data[i][j];
        keys.data[i][j] = key;
        return Ok(std::move(temp));
    }

    Result<V, MapMatrixError> setValue(size_t i, size_t j, const V& val) {
        if (i >= Rows || j >= Cols) {
            return Err<MapMatrixErrorCode::INDEX_OUT_OF_BOUNDS>("MapMatrix index is out of bounds");
        }
        auto temp       = vals.data[i][j];
        vals.data[i][j] = val;
        return Ok(std::move(temp));
    }

    Result<Entry, MapMatrixError> setEntry(size_t i, size_t j, const Entry& entry) {
        if (i >= Rows || j >= Cols) {
            return Err<MapMatrixErrorCode::INDEX_OUT_OF_BOUNDS>("MapMatrix index is out of bounds");
        }
        Entry temp {keys.data[i][j], vals.data[i][j]};
        keys.data[i][j] = entry.key;
        vals.data[i][j] = entry.value;
        return Ok(std::move(temp));
    }

    // =================== replace ====================
    Result<K, MapMatrixError> replaceKey(size_t i, size_t j, const K& key) {
        return setKey(i, j, key);
    }

    Result<V, MapMatrixError> replaceValue(size_t i, size_t j, const V& val) {
        return setValue(i, j, val);
    }

    Result<Entry, MapMatrixError> replaceEntry(size_t i, size_t j, const Entry& entry) {
        return setEntry(i, j, entry);
    }

    // =================== remove ====================
    Result<K, MapMatrixError> removeKey(size_t i, size_t j) {
        return setKey(i, j, K());
    }

    Result<V, MapMatrixError> removeValue(size_t i, size_t j) {
        return setValue(i, j, V());
    }

    Result<Entry, MapMatrixError> removeEntry(size_t i, size_t j) {
        return setEntry(i, j, Entry {K(), V()});
    }

    // ================ search ===================
    // clang-format off
    struct Idx
    {
        size_t i;
        size_t j;
    };

    struct SearchCursor {
        size_t           i = 0, j = Cols;
        const MapMatrix* map;
        K                needle;

        bool next() {
            while (++j >= Cols) { j = 0; if (++i >= Rows) return false; }
            return map->keys.data[i][j] == needle ? true : next();
        }

        void reset() { i = 0; j = Cols; }

        size_t row() const { return i; }
        size_t col() const { return j; }

        explicit operator bool() const { return i < Rows && j < Cols; }
    };

    struct ValueSearchCursor {
        size_t           i = 0, j = Cols;
        const MapMatrix* map;
        V                needle;

        bool next() {
            while (++j >= Cols) { j = 0; if (++i >= Rows) return false; }
            return map->vals.data[i][j] == needle ? true : next();
        }

        void reset() { i = 0; j = Cols; }

        size_t row() const { return i; }
        size_t col() const { return j; }

        explicit operator bool() const { return i < Rows && j < Cols; }
    };

    SearchCursor      searchByKey(const K& key) const { return SearchCursor {0, Cols, this, key}; }
    ValueSearchCursor searchByValue(const V& val) const { return ValueSearchCursor {0, Cols, this, val}; }

    Result<Idx, MapMatrixError> findFirstKey(const K& key) const {
        for (size_t i = 0; i < Rows; ++i)
            for (size_t j = 0; j < Cols; ++j)
                if (keys.data[i][j] == key)
                    return Ok(Idx {i, j});
        return Err<MapMatrixErrorCode::INDEX_OUT_OF_BOUNDS>("MapMatrix value was not found");
    }

    Result<Idx, MapMatrixError> findFirstValue(const V& val) const {
        for (size_t i = 0; i < Rows; ++i)
            for (size_t j = 0; j < Cols; ++j)
                if (vals.data[i][j] == val)
                    return Ok(Idx {i, j});
        return Err<MapMatrixErrorCode::INDEX_OUT_OF_BOUNDS>("MapMatrix value was not found");
    }

    bool containsKey(const K& key) const {
        for (size_t i = 0; i < Rows; ++i)
            for (size_t j = 0; j < Cols; ++j)
                if (keys.data[i][j] == key) return true;
        return false;
    }

    bool containsValue(const V& val) const {
        for (size_t i = 0; i < Rows; ++i)
            for (size_t j = 0; j < Cols; ++j)
                if (vals.data[i][j] == val) return true;
        return false;
    }

    size_t countKey(const K& key) const {
        size_t n = 0;
        for (size_t i = 0; i < Rows; ++i)
            for (size_t j = 0; j < Cols; ++j)
                if (keys.data[i][j] == key) ++n;
        return n;
    }

    size_t countValue(const V& val) const {
        size_t n = 0;
        for (size_t i = 0; i < Rows; ++i)
            for (size_t j = 0; j < Cols; ++j)
                if (vals.data[i][j] == val) ++n;
        return n;
    }

    // ==================  Iterators  =====================
    static constexpr size_t kSize = Rows * Cols;

    struct Iterator {
        K* kptr;
        V* vptr;
        Entry operator*() const { return {*kptr, *vptr}; }
        Iterator& operator++() { ++kptr; ++vptr; return *this; }
        bool operator!=(const Iterator& other) const { return kptr != other.kptr; }
    };

    struct ConstIterator {
        const K* kptr;
        const V* vptr;
        Entry operator*() const { return {*kptr, *vptr}; }
        ConstIterator& operator++() { ++kptr; ++vptr; return *this; }
        bool operator!=(const ConstIterator& other) const { return kptr != other.kptr; }
    };

    Iterator      begin()       { return {keys.begin(), vals.begin()}; }
    Iterator      end()         { return {keys.end(),   vals.end()}; }
    ConstIterator begin() const { return {keys.begin(), vals.begin()}; }
    ConstIterator end()   const { return {keys.end(),   vals.end()}; }
    ConstIterator cbegin() const { return {keys.begin(), vals.begin()}; }
    ConstIterator cend()   const { return {keys.end(),   vals.end()}; }

    // ================= Utility Methods ==================
    static constexpr size_t size()     { return kSize; }
    static constexpr size_t capacity() { return kSize; }

    Result<void, MapMatrixError> fill(const K& k, const V& v) {
        keys.fill(k);
        vals.fill(v);
        return Ok();
    }

    Result<void, MapMatrixError> clear() {
        return fill(K(), V());
    }

    Result<void, MapMatrixError> swap(size_t i1, size_t j1, size_t i2, size_t j2) {
        if (i1 >= Rows || j1 >= Cols || i2 >= Rows || j2 >= Cols) {
            return Err<MapMatrixErrorCode::INDEX_OUT_OF_BOUNDS>("MapMatrix index is out of bounds");
        }
        std::swap(keys.data[i1][j1], keys.data[i2][j2]);
        std::swap(vals.data[i1][j1], vals.data[i2][j2]);
        return Ok();
    }

    bool operator==(const MapMatrix& other) const {
        for (size_t i = 0; i < Rows; ++i)
            for (size_t j = 0; j < Cols; ++j) {
                if (keys.data[i][j] != other.keys.data[i][j]) return false;
                if (vals.data[i][j] != other.vals.data[i][j]) return false;
            }
        return true;
    }

    bool operator!=(const MapMatrix& other) const { return !(*this == other); }
    // clang-format on
};
