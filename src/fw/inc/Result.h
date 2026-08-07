#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

template <auto Code>
struct ErrorConstant
{
    static constexpr auto value = Code;
};

template <auto Code>
inline constexpr ErrorConstant<Code> error {};

template <auto Code, typename Function>
class ErrorCase
{
public:
    static constexpr auto code = Code;
    using function_type        = Function;

    constexpr explicit ErrorCase(Function function)
        : function_(std::move(function)) {}

    constexpr decltype(auto) invoke() {
        return std::invoke(function_);
    }

    constexpr decltype(auto) invoke() const
        requires std::invocable<const Function&>
    {
        return std::invoke(function_);
    }

private:
    [[no_unique_address]] Function function_;
};

template <auto Code, typename Function>
[[nodiscard]] constexpr auto on(Function&& function) {
    return ErrorCase<Code, std::decay_t<Function>> {
        std::forward<Function>(function),
    };
}

namespace result_detail
{

    template <auto... Values>
    struct UniqueValues;

    template <>
    struct UniqueValues<> : std::true_type
    {};

    template <auto Head, auto... Tail>
    struct UniqueValues<Head, Tail...>
        : std::bool_constant<((Head != Tail) && ...) && UniqueValues<Tail...>::value>
    {};

    template <typename T>
    struct ErrorCaseInfo
    {
        static constexpr bool is_case = false;
    };

    template <auto Code, typename Function>
    struct ErrorCaseInfo<ErrorCase<Code, Function>>
    {
        static constexpr bool is_case = true;
        static constexpr auto code    = Code;
        using function_type           = Function;
    };

    template <typename T>
    concept NullaryErrorCase =
        ErrorCaseInfo<std::remove_cvref_t<T>>::is_case && std::invocable<typename ErrorCaseInfo<std::remove_cvref_t<T>>::function_type&>;

} // namespace result_detail

template <auto... Allowed>
class ErrorSet
{
    static_assert(sizeof...(Allowed) > 0, "ErrorSet must contain at least one error code");
    static_assert(result_detail::UniqueValues<Allowed...>::value, "ErrorSet cannot contain duplicate error codes");

public:
    using code_type = std::common_type_t<decltype(Allowed)...>;

private:
    static_assert((std::convertible_to<decltype(Allowed), code_type> && ...),
                  "All ErrorSet values must share a common code type");

    struct Unchecked
    {};

    code_type code_;

    constexpr ErrorSet(code_type code, Unchecked)
        : code_(code) {}

    template <auto Code>
    static constexpr bool contains_code_ = ((Code == Allowed) || ...);

    template <typename Case>
    static constexpr auto case_code_ =
        result_detail::ErrorCaseInfo<std::remove_cvref_t<Case>>::code;

    template <auto Code, typename... Cases>
    static constexpr size_t case_count_ =
        (size_t {0} + ... + (case_code_<Cases> == Code ? size_t {1} : size_t {0}));

    template <typename Current, typename... Rest>
    constexpr decltype(auto) dispatch(Current&& current, Rest&&... rest) const {
        if constexpr (sizeof...(Rest) == 0) {
            // Exhaustiveness and ErrorSet's private construction guarantee that
            // the final arm is the matching arm.
            return current.invoke();
        }
        else {
            if (code_ == static_cast<code_type>(case_code_<Current>)) {
                return current.invoke();
            }

            return dispatch(std::forward<Rest>(rest)...);
        }
    }

public:
    ErrorSet() = delete;

    template <auto Code>
        requires contains_code_<Code>
    constexpr ErrorSet(ErrorConstant<Code>)
        : code_(static_cast<code_type>(Code)) {}

    template <auto... Other>
        requires((contains_code_<Other>) && ...)
    constexpr ErrorSet(ErrorSet<Other...> other)
        : code_(static_cast<code_type>(other.native())) {}

    template <auto Code>
        requires contains_code_<Code>
    [[nodiscard]] static constexpr ErrorSet of() {
        return ErrorSet {
            static_cast<code_type>(Code),
            Unchecked {},
        };
    }

    [[nodiscard]] static constexpr bool contains(code_type code) {
        return ((code == static_cast<code_type>(Allowed)) || ...);
    }

    [[nodiscard]] static constexpr std::optional<ErrorSet> try_from(code_type code) {
        if (!contains(code)) {
            return std::nullopt;
        }

        return ErrorSet {
            code,
            Unchecked {},
        };
    }

    template <auto Fallback>
        requires contains_code_<Fallback>
    [[nodiscard]] static constexpr ErrorSet narrow_or(code_type code) {
        return ErrorSet {
            contains(code) ? code : static_cast<code_type>(Fallback),
            Unchecked {},
        };
    }

    template <auto Code>
    [[nodiscard]] constexpr bool is() const {
        if constexpr (contains_code_<Code>) {
            return code_ == static_cast<code_type>(Code);
        }
        else {
            return false;
        }
    }

    [[nodiscard]] constexpr code_type native() const {
        return code_;
    }

    template <result_detail::NullaryErrorCase... Cases>
        requires(sizeof...(Cases) > 0)
    constexpr decltype(auto) match(Cases&&... cases) const {
        static_assert((contains_code_<case_code_<Cases>> && ...),
                      "ErrorSet::match contains an arm outside this error set");
        static_assert(((case_count_<Allowed, Cases...> == 1) && ...),
                      "ErrorSet::match must handle every error exactly once");

        using FirstCase   = std::tuple_element_t<0, std::tuple<std::remove_cvref_t<Cases>...>>;
        using MatchResult = std::invoke_result_t<typename result_detail::ErrorCaseInfo<FirstCase>::function_type&>;

        static_assert((std::same_as<
                           MatchResult,
                           std::invoke_result_t<
                               typename result_detail::ErrorCaseInfo<std::remove_cvref_t<Cases>>::function_type&>> &&
                       ...),
                      "Every ErrorSet::match arm must return the same type");

        return dispatch(std::forward<Cases>(cases)...);
    }
};

template <typename T>
struct OkValue
{
    T value;
};

struct OkVoid
{};

template <typename E>
struct ErrValue
{
    E error;
};

[[nodiscard]] constexpr OkVoid Ok() {
    return {};
}

template <typename T>
[[nodiscard]] constexpr OkValue<std::decay_t<T>> Ok(T&& value) {
    return {
        std::forward<T>(value),
    };
}

template <typename E>
[[nodiscard]] constexpr ErrValue<std::decay_t<E>> Err(E&& errorValue) {
    return {
        std::forward<E>(errorValue),
    };
}

template <auto Code>
[[nodiscard]] constexpr ErrValue<ErrorConstant<Code>> Err() {
    return {
        ErrorConstant<Code> {},
    };
}

template <typename T, typename E>
class Result;

template <typename T, typename E>
class [[nodiscard]] Result
{
    static_assert(!std::is_void_v<T>, "Use Result<void, E> for a result without a value");
    static_assert(std::is_trivially_copyable_v<T>, "MCU Result requires a trivially copyable value");
    static_assert(std::is_trivially_copyable_v<E>, "MCU Result requires a trivially copyable error");

    bool ok_;

    union
    {
        T value_;
        E error_;
    };

public:
    using value_type = T;
    using error_type = E;

    template <typename U>
        requires std::constructible_from<T, U>
    constexpr Result(OkValue<U> ok)
        : ok_(true), value_(std::move(ok.value)) {}

    template <typename G>
        requires std::constructible_from<E, G>
    constexpr Result(ErrValue<G> err)
        : ok_(false), error_(std::move(err.error)) {}

    [[nodiscard]] constexpr bool is_ok() const {
        return ok_;
    }

    [[nodiscard]] constexpr bool is_err() const {
        return !ok_;
    }

    [[nodiscard]] constexpr explicit operator bool() const {
        return is_ok();
    }

    [[nodiscard]] constexpr const T& value() const {
        return value_;
    }

    [[nodiscard]] constexpr const E& error() const {
        return error_;
    }

    [[nodiscard]] constexpr T unwrap() const {
        return value_;
    }

    [[nodiscard]] constexpr E unwrap_err() const {
        return error_;
    }

    [[nodiscard]] constexpr T value_or(T defaultValue) const {
        return ok_ ? value_ : defaultValue;
    }

    template <typename Function>
        requires std::invocable<Function, const T&>
    constexpr auto map(Function&& function) const {
        using RawU = std::invoke_result_t<Function, const T&>;
        using U    = std::decay_t<RawU>;

        if constexpr (std::is_void_v<RawU>) {
            if (ok_) {
                std::invoke(std::forward<Function>(function), value_);
                return Result<void, E> {Ok()};
            }

            return Result<void, E> {Err(error_)};
        }
        else {
            if (ok_) {
                return Result<U, E> {
                    Ok(std::invoke(std::forward<Function>(function), value_)),
                };
            }

            return Result<U, E> {Err(error_)};
        }
    }

    template <typename Function>
        requires std::invocable<Function, const E&>
    constexpr auto map_err(Function&& function) const {
        using RawG = std::invoke_result_t<Function, const E&>;
        using G    = std::decay_t<RawG>;

        static_assert(!std::is_void_v<RawG>, "Result::map_err cannot produce a void error");

        if (ok_) {
            return Result<T, G> {Ok(value_)};
        }

        return Result<T, G> {
            Err(std::invoke(std::forward<Function>(function), error_)),
        };
    }

    template <typename U, typename Function>
        requires std::invocable<Function, const T&> && std::convertible_to<std::invoke_result_t<Function, const T&>, U>
    [[nodiscard]] constexpr U map_or(U defaultValue, Function&& function) const {
        if (ok_) {
            return static_cast<U>(
                std::invoke(std::forward<Function>(function), value_));
        }

        return defaultValue;
    }

    template <typename U, typename Function>
        requires std::invocable<Function, const E&> && std::convertible_to<std::invoke_result_t<Function, const E&>, U>
    [[nodiscard]] constexpr U map_err_or(U defaultValue, Function&& function) const {
        if (ok_) {
            return defaultValue;
        }

        return static_cast<U>(
            std::invoke(std::forward<Function>(function), error_));
    }

    template <typename Function>
        requires std::invocable<Function, const T&>
    constexpr auto and_then(Function&& function) const
        -> std::remove_cvref_t<std::invoke_result_t<Function, const T&>> {
        using R = std::remove_cvref_t<std::invoke_result_t<Function, const T&>>;

        if constexpr (!std::is_constructible_v<R, ErrValue<E>>) {
            static_assert(std::is_constructible_v<R, ErrValue<E>>,
                          "Result::and_then target must accept this result's error type");
        }
        else {
            if (ok_) {
                return R {
                    std::invoke(std::forward<Function>(function), value_),
                };
            }

            return R {Err(error_)};
        }
    }

    template <typename Function>
        requires std::invocable<Function, const E&>
    constexpr auto or_else(Function&& function) const
        -> std::remove_cvref_t<std::invoke_result_t<Function, const E&>> {
        using R = std::remove_cvref_t<std::invoke_result_t<Function, const E&>>;

        if constexpr (!std::is_constructible_v<R, OkValue<T>>) {
            static_assert(std::is_constructible_v<R, OkValue<T>>,
                          "Result::or_else target must accept this result's value type");
        }
        else {
            if (ok_) {
                return R {Ok(value_)};
            }

            return R {
                std::invoke(std::forward<Function>(function), error_),
            };
        }
    }

    template <typename OkFunction, typename ErrFunction>
        requires std::invocable<OkFunction, const T&> && std::invocable<ErrFunction, const E&>
    constexpr decltype(auto) match(OkFunction&& okFunction, ErrFunction&& errFunction) const {
        using OkResult  = std::invoke_result_t<OkFunction, const T&>;
        using ErrResult = std::invoke_result_t<ErrFunction, const E&>;

        static_assert(std::same_as<OkResult, ErrResult>,
                      "Both Result::match arms must return the same type");

        if (ok_) {
            return std::invoke(std::forward<OkFunction>(okFunction), value_);
        }

        return std::invoke(std::forward<ErrFunction>(errFunction), error_);
    }
};

template <typename E>
class [[nodiscard]] Result<void, E>
{
    static_assert(std::is_trivially_copyable_v<E>, "MCU Result requires a trivially copyable error");

    bool ok_;

    union
    {
        E error_;
    };

public:
    using value_type = void;
    using error_type = E;

    constexpr Result(OkVoid)
        : ok_(true) {}

    template <typename G>
        requires std::constructible_from<E, G>
    constexpr Result(ErrValue<G> err)
        : ok_(false), error_(std::move(err.error)) {}

    [[nodiscard]] constexpr bool is_ok() const {
        return ok_;
    }

    [[nodiscard]] constexpr bool is_err() const {
        return !ok_;
    }

    [[nodiscard]] constexpr explicit operator bool() const {
        return is_ok();
    }

    [[nodiscard]] constexpr const E& error() const {
        return error_;
    }

    constexpr void unwrap() const {}

    [[nodiscard]] constexpr E unwrap_err() const {
        return error_;
    }

    template <typename Function>
        requires std::invocable<Function>
    constexpr auto map(Function&& function) const {
        using RawU = std::invoke_result_t<Function>;
        using U    = std::decay_t<RawU>;

        if constexpr (std::is_void_v<RawU>) {
            if (ok_) {
                std::invoke(std::forward<Function>(function));
                return Result<void, E> {Ok()};
            }

            return Result<void, E> {Err(error_)};
        }
        else {
            if (ok_) {
                return Result<U, E> {
                    Ok(std::invoke(std::forward<Function>(function))),
                };
            }

            return Result<U, E> {Err(error_)};
        }
    }

    template <typename Function>
        requires std::invocable<Function, const E&>
    constexpr auto map_err(Function&& function) const {
        using RawG = std::invoke_result_t<Function, const E&>;
        using G    = std::decay_t<RawG>;

        static_assert(!std::is_void_v<RawG>, "Result::map_err cannot produce a void error");

        if (ok_) {
            return Result<void, G> {Ok()};
        }

        return Result<void, G> {
            Err(std::invoke(std::forward<Function>(function), error_)),
        };
    }

    template <typename U, typename Function>
        requires std::invocable<Function> && std::convertible_to<std::invoke_result_t<Function>, U>
    [[nodiscard]] constexpr U map_or(U defaultValue, Function&& function) const {
        if (ok_) {
            return static_cast<U>(
                std::invoke(std::forward<Function>(function)));
        }

        return defaultValue;
    }

    template <typename U, typename Function>
        requires std::invocable<Function, const E&> && std::convertible_to<std::invoke_result_t<Function, const E&>, U>
    [[nodiscard]] constexpr U map_err_or(U defaultValue, Function&& function) const {
        if (ok_) {
            return defaultValue;
        }

        return static_cast<U>(
            std::invoke(std::forward<Function>(function), error_));
    }

    template <typename Function>
        requires std::invocable<Function>
    constexpr auto and_then(Function&& function) const
        -> std::remove_cvref_t<std::invoke_result_t<Function>> {
        using R = std::remove_cvref_t<std::invoke_result_t<Function>>;

        if constexpr (!std::is_constructible_v<R, ErrValue<E>>) {
            static_assert(std::is_constructible_v<R, ErrValue<E>>,
                          "Result::and_then target must accept this result's error type");
        }
        else {
            if (ok_) {
                return R {
                    std::invoke(std::forward<Function>(function)),
                };
            }

            return R {Err(error_)};
        }
    }

    template <typename Function>
        requires std::invocable<Function, const E&>
    constexpr auto or_else(Function&& function) const
        -> std::remove_cvref_t<std::invoke_result_t<Function, const E&>> {
        using R = std::remove_cvref_t<std::invoke_result_t<Function, const E&>>;

        if constexpr (!std::is_constructible_v<R, OkVoid>) {
            static_assert(std::is_constructible_v<R, OkVoid>,
                          "Result::or_else target must accept an empty success value");
        }
        else {
            if (ok_) {
                return R {Ok()};
            }

            return R {
                std::invoke(std::forward<Function>(function), error_),
            };
        }
    }

    template <typename OkFunction, typename ErrFunction>
        requires std::invocable<OkFunction> && std::invocable<ErrFunction, const E&>
    constexpr decltype(auto) match(OkFunction&& okFunction, ErrFunction&& errFunction) const {
        using OkResult  = std::invoke_result_t<OkFunction>;
        using ErrResult = std::invoke_result_t<ErrFunction, const E&>;

        static_assert(std::same_as<OkResult, ErrResult>,
                      "Both Result::match arms must return the same type");

        if (ok_) {
            return std::invoke(std::forward<OkFunction>(okFunction));
        }

        return std::invoke(std::forward<ErrFunction>(errFunction), error_);
    }
};

/**
 * Convert a native zero-on-success error code into a closed Result error set.
 *
 * Known errors are preserved. An unknown native error is reported through
 * onUnexpected and then converted to Fallback.
 */
template <typename E, auto Fallback, auto Success = 0, typename Code, typename UnexpectedFunction>
    requires std::invocable<UnexpectedFunction, Code>
[[nodiscard]] constexpr Result<void, E>
from_native(Code code, UnexpectedFunction&& onUnexpected) {
    if (code == static_cast<Code>(Success)) {
        return Ok();
    }

    if (const auto error = E::try_from(code)) {
        return Err(*error);
    }

    std::invoke(
        std::forward<UnexpectedFunction>(onUnexpected),
        code);
    return Err(E::template of<Fallback>());
}
