#pragma once

#include <stdint.h>
#include <type_traits>
#include <utility>

// ========================
// Ok / Err 中间包装
// ========================

struct OkVoid
{};

template <typename T>
struct OkValue
{
    T value;
};

template <typename E>
struct ErrValue
{
    E error;
};

constexpr OkVoid Ok() {
    return {};
}

template <typename T>
constexpr OkValue<std::decay_t<T>> Ok(T&& value) {
    return {std::forward<T>(value)};
}

template <typename E>
constexpr ErrValue<std::decay_t<E>> Err(E&& error) {
    return {std::forward<E>(error)};
}

template <typename T, typename E>
class Result;

// ========================
// Result<T, E>
// ========================

template <typename T, typename E>
class Result
{
    static_assert(!std::is_void_v<T>, "Use Result<void, E> specialization");
    static_assert(std::is_trivially_copyable_v<T>,
                  "MCU Result requires trivially copyable T");
    static_assert(std::is_trivially_copyable_v<E>,
                  "MCU Result requires trivially copyable E");

private:
    bool ok_;

    union
    {
        T value_;
        E error_;
    };

public:
    using value_type = T;
    using error_type = E;

    template <typename U,
              typename = std::enable_if_t<std::is_convertible_v<U, T>>>
    constexpr Result(OkValue<U> ok)
        : ok_(true), value_(static_cast<T>(ok.value)) {}

    template <typename G,
              typename = std::enable_if_t<std::is_convertible_v<G, E>>>
    constexpr Result(ErrValue<G> err)
        : ok_(false), error_(static_cast<E>(err.error)) {}

    constexpr bool is_ok() const {
        return ok_;
    }

    constexpr bool is_err() const {
        return !ok_;
    }

    constexpr explicit operator bool() const {
        return ok_;
    }

    constexpr T unwrap() const {
        return value_;
    }

    constexpr E unwrap_err() const {
        return error_;
    }

    constexpr T value_or(T default_value) const {
        return ok_ ? value_ : default_value;
    }

    template <typename F>
    constexpr auto map(F f) const {
        using RawU = decltype(f(value_));
        using U    = std::decay_t<RawU>;

        if constexpr (std::is_void_v<RawU>) {
            if (ok_) {
                f(value_);
                return Result<void, E>(Ok());
            }
            else {
                return Result<void, E>(Err(error_));
            }
        }
        else {
            if (ok_) {
                return Result<U, E>(Ok(f(value_)));
            }
            else {
                return Result<U, E>(Err(error_));
            }
        }
    }

    template <typename F>
    constexpr auto map_err(F f) const {
        using RawG = decltype(f(error_));
        using G    = std::decay_t<RawG>;

        static_assert(!std::is_void_v<G>, "Error type cannot be void");

        if (ok_) {
            return Result<T, G>(Ok(value_));
        }
        else {
            return Result<T, G>(Err(f(error_)));
        }
    }

    template <typename U, typename F>
    constexpr U map_or(U default_value, F f) const {
        if (ok_) {
            return static_cast<U>(f(value_));
        }
        else {
            return default_value;
        }
    }

    template <typename U, typename F>
    constexpr U map_err_or(U default_value, F f) const {
        if (ok_) {
            return default_value;
        }
        else {
            return static_cast<U>(f(error_));
        }
    }

    template <typename F>
    constexpr auto and_then(F f) const {
        using R = decltype(f(value_));

        if (ok_) {
            return f(value_);
        }
        else {
            return R(Err(error_));
        }
    }

    template <typename F>
    constexpr auto or_else(F f) const {
        using R = decltype(f(error_));

        if (ok_) {
            return R(Ok(value_));
        }
        else {
            return f(error_);
        }
    }
};

template <typename E>
class Result<void, E>
{
    static_assert(std::is_trivially_copyable_v<E>,
                  "MCU Result requires trivially copyable E");

private:
    bool ok_;

    union
    {
        E error_;
    };

public:
    using value_type = void;
    using error_type = E;

    constexpr Result(OkVoid) : ok_(true) {}

    template <typename G,
              typename = std::enable_if_t<std::is_convertible_v<G, E>>>
    constexpr Result(ErrValue<G> err)
        : ok_(false), error_(static_cast<E>(err.error)) {}

    constexpr bool is_ok() const {
        return ok_;
    }

    constexpr bool is_err() const {
        return !ok_;
    }

    constexpr explicit operator bool() const {
        return ok_;
    }

    constexpr void unwrap() const {}

    constexpr E unwrap_err() const {
        return error_;
    }

    template <typename F>
    constexpr auto map(F f) const {
        using RawU = decltype(f());
        using U    = std::decay_t<RawU>;

        if constexpr (std::is_void_v<RawU>) {
            if (ok_) {
                f();
                return Result<void, E>(Ok());
            }
            else {
                return Result<void, E>(Err(error_));
            }
        }
        else {
            if (ok_) {
                return Result<U, E>(Ok(f()));
            }
            else {
                return Result<U, E>(Err(error_));
            }
        }
    }

    template <typename F>
    constexpr auto map_err(F f) const {
        using RawG = decltype(f(error_));
        using G    = std::decay_t<RawG>;

        static_assert(!std::is_void_v<G>, "Error type cannot be void");

        if (ok_) {
            return Result<void, G>(Ok());
        }
        else {
            return Result<void, G>(Err(f(error_)));
        }
    }

    template <typename U, typename F>
    constexpr U map_or(U default_value, F f) const {
        if (ok_) {
            return static_cast<U>(f());
        }
        else {
            return default_value;
        }
    }

    template <typename U, typename F>
    constexpr U map_err_or(U default_value, F f) const {
        if (ok_) {
            return default_value;
        }
        else {
            return static_cast<U>(f(error_));
        }
    }

    template <typename F>
    constexpr auto and_then(F f) const {
        using R = decltype(f());

        if (ok_) {
            return f();
        }
        else {
            return R(Err(error_));
        }
    }

    template <typename F>
    constexpr auto or_else(F f) const {
        using R = decltype(f(error_));

        if (ok_) {
            return R(Ok());
        }
        else {
            return f(error_);
        }
    }
};
