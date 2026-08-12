#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <concepts>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace error_trace_depth
{
    /// 不保存 cause，只有顶层错误。No cause frames; keep only the top-level error.
    inline constexpr std::size_t AMPUTATION = 0;
    /// 默认保存 4 层 cause。Default policy: retain up to four cause frames.
    inline constexpr std::size_t RUN_FOR_YOUR_LIFE = 4;
    /// 最大保存 8 层 cause。Maximum policy: retain up to eight cause frames.
    inline constexpr std::size_t CHASE_IT_DOWN = 8;
} // namespace error_trace_depth

/**
 * @brief 单个错误码的 trace 策略描述符。Per-error trace policy descriptor.
 *
 * 业务代码通常使用同名变量模板 `TraceErrorSet<Depth, Code>`，而不直接构造本类型。
 */
template <std::size_t Depth, auto Code>
struct TraceErrorSpec
{
    static_assert(Depth <= error_trace_depth::CHASE_IT_DOWN,
                  "Per-error trace depth cannot exceed CHASE_IT_DOWN");
    static_assert(std::is_enum_v<decltype(Code)> || std::is_integral_v<decltype(Code)>,
                  "Error codes must be enum or integral values");

    static constexpr std::size_t depth = Depth;
    static constexpr auto        code  = Code;

    constexpr bool operator==(const TraceErrorSpec&) const noexcept = default;
};

/**
 * @brief 在 `ErrorSet` 声明中覆盖一个错误的 cause 深度。
 * Override one error's cause depth inside an ErrorSet declaration.
 *
 * @code
 * using Errors = ErrorSet<
 *     Error::Ordinary,
 *     TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, Error::Important>>;
 * @endcode
 *
 * 返回错误时仍写 `Err<Error::Important>()`，不要把此描述符传给 `Err<>`。
 */
template <std::size_t Depth, auto Code>
inline constexpr TraceErrorSpec<Depth, Code> TraceErrorSet {};

/**
 * @brief 一层只读错误诊断信息。A read-only diagnostic frame in a cause chain.
 *
 * 字符串均为非拥有指针，通常指向字符串字面量或编译期生成的名称。
 * All strings are non-owning and normally point to literals or compile-time names.
 * `numericCode` 只用于日志；业务分支应使用 `ErrorSet::is<Code>()`。
 */
struct ErrorFrame
{
    const char*  domain      = "";
    const char*  name        = "";
    std::int32_t numericCode = 0;
    const char*  message     = "";
};

template <typename Code>
/**
 * @brief 可选的错误域名称覆盖。Optional override for an error-code type name.
 *
 * 普通 enum 不需要特化；默认域名自动取 enum 类型名。仅在日志需要稳定 ABI
 * 名称时特化，例如：
 * @code
 * template <> struct ErrorDomainOverride<MyError> {
 *     inline static constexpr const char* value = "motor";
 * };
 * @endcode
 */
struct ErrorDomainOverride
{
    inline static constexpr const char* value = nullptr;
};

template <auto Code>
/**
 * @brief 可选的单个错误名称覆盖。Optional override for one error name.
 *
 * 普通枚举项会自动提取名字，无需注册。此接口主要处理同值枚举别名或稳定日志文案。
 */
struct ErrorNameOverride
{
    inline static constexpr const char* value = nullptr;
};

namespace result_detail
{
    template <typename T>
    struct IsTraceErrorSpec : std::false_type
    {};

    template <std::size_t Depth, auto Code>
    struct IsTraceErrorSpec<TraceErrorSpec<Depth, Code>> : std::true_type
    {};

    template <auto Entry>
    inline constexpr bool IsTraceErrorSpecValue =
        IsTraceErrorSpec<std::remove_cvref_t<decltype(Entry)>>::value;

    template <auto Entry>
    [[nodiscard]] consteval auto entry_code() {
        if constexpr (IsTraceErrorSpecValue<Entry>) {
            return std::remove_cvref_t<decltype(Entry)>::code;
        }
        else {
            return Entry;
        }
    }

    template <std::size_t DefaultDepth, auto Entry>
    [[nodiscard]] consteval std::size_t entry_depth() {
        if constexpr (IsTraceErrorSpecValue<Entry>) {
            return std::remove_cvref_t<decltype(Entry)>::depth;
        }
        else {
            return DefaultDepth;
        }
    }

    template <std::size_t DefaultDepth, auto... Entries>
    [[nodiscard]] consteval std::size_t maximum_entry_depth() {
        std::size_t result = 0;
        ((result = result < entry_depth<DefaultDepth, Entries>()
                       ? entry_depth<DefaultDepth, Entries>()
                       : result),
         ...);
        return result;
    }

    template <auto Left, auto Right>
    [[nodiscard]] consteval bool same_error() {
        constexpr auto LeftCode  = entry_code<Left>();
        constexpr auto RightCode = entry_code<Right>();
        if constexpr (!std::same_as<decltype(LeftCode), decltype(RightCode)>) {
            return false;
        }
        else {
            return LeftCode == RightCode;
        }
    }

    template <auto... Values>
    struct UniqueErrors;

    template <>
    struct UniqueErrors<> : std::true_type
    {};

    template <auto Head, auto... Tail>
    struct UniqueErrors<Head, Tail...>
        : std::bool_constant<((!same_error<Head, Tail>()) && ...) && UniqueErrors<Tail...>::value>
    {};

    template <typename Code>
    [[nodiscard]] constexpr std::int32_t diagnostic_code(Code code) noexcept {
        static_assert(std::is_enum_v<Code> || std::is_integral_v<Code>,
                      "Error codes must be enums or integers");
        return static_cast<std::int32_t>(code);
    }

    template <typename T>
    [[nodiscard]] consteval std::string_view wrapped_type_name() {
#if defined(__clang__) || defined(__GNUC__)
        return __PRETTY_FUNCTION__;
#else
#error "Automatic error names require GCC or Clang"
#endif
    }

    template <auto Value>
    [[nodiscard]] consteval std::string_view wrapped_value_name() {
#if defined(__clang__) || defined(__GNUC__)
        return __PRETTY_FUNCTION__;
#else
#error "Automatic error names require GCC or Clang"
#endif
    }

    [[nodiscard]] consteval std::string_view extract_assignment(
        std::string_view wrapped,
        std::string_view key) {
        const std::size_t begin = wrapped.find(key);
        if (begin == std::string_view::npos) {
            return {};
        }
        const std::size_t valueBegin = begin + key.size();
        std::size_t       valueEnd   = wrapped.find(';', valueBegin);
        const std::size_t bracketEnd = wrapped.find(']', valueBegin);
        if (valueEnd == std::string_view::npos ||
            (bracketEnd != std::string_view::npos && bracketEnd < valueEnd)) {
            valueEnd = bracketEnd;
        }
        return valueEnd == std::string_view::npos
                   ? std::string_view {}
                   : wrapped.substr(valueBegin, valueEnd - valueBegin);
    }

    template <typename T>
    [[nodiscard]] consteval std::string_view reflected_type_name() {
#if defined(__clang__)
        return extract_assignment(wrapped_type_name<T>(), "T = ");
#else
        return extract_assignment(wrapped_type_name<T>(), "T = ");
#endif
    }

    template <auto Value>
    [[nodiscard]] consteval std::string_view reflected_full_value_name() {
#if defined(__clang__)
        return extract_assignment(wrapped_value_name<Value>(), "Value = ");
#else
        return extract_assignment(wrapped_value_name<Value>(), "Value = ");
#endif
    }

    template <auto Value>
    [[nodiscard]] consteval std::string_view reflected_value_name() {
        constexpr std::string_view full      = reflected_full_value_name<Value>();
        const std::size_t          separator = full.rfind("::");
        return separator == std::string_view::npos ? full : full.substr(separator + 2);
    }

    template <std::size_t Size>
    struct StaticText
    {
        std::array<char, Size + 1> bytes {};

        [[nodiscard]] constexpr const char* data() const noexcept {
            return bytes.data();
        }
    };

    template <typename T>
    [[nodiscard]] consteval auto make_type_text() {
        constexpr std::string_view source = reflected_type_name<T>();
        StaticText<source.size()>  result {};
        for (std::size_t index = 0; index < source.size(); ++index) {
            result.bytes[index] = source[index];
        }
        return result;
    }

    template <auto Value>
    [[nodiscard]] consteval auto make_value_text() {
        constexpr std::string_view source = reflected_value_name<Value>();
        StaticText<source.size()>  result {};
        for (std::size_t index = 0; index < source.size(); ++index) {
            result.bytes[index] = source[index];
        }
        return result;
    }

    template <typename T>
    inline constexpr auto ReflectedTypeText = make_type_text<T>();

    template <auto Value>
    inline constexpr auto ReflectedValueText = make_value_text<Value>();

    template <typename T>
    [[nodiscard]] constexpr const char* error_domain() noexcept {
        return ErrorDomainOverride<T>::value != nullptr
                   ? ErrorDomainOverride<T>::value
                   : ReflectedTypeText<T>.data();
    }

    template <auto Value>
    [[nodiscard]] constexpr const char* error_name() noexcept {
        return ErrorNameOverride<Value>::value != nullptr
                   ? ErrorNameOverride<Value>::value
                   : ReflectedValueText<Value>.data();
    }
} // namespace result_detail

template <std::size_t Depth>
/**
 * @brief 固定容量、无堆分配的 cause 存储。Fixed-capacity, allocation-free cause storage.
 *
 * 用户通常通过 `ErrorSet::cause()` 访问它，而不是直接构造 `ErrorTrace`。
 */
class ErrorTrace
{
    static_assert(Depth <= error_trace_depth::CHASE_IT_DOWN,
                  "Error trace depth cannot exceed CHASE_IT_DOWN");

public:
    [[nodiscard]] constexpr std::size_t size() const noexcept {
        return size_;
    }

    [[nodiscard]] constexpr bool truncated() const noexcept {
        return truncated_;
    }

    [[nodiscard]] constexpr const ErrorFrame& operator[](std::size_t index) const noexcept {
        return frames_[index];
    }

    constexpr void append(ErrorFrame frame) noexcept {
        if (size_ < Depth) {
            frames_[size_++] = frame;
        }
        else {
            truncated_ = true;
        }
    }

    constexpr void mark_truncated() noexcept {
        truncated_ = true;
    }

private:
    ErrorFrame  frames_[Depth] {};
    std::size_t size_ {};
    bool        truncated_ {};
};

template <>
class ErrorTrace<0>
{
public:
    [[nodiscard]] static constexpr std::size_t size() noexcept {
        return 0;
    }

    [[nodiscard]] constexpr bool truncated() const noexcept {
        return truncated_;
    }

    [[nodiscard]] constexpr ErrorFrame operator[](std::size_t) const noexcept {
        return {};
    }

    constexpr void append(ErrorFrame) noexcept {
        truncated_ = true;
    }

    constexpr void mark_truncated() noexcept {
        truncated_ = true;
    }

private:
    bool truncated_ {};
};

struct StaticErrorCause
{
    ErrorFrame frame;
};

/**
 * @brief 把旧式/外部错误包装成 cause。Adapt a legacy or external error into a cause frame.
 *
 * @code
 * return Err<DeviceError::DriverFailed>(
 *     "RMT setup failed",
 *     error_cause("RMT", nativeCode, "rmt_transmit failed"));
 * @endcode
 */
[[nodiscard]] constexpr StaticErrorCause error_cause(
    const char*  domain,
    std::int32_t code,
    const char*  message = "") noexcept {
    return {{
        domain != nullptr ? domain : "",
        "",
        code,
        message != nullptr ? message : "",
    }};
}

/** @overload 同时指定错误名称。Also supplies a symbolic error name. */
[[nodiscard]] constexpr StaticErrorCause error_cause(
    const char*  domain,
    const char*  name,
    std::int32_t code,
    const char*  message = "") noexcept {
    return {{
        domain != nullptr ? domain : "",
        name != nullptr ? name : "",
        code,
        message != nullptr ? message : "",
    }};
}

/**
 * @brief 只接受字符串字面量的非拥有错误消息。Non-owning message restricted to literals.
 *
 * 该包装器防止把局部数组、临时 `std::string` 或动态缓冲区悬挂进错误对象。
 * 调用 `Err<Code>("context")` 时由编译器自动构造，通常无需显式写出类型名。
 */
class StaticErrorMessage
{
public:
    template <std::size_t N>
    consteval StaticErrorMessage(const char (&message)[N])
        : value_(message) {
        static_assert(N > 1, "Error message cannot be empty");
    }

    [[nodiscard]] constexpr const char* get() const noexcept {
        return value_;
    }

private:
    const char* value_;
};

template <auto Code>
/// 编译期错误码及可选静态消息；通常由 `Err<Code>()` 创建。
struct ErrorConstant
{
    static constexpr auto value   = Code;
    const char*           message = nullptr;
};

template <auto Code, typename Cause>
/// 编译期错误码、可选静态消息以及一个底层 cause；通常由 `Err<Code>(..., cause)` 创建。
struct ErrorWithCause
{
    static constexpr auto value = Code;
    const char*           message;
    Cause                 cause;
};

template <auto Code>
/// 可用于显式构造 ErrorSet 的无消息错误常量，例如 `Errors e = error<MyError::Busy>;`。
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

/**
 * @brief 为 `ErrorSet::match()` 创建一个错误分支。Create one ErrorSet match arm.
 *
 * @code
 * int kind = error.match(
 *     on<MyError::Busy>([] { return 1; }),
 *     on<MyError::Timeout>([] { return 2; }));
 * @endcode
 * 每个允许的错误必须恰好出现一次，且所有 lambda 返回类型必须完全相同。
 */
template <auto Code, typename Function>
[[nodiscard]] constexpr auto on(Function&& function) {
    static_assert(!result_detail::IsTraceErrorSpecValue<Code>,
                  "on<> expects a raw error code, not TraceErrorSet<Depth, Code>");
    return ErrorCase<Code, std::decay_t<Function>> {
        std::forward<Function>(function),
    };
}

namespace result_detail
{

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

/**
 * @brief 闭合、类型安全、支持逐错误 trace 策略的错误集合。
 * Closed, type-safe error set with per-error trace policies.
 *
 * 错误身份是“代码类型 + 值”，因此两个不同 enum 的同一数值不会冲突。
 * 同一 enum 的同值别名则是同一个错误，不能重复列入集合。
 *
 * @tparam Depth 未显式覆盖时的默认 cause 深度，范围为 0..8。
 * @tparam Entries 裸错误码，或 `TraceErrorSet<Depth, Code>` 策略描述符。
 *
 * @code
 * enum class ReadError : uint8_t { Timeout, Disconnected };
 * using ReadErrors = ErrorSet<ReadError::Timeout, ReadError::Disconnected>;
 * using ReadResult = Result<int, ReadErrors>;
 *
 * using MixedDepthErrors = ErrorSet<
 *     ReadError::Disconnected, // default: four causes
 *     TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, ReadError::Timeout>>;
 * @endcode
 *
 * 调用点始终使用裸码，例如 `Err<ReadError::Timeout>()`。策略只写在集合声明中。
 * 对象大小由集合内最大的有效深度决定。
 */
template <std::size_t Depth, auto... Entries>
class TracedErrorSet
{
    static_assert(sizeof...(Entries) > 0, "TracedErrorSet must contain at least one error code");
    static_assert(Depth <= error_trace_depth::CHASE_IT_DOWN,
                  "Error trace depth cannot exceed CHASE_IT_DOWN");
    static_assert(result_detail::UniqueErrors<Entries...>::value,
                  "TracedErrorSet cannot contain duplicate error codes");
    static_assert(((result_detail::IsTraceErrorSpecValue<Entries> ||
                    std::is_enum_v<decltype(Entries)> ||
                    std::is_integral_v<decltype(Entries)>) &&
                   ...),
                  "ErrorSet entries must be error codes or TraceErrorSet<Depth, Code>");

public:
    using code_type = std::tuple_element_t<
        0,
        std::tuple<decltype(result_detail::entry_code<Entries>())...>>;
    static constexpr bool homogeneous =
        (std::same_as<code_type, decltype(result_detail::entry_code<Entries>())> && ...);

private:
    using ordinal_type = std::conditional_t<
        (sizeof...(Entries) <= std::numeric_limits<std::uint8_t>::max() + std::size_t {1}),
        std::uint8_t,
        std::conditional_t<
            (sizeof...(Entries) <= std::numeric_limits<std::uint16_t>::max() + std::size_t {1}),
            std::uint16_t,
            std::size_t>>;

    struct Unchecked
    {};

    ordinal_type                 ordinal_ {};
    const char*                  message_ = nullptr;
    static constexpr std::size_t storage_trace_depth_ =
        result_detail::maximum_entry_depth<Depth, Entries...>();

    [[no_unique_address]] ErrorTrace<storage_trace_depth_> trace_ {};

    constexpr TracedErrorSet(ordinal_type ordinal, const char* message, Unchecked)
        : ordinal_(ordinal), message_(message) {}

    template <auto Code>
    static constexpr bool contains_code_ = (result_detail::same_error<Code, Entries>() || ...);

    template <auto Code>
    [[nodiscard]] static consteval ordinal_type ordinal_for() {
        static_assert(contains_code_<Code>, "Error code is not part of this ErrorSet");
        std::size_t result = 0;
        std::size_t index  = 0;
        ((result_detail::same_error<Code, Entries>() ? result = index : result, ++index), ...);
        return static_cast<ordinal_type>(result);
    }

    template <auto Code>
    [[nodiscard]] static consteval std::size_t trace_depth_for_() {
        static_assert(!result_detail::IsTraceErrorSpecValue<Code>,
                      "trace_depth_for<> expects a raw error code");
        static_assert(contains_code_<Code>, "Error code is not part of this ErrorSet");
        std::size_t result = 0;
        ((result_detail::same_error<Code, Entries>()
              ? result = result_detail::entry_depth<Depth, Entries>()
              : result),
         ...);
        return result;
    }

    [[nodiscard]] constexpr std::size_t active_trace_depth_() const noexcept {
        std::size_t result = 0;
        std::size_t index  = 0;
        ([&] {
            if (ordinal_ == index)
                result = result_detail::entry_depth<Depth, Entries>();
            ++index;
        }(),
         ...);
        return result;
    }

    template <typename Code>
    [[nodiscard]] static constexpr std::optional<ordinal_type> find_ordinal(Code code) {
        std::optional<ordinal_type> result;
        std::size_t                 index = 0;
        ([&] {
            using EntryCode = decltype(result_detail::entry_code<Entries>());
            if constexpr (std::same_as<Code, EntryCode>) {
                if (code == result_detail::entry_code<Entries>()) {
                    result = static_cast<ordinal_type>(index);
                }
            }
            ++index;
        }(),
         ...);
        return result;
    }

    template <std::size_t OtherDepth, auto... Other>
    [[nodiscard]] static constexpr ordinal_type map_ordinal(
        const TracedErrorSet<OtherDepth, Other...>& other) {
        ordinal_type result {};
        ((other.template is<result_detail::entry_code<Other>()>()
              ? result = ordinal_for<result_detail::entry_code<Other>()>()
              : result),
         ...);
        return result;
    }

    template <typename Case>
    static constexpr auto case_code_ =
        result_detail::ErrorCaseInfo<std::remove_cvref_t<Case>>::code;

    template <auto Code, typename... Cases>
    static constexpr size_t case_count_ =
        (size_t {0} + ... + (result_detail::same_error<case_code_<Cases>, Code>() ? size_t {1} : size_t {0}));

    template <typename Current, typename... Rest>
    constexpr decltype(auto) dispatch(Current&& current, Rest&&... rest) const {
        if constexpr (sizeof...(Rest) == 0) {
            // Exhaustiveness and ErrorSet's private construction guarantee that
            // the final arm is the matching arm.
            return current.invoke();
        }
        else {
            if (ordinal_ == ordinal_for<case_code_<Current>>()) {
                return current.invoke();
            }

            return dispatch(std::forward<Rest>(rest)...);
        }
    }

    constexpr void append_cause(StaticErrorCause cause) noexcept {
        if (trace_.size() < active_trace_depth_())
            trace_.append(cause.frame);
        else
            trace_.mark_truncated();
    }

    template <std::size_t OtherDepth, auto... Other>
    constexpr void append_cause(const TracedErrorSet<OtherDepth, Other...>& cause) noexcept {
        append_cause(StaticErrorCause {cause.frame()});

        for (std::size_t index = 0; index < cause.cause_count(); ++index) {
            append_cause(StaticErrorCause {cause.cause(index)});
        }
        if (cause.truncated()) {
            trace_.mark_truncated();
        }
    }

public:
    /// 集合内最大的有效深度，也是对象实际预留的 cause 容量。
    static constexpr std::size_t trace_depth = storage_trace_depth_;

    /**
     * @brief 返回指定裸错误码在本集合中的最大 cause 深度。
     * @code
     * static_assert(Errors::trace_depth_for<Error::Important> == 8);
     * @endcode
     */
    template <auto Code>
    static constexpr std::size_t trace_depth_for = trace_depth_for_<Code>();

    /// 返回当前活动错误的最大 cause 深度；它可能小于集合的 `trace_depth`。
    [[nodiscard]] constexpr std::size_t active_trace_depth() const noexcept {
        return active_trace_depth_();
    }

    TracedErrorSet() = delete;

    template <auto Code>
        requires contains_code_<Code> && (!result_detail::IsTraceErrorSpecValue<Code>)
    /// 从 `ErrorConstant` 构造；通常由 `return Err<Code>("message")` 隐式调用。
    constexpr TracedErrorSet(ErrorConstant<Code> error)
        : ordinal_(ordinal_for<Code>()), message_(error.message) {}

    template <std::size_t OtherDepth, auto... Other>
        requires((contains_code_<result_detail::entry_code<Other>()>) && ...)
    /**
     * @brief 将错误真子集自动扩大到本集合。Widen a subset into this set.
     *
     * 顶层 code、message 和已有 cause chain 都会保留。反向缩小不会隐式发生。
     * @code
     * using Small = ErrorSet<E::Timeout>;
     * using Large = ErrorSet<E::Timeout, E::Busy>;
     * Large widened = Small::of<E::Timeout>();
     * @endcode
     */
    constexpr TracedErrorSet(const TracedErrorSet<OtherDepth, Other...>& other)
        : ordinal_(map_ordinal(other)),
          message_(other.has_message() ? other.message() : nullptr) {
        for (std::size_t index = 0; index < other.cause_count(); ++index) {
            append_cause(StaticErrorCause {other.cause(index)});
        }
        if (other.truncated()) {
            trace_.mark_truncated();
        }
    }

    template <auto Code, typename Cause>
        requires contains_code_<Code> && (!result_detail::IsTraceErrorSpecValue<Code>) &&
                     (std::same_as<std::remove_cvref_t<Cause>, StaticErrorCause> ||
                      requires(const Cause& cause) {
                          cause.frame();
                          cause.cause_count();
                      })
    /// 从顶层错误和底层 cause 构造；`Err<Code>(message, cause)` 会使用它。
    constexpr TracedErrorSet(ErrorWithCause<Code, Cause> error)
        : ordinal_(ordinal_for<Code>()), message_(error.message) {
        append_cause(error.cause);
    }

    template <auto Code>
        requires contains_code_<Code> && (!result_detail::IsTraceErrorSpecValue<Code>)
    /**
     * @brief 创建无消息错误。Create an error without a manual message.
     * @code
     * ReadErrors error = ReadErrors::of<ReadError::Timeout>();
     * @endcode
     */
    [[nodiscard]] static constexpr TracedErrorSet of() {
        return TracedErrorSet {
            ordinal_for<Code>(),
            nullptr,
            Unchecked {},
        };
    }

    template <typename Code>
    /// 若给定“类型 + 值”属于本集合则返回 true。Checks exact typed membership.
    [[nodiscard]] static constexpr bool contains(Code code) {
        return find_ordinal(code).has_value();
    }

    template <typename Code>
    /**
     * @brief 尝试把运行时错误码收窄进本集合。Try to narrow a runtime code.
     * @return 已知值返回 ErrorSet，未知值返回 `std::nullopt`。
     * @note `message` 必须具有足够长的生命周期；优先传字符串字面量。
     */
    [[nodiscard]] static constexpr std::optional<TracedErrorSet> try_from(
        Code        code,
        const char* message = nullptr) {
        const auto ordinal = find_ordinal(code);
        if (!ordinal.has_value()) {
            return std::nullopt;
        }

        return TracedErrorSet {
            *ordinal,
            message,
            Unchecked {},
        };
    }

    template <auto Fallback, typename Code>
        requires contains_code_<Fallback>
    /// 收窄运行时码；未知时返回编译期指定的 `Fallback`。
    [[nodiscard]] static constexpr TracedErrorSet narrow_or(Code code) {
        if (const auto found = try_from(code)) {
            return *found;
        }
        return of<Fallback>();
    }

    template <auto Code>
    /**
     * @brief 判断当前顶层错误是否为精确的 `Code`。Test the exact top-level error.
     * @code
     * if (result.error().is<ReadError::Timeout>()) { retry(); }
     * @endcode
     * 不属于集合的 `Code` 直接得到 false，便于泛型代码使用。
     */
    [[nodiscard]] constexpr bool is() const {
        static_assert(!result_detail::IsTraceErrorSpecValue<Code>,
                      "is<> expects a raw error code, not TraceErrorSet<Depth, Code>");
        if constexpr (contains_code_<Code>) {
            return ordinal_ == ordinal_for<Code>();
        }
        else {
            return false;
        }
    }

    /**
     * @brief 返回同构 ErrorSet 的类型化错误码。Return the typed code of a homogeneous set.
     *
     * 仅当集合内所有项属于同一类型时存在。异构集合请使用 `is<Code>()`、
     * `match()` 或 `code_as<Enum>()`。
     */
    [[nodiscard]] constexpr code_type code() const
        requires homogeneous
    {
        const auto value = code_as<code_type>();
        assert(value.has_value());
        return *value;
    }

    template <typename Code>
    /// 若当前错误属于指定 code 类型则返回它，否则返回 `std::nullopt`；适合异构集合。
    [[nodiscard]] constexpr std::optional<Code> code_as() const {
        std::optional<Code> result;
        std::size_t         index = 0;
        ([&] {
            using EntryCode = decltype(result_detail::entry_code<Entries>());
            if constexpr (std::same_as<Code, EntryCode>) {
                if (ordinal_ == index) {
                    result = result_detail::entry_code<Entries>();
                }
            }
            ++index;
        }(),
         ...);
        return result;
    }

    /// 自动生成的错误类型/域名，例如 `"ReadError"`。
    [[nodiscard]] constexpr const char* domain() const noexcept {
        const char* result = "";
        std::size_t index  = 0;
        ([&] {
            if (ordinal_ == index) {
                using EntryCode = decltype(result_detail::entry_code<Entries>());
                result          = result_detail::error_domain<EntryCode>();
            }
            ++index;
        }(),
         ...);
        return result;
    }

    /// 自动生成的枚举项名称，例如 `"Timeout"`。
    [[nodiscard]] constexpr const char* name() const noexcept {
        const char* result = "";
        std::size_t index  = 0;
        ([&] {
            if (ordinal_ == index) {
                result = result_detail::error_name<result_detail::entry_code<Entries>()>();
            }
            ++index;
        }(),
         ...);
        return result;
    }

    /// 用于日志和跨边界诊断的数值；不要用它代替类型安全的业务分支。
    [[nodiscard]] constexpr std::int32_t numeric_code() const noexcept {
        std::int32_t result = 0;
        std::size_t  index  = 0;
        ([&] {
            if (ordinal_ == index) {
                result = result_detail::diagnostic_code(result_detail::entry_code<Entries>());
            }
            ++index;
        }(),
         ...);
        return result;
    }

    /// 是否携带人工添加的上下文消息。
    [[nodiscard]] constexpr bool has_message() const {
        return message_ != nullptr;
    }

    /// 返回人工消息；未设置时返回空字符串而非 nullptr。
    [[nodiscard]] constexpr const char* message() const {
        return has_message() ? message_ : "";
    }

    /// 返回人工消息，若未设置则返回 `fallback`（nullptr fallback 会变为空字符串）。
    [[nodiscard]] constexpr const char* message_or(const char* fallback) const {
        return has_message() ? message_ : (fallback != nullptr ? fallback : "");
    }

    /// 将当前顶层错误导出为统一诊断 frame。
    [[nodiscard]] constexpr ErrorFrame frame() const noexcept {
        return {
            domain(),
            name(),
            numeric_code(),
            message(),
        };
    }

    /// 当前实际保存的底层 cause 数量，不包含顶层错误自身。
    [[nodiscard]] constexpr std::size_t cause_count() const noexcept {
        return trace_.size();
    }

    /// 按“离顶层最近到最远”的顺序读取 cause；越界返回空 frame。
    [[nodiscard]] constexpr ErrorFrame cause(std::size_t index) const noexcept {
        return index < cause_count() ? trace_[index] : ErrorFrame {};
    }

    template <typename Function>
        requires std::invocable<Function, const ErrorFrame&>
    /**
     * @brief 依次访问所有 cause。Visit causes from nearest to oldest.
     * @code
     * error.for_each_cause([](const ErrorFrame& frame) {
     *     log("%s::%s: %s", frame.domain, frame.name, frame.message);
     * });
     * @endcode
     */
    constexpr void for_each_cause(Function&& function) const {
        for (std::size_t index = 0; index < cause_count(); ++index) {
            std::invoke(function, trace_[index]);
        }
    }

    /// cause 是否因为所选固定深度不足而被截断。
    [[nodiscard]] constexpr bool truncated() const noexcept {
        return trace_.truncated();
    }

    template <result_detail::NullaryErrorCase... Cases>
        requires(sizeof...(Cases) > 0)
    /**
     * @brief 穷举匹配当前错误码。Exhaustively match the current error code.
     *
     * 必须使用 `on<Code>(lambda)` 将每个允许的错误恰好处理一次；所有 lambda
     * 返回类型必须完全相同。新增错误码后，遗漏分支会在编译期报错。
     * @code
     * const char* text = error.match(
     *     on<ReadError::Timeout>([] { return "timeout"; }),
     *     on<ReadError::Disconnected>([] { return "offline"; }));
     * @endcode
     */
    constexpr decltype(auto) match(Cases&&... cases) const {
        static_assert((contains_code_<case_code_<Cases>> && ...),
                      "ErrorSet::match contains an arm outside this error set");
        static_assert(((case_count_<result_detail::entry_code<Entries>(), Cases...> == 1) && ...),
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

template <auto... Allowed>
/**
 * @brief 裸错误默认保存 4 层 cause 的闭合错误集合。
 * Closed error set whose unwrapped entries default to four causes.
 *
 * 这是业务代码应该优先使用的写法：`using Errors = ErrorSet<E::A, E::B>;`。
 * 单个错误需要覆盖时写 `TraceErrorSet<Depth, E::B>`；其他错误仍使用默认值。
 */
using ErrorSet = TracedErrorSet<error_trace_depth::RUN_FOR_YOUR_LIFE, Allowed...>;

template <typename>
struct IsErrorSet : std::false_type
{};

template <std::size_t Depth, auto... Allowed>
struct IsErrorSet<TracedErrorSet<Depth, Allowed...>> : std::true_type
{};

template <typename E>
/// 判断一个类型是否为 `ErrorSet`/`TracedErrorSet`，供泛型约束使用。
concept ErrorSetType = IsErrorSet<std::remove_cvref_t<E>>::value;

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

/** @brief 创建 `Result<void, E>` 的成功值。Create a void success: `return Ok();`. */
[[nodiscard]] constexpr OkVoid Ok() {
    return {};
}

template <typename T>
/**
 * @brief 创建带值成功结果。Create a success carrying a value.
 * @code
 * Result<int, ReadErrors> read() { return Ok(42); }
 * @endcode
 */
[[nodiscard]] constexpr OkValue<std::decay_t<T>> Ok(T&& value) {
    return {
        std::forward<T>(value),
    };
}

template <typename E>
/// 转发一个已构造的 ErrorSet，常用于 `return Err(existingError);`。
[[nodiscard]] constexpr ErrValue<std::decay_t<E>> Err(E&& errorValue) {
    return {
        std::forward<E>(errorValue),
    };
}

template <auto Code>
/**
 * @brief 创建无消息的类型化错误。Create a typed error without a message.
 * @code
 * return Err<ReadError::Timeout>();
 * @endcode
 * 返回目标 `Result` 时会检查 `Code` 是否属于其 `ErrorSet`。
 */
[[nodiscard]] constexpr ErrValue<ErrorConstant<Code>> Err() {
    static_assert(!result_detail::IsTraceErrorSpecValue<Code>,
                  "Err<> expects a raw error code; declare TraceErrorSet<Depth, Code> inside ErrorSet instead");
    return {
        ErrorConstant<Code> {},
    };
}

template <auto Code>
/**
 * @brief 创建带静态上下文消息的类型化错误。Create a typed error with static context.
 * @code
 * return Err<ReadError::Timeout>("sensor did not answer");
 * @endcode
 * 只接受字符串字面量，错误对象不复制也不释放该字符串。
 */
[[nodiscard]] constexpr ErrValue<ErrorConstant<Code>> Err(StaticErrorMessage message) {
    static_assert(!result_detail::IsTraceErrorSpecValue<Code>,
                  "Err<> expects a raw error code; declare TraceErrorSet<Depth, Code> inside ErrorSet instead");
    return {
        ErrorConstant<Code> {.message = message.get()},
    };
}

template <auto Code, typename Cause>
/**
 * @brief 包装底层错误并自动建立 cause chain。Wrap a lower error as the cause.
 * @code
 * if (lower.is_err())
 *     return Err<DeviceError::DriverFailed>(lower.error());
 * @endcode
 * 若已有一个失败的 `Result`，通常更简洁地使用 `lower.propagate<Code>()`。
 */
[[nodiscard]] constexpr auto Err(Cause&& cause)
    requires(std::same_as<std::remove_cvref_t<Cause>, StaticErrorCause> ||
             requires(const std::remove_cvref_t<Cause>& value) {
                 value.frame();
                 value.cause_count();
             })
{
    static_assert(!result_detail::IsTraceErrorSpecValue<Code>,
                  "Err<> expects a raw error code; declare TraceErrorSet<Depth, Code> inside ErrorSet instead");
    using StoredCause = std::remove_cvref_t<Cause>;
    return ErrValue<ErrorWithCause<Code, StoredCause>> {
        ErrorWithCause<Code, StoredCause> {
            nullptr,
            std::forward<Cause>(cause),
        },
    };
}

template <auto Code, typename Cause>
/**
 * @brief 包装底层 cause，并为新的顶层错误添加静态消息。
 * Wrap a cause and add context to the new top-level error.
 * @code
 * return Err<DeviceError::DriverFailed>("device setup failed", lower.error());
 * // Equivalent shorter form when lower is a failed Result:
 * return lower.propagate<DeviceError::DriverFailed>("device setup failed");
 * @endcode
 */
[[nodiscard]] constexpr auto Err(StaticErrorMessage message, Cause&& cause) {
    static_assert(!result_detail::IsTraceErrorSpecValue<Code>,
                  "Err<> expects a raw error code; declare TraceErrorSet<Depth, Code> inside ErrorSet instead");
    using StoredCause = std::remove_cvref_t<Cause>;
    static_assert(std::same_as<StoredCause, StaticErrorCause> || requires(const StoredCause& value) {
                      value.frame();
                      value.cause_count(); }, "An error cause must be another ErrorSet or StaticErrorCause");
    return ErrValue<ErrorWithCause<Code, StoredCause>> {
        ErrorWithCause<Code, StoredCause> {
            message.get(),
            std::forward<Cause>(cause),
        },
    };
}

template <typename T, typename E>
class Result;

/**
 * @brief 延迟转换原生错误码的适配器。Lazy adapter from a native code to Result<void,E>.
 *
 * 用户通常通过 `NativeErr<Fallback>(...)`（ESP）创建它，而不是直接构造。
 */
template <typename NativeCode, auto Fallback>
struct NativeErrorProxy
{
    NativeCode  code;
    NativeCode  success;
    const char* domain;
    const char* name;
    const char* message;

    template <typename E>
    [[nodiscard]] constexpr operator Result<void, E>() const;
};

/**
 * @brief 成功值 `T` 或闭合错误集合 `E`。A success value `T` or closed error set `E`.
 *
 * @tparam T 必须是可平凡复制的非 void 类型；无返回值请使用 `Result<void,E>`。
 * @tparam E 必须是可平凡复制的 `ErrorSet<...>`。
 *
 * @code
 * enum class ReadError : uint8_t { Timeout, Disconnected };
 * using ReadErrors = ErrorSet<ReadError::Timeout, ReadError::Disconnected>;
 * using ReadResult = Result<int, ReadErrors>;
 *
 * ReadResult read(bool online) {
 *     if (!online) return Err<ReadError::Disconnected>("sensor offline");
 *     return Ok(42);
 * }
 * @endcode
 *
 * 对象不分配堆内存。调用 `value()/unwrap()` 前必须确认 `is_ok()`；调用
 * `error()/unwrap_err()/propagate()` 前必须确认 `is_err()`。
 */
template <typename T, typename E>
class [[nodiscard]] Result
{
    static_assert(!std::is_void_v<T>, "Use Result<void, E> for a result without a value");
    static_assert(ErrorSetType<E>, "Result error type must be ErrorSet<...>");
    static_assert(std::is_trivially_copyable_v<T>, "MCU Result requires a trivially copyable value");
    static_assert(std::is_trivially_copyable_v<E>, "MCU Result requires a trivially copyable error");

    bool ok_;

    union
    {
        T value_;
        E error_;
    };

public:
    /// 成功值类型。The success value type.
    using value_type = T;
    /// 闭合错误集合类型。The closed error-set type.
    using error_type = E;

    template <typename U>
        requires std::constructible_from<T, U>
    constexpr Result(OkValue<U> ok)
        : ok_(true), value_(std::move(ok.value)) {}

    template <typename G>
        requires std::constructible_from<E, G>
    constexpr Result(ErrValue<G> err)
        : ok_(false), error_(std::move(err.error)) {}

    /// 成功时返回 true。
    [[nodiscard]] constexpr bool is_ok() const {
        return ok_;
    }

    /// 失败时返回 true。
    [[nodiscard]] constexpr bool is_err() const {
        return !ok_;
    }

    /// 与 `is_ok()` 相同；例如 `if (result) { ... }`。
    [[nodiscard]] constexpr explicit operator bool() const {
        return is_ok();
    }

    /// 返回成功值引用。前置条件：`is_ok()`；失败状态调用属于错误用法。
    [[nodiscard]] constexpr const T& value() const {
        return value_;
    }

    /// 返回错误集合引用。前置条件：`is_err()`；成功状态调用属于错误用法。
    [[nodiscard]] constexpr const E& error() const {
        return error_;
    }

    /// 按值返回成功值。前置条件与 `value()` 相同；嵌入式实现不会抛异常。
    [[nodiscard]] constexpr T unwrap() const {
        return value_;
    }

    /// 按值返回错误。前置条件与 `error()` 相同；嵌入式实现不会抛异常。
    [[nodiscard]] constexpr E unwrap_err() const {
        return error_;
    }

    /**
     * @brief 原样向上返回当前错误。Propagate the same error set.
     * @code
     * if (lower.is_err()) return lower.propagate();
     * @endcode
     * 目标 Result 的错误集合可以相同，也可以是包含它的超集。前置条件：`is_err()`。
     */
    [[nodiscard]] constexpr auto propagate() const {
        assert(is_err());
        return Err(error_);
    }

    template <auto Code>
    /**
     * @brief 用新的顶层 `Code` 包装当前错误，并自动把当前错误加入 cause chain。
     * @code
     * if (lower.is_err())
     *     return lower.propagate<ServiceError::DriverFailed>();
     * @endcode
     */
    [[nodiscard]] constexpr auto propagate() const {
        assert(is_err());
        return Err<Code>(error_);
    }

    template <auto Code>
    /**
     * @brief 包装并传播当前错误，同时为新顶层错误添加静态消息。
     * @code
     * return lower.propagate<ServiceError::DriverFailed>("LED setup failed");
     * @endcode
     * 这是跨层传播错误的首选写法，不需要手写 `lower.error()`。
     */
    [[nodiscard]] constexpr auto propagate(StaticErrorMessage message) const {
        assert(is_err());
        return Err<Code>(message, error_);
    }

    /// 成功时返回值，否则返回 `defaultValue`。不会执行回调。
    [[nodiscard]] constexpr T value_or(T defaultValue) const {
        return ok_ ? value_ : defaultValue;
    }

    template <typename Function>
        requires std::invocable<Function, const T&>
    /**
     * @brief 只转换成功值，错误原样透传。Transform only the success value.
     *
     * 回调 `U(const T&)` 产生 `Result<U,E>`；回调返回 void 时产生
     * `Result<void,E>`。失败时不调用回调。
     * @code
     * auto doubled = read().map([](int value) { return value * 2; });
     * auto consumed = read().map([](int value) { use(value); });
     * @endcode
     */
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
    /**
     * @brief 只转换错误集合，成功值原样保留。Transform only the error set.
     *
     * 回调必须返回另一个 `ErrorSet<...>`，结果类型为 `Result<T,G>`。
     * 若只是将错误子集扩大到超集，通常无需调用它，直接返回即可隐式扩大。
     * @code
     * auto remapped = read().map_err([](const ReadErrors&) {
     *     return ServiceErrors::of<ServiceError::ReadFailed>();
     * });
     * @endcode
     */
    constexpr auto map_err(Function&& function) const {
        using RawG = std::invoke_result_t<Function, const E&>;
        using G    = std::decay_t<RawG>;

        static_assert(!std::is_void_v<RawG>, "Result::map_err cannot produce a void error");
        static_assert(ErrorSetType<G>, "Result::map_err must produce ErrorSet<...>");

        if (ok_) {
            return Result<T, G> {Ok(value_)};
        }

        return Result<T, G> {
            Err(std::invoke(std::forward<Function>(function), error_)),
        };
    }

    template <typename U, typename Function>
        requires std::invocable<Function, const T&> && std::convertible_to<std::invoke_result_t<Function, const T&>, U>
    /**
     * @brief 成功时映射为普通值，失败时返回默认值。Map success or use a default.
     * @code
     * int displayValue = read().map_or(-1, [](int value) { return value * 2; });
     * @endcode
     * 返回的是普通 `U`，不再是 Result。
     */
    [[nodiscard]] constexpr U map_or(U defaultValue, Function&& function) const {
        if (ok_) {
            return static_cast<U>(
                std::invoke(std::forward<Function>(function), value_));
        }

        return defaultValue;
    }

    template <typename U, typename Function>
        requires std::invocable<Function, const E&> && std::convertible_to<std::invoke_result_t<Function, const E&>, U>
    /**
     * @brief 失败时把错误映射为普通值，成功时返回默认值。
     * Map an error or use the default for success.
     * @code
     * int category = read().map_err_or(0, [](const ReadErrors& e) {
     *     return e.is<ReadError::Timeout>() ? 1 : 2;
     * });
     * @endcode
     */
    [[nodiscard]] constexpr U map_err_or(U defaultValue, Function&& function) const {
        if (ok_) {
            return defaultValue;
        }

        return static_cast<U>(
            std::invoke(std::forward<Function>(function), error_));
    }

    template <typename Function>
        requires std::invocable<Function, const T&>
    /**
     * @brief 成功后继续执行另一个可能失败的操作。Chain a fallible success operation.
     *
     * 回调必须返回另一个 `Result<U,G>`，且该结果必须能接收当前错误 `E`。
     * 当前对象失败时回调不会执行，原错误直接透传。
     * @code
     * auto frame = read().and_then([](int value) -> EncodeResult {
     *     return encode(value);
     * });
     * @endcode
     */
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
    /**
     * @brief 失败时尝试恢复，成功值保持不变。Recover from an error.
     *
     * 回调必须返回能接收当前成功值 `T` 的 Result。成功时不执行回调。
     * @code
     * auto value = read().or_else([](const ReadErrors&) -> ReadResult {
     *     return Ok(0); // fallback value
     * });
     * @endcode
     */
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
    /**
     * @brief 将成功和失败两种状态穷尽折叠成同一种返回类型。
     * Fold both Result states into one identical return type.
     * @code
     * const char* text = read().match(
     *     [](int) { return "ok"; },
     *     [](const ReadErrors&) { return "failed"; });
     * @endcode
     * 两个 lambda 的返回类型必须完全相同；只执行与当前状态对应的一支。
     */
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

/**
 * @brief 无成功值的 Result 特化。Result specialization for operations returning no value.
 *
 * @code
 * using SetupResult = Result<void, SetupErrors>;
 * SetupResult setup() {
 *     if (badConfig) return Err<SetupError::InvalidConfig>();
 *     return Ok();
 * }
 * @endcode
 *
 * 它仍完整支持 `map/map_err/and_then/or_else/match`；成功回调不接收参数。
 */
template <typename E>
class [[nodiscard]] Result<void, E>
{
    static_assert(ErrorSetType<E>, "Result error type must be ErrorSet<...>");
    static_assert(std::is_trivially_copyable_v<E>, "MCU Result requires a trivially copyable error");

    bool ok_;

    union
    {
        E error_;
    };

public:
    /// 固定为 void。
    using value_type = void;
    /// 闭合错误集合类型。
    using error_type = E;

    constexpr Result(OkVoid)
        : ok_(true) {}

    template <typename G>
        requires std::constructible_from<E, G>
    constexpr Result(ErrValue<G> err)
        : ok_(false), error_(std::move(err.error)) {}

    /// 成功时返回 true。
    [[nodiscard]] constexpr bool is_ok() const {
        return ok_;
    }

    /// 失败时返回 true。
    [[nodiscard]] constexpr bool is_err() const {
        return !ok_;
    }

    /// 与 `is_ok()` 相同；例如 `if (setup()) { ... }`。
    [[nodiscard]] constexpr explicit operator bool() const {
        return is_ok();
    }

    /// 返回错误引用。前置条件：`is_err()`。
    [[nodiscard]] constexpr const E& error() const {
        return error_;
    }

    /// 确认/消费成功状态；返回 void。前置条件：`is_ok()`。
    constexpr void unwrap() const {}

    /// 按值返回错误。前置条件：`is_err()`。
    [[nodiscard]] constexpr E unwrap_err() const {
        return error_;
    }

    /// 原样传播当前错误；目标错误集合可以相同或为其超集。前置条件：`is_err()`。
    [[nodiscard]] constexpr auto propagate() const {
        assert(is_err());
        return Err(error_);
    }

    template <auto Code>
    /// 用新的顶层 `Code` 包装当前错误，当前错误自动进入 cause chain。
    [[nodiscard]] constexpr auto propagate() const {
        assert(is_err());
        return Err<Code>(error_);
    }

    template <auto Code>
    /**
     * @brief 包装并传播当前错误，同时添加静态上下文。
     * @code
     * if (driver.is_err())
     *     return driver.propagate<SetupError::DriverFailed>("driver setup failed");
     * @endcode
     */
    [[nodiscard]] constexpr auto propagate(StaticErrorMessage message) const {
        assert(is_err());
        return Err<Code>(message, error_);
    }

    template <typename Function>
        requires std::invocable<Function>
    /**
     * @brief 成功时执行无参转换，错误原样透传。Transform a void success.
     *
     * 返回值回调产生 `Result<U,E>`，void 回调产生 `Result<void,E>`。
     * @code
     * auto ready = setup().map([] { return DeviceState::Ready; });
     * auto notified = setup().map([] { notifyReady(); });
     * @endcode
     */
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
    /**
     * @brief 只转换错误集合；成功仍为 void success。
     * Transform only the error set while preserving success.
     * 回调必须返回另一个 `ErrorSet<...>`。
     */
    constexpr auto map_err(Function&& function) const {
        using RawG = std::invoke_result_t<Function, const E&>;
        using G    = std::decay_t<RawG>;

        static_assert(!std::is_void_v<RawG>, "Result::map_err cannot produce a void error");
        static_assert(ErrorSetType<G>, "Result::map_err must produce ErrorSet<...>");

        if (ok_) {
            return Result<void, G> {Ok()};
        }

        return Result<void, G> {
            Err(std::invoke(std::forward<Function>(function), error_)),
        };
    }

    template <typename U, typename Function>
        requires std::invocable<Function> && std::convertible_to<std::invoke_result_t<Function>, U>
    /// 成功时调用无参函数产生普通 `U`，失败时返回 `defaultValue`。
    [[nodiscard]] constexpr U map_or(U defaultValue, Function&& function) const {
        if (ok_) {
            return static_cast<U>(
                std::invoke(std::forward<Function>(function)));
        }

        return defaultValue;
    }

    template <typename U, typename Function>
        requires std::invocable<Function, const E&> && std::convertible_to<std::invoke_result_t<Function, const E&>, U>
    /// 失败时把错误映射成普通 `U`，成功时返回 `defaultValue`。
    [[nodiscard]] constexpr U map_err_or(U defaultValue, Function&& function) const {
        if (ok_) {
            return defaultValue;
        }

        return static_cast<U>(
            std::invoke(std::forward<Function>(function), error_));
    }

    template <typename Function>
        requires std::invocable<Function>
    /**
     * @brief 成功后继续执行另一个可能失败的无参操作。
     * Chain another fallible operation after a void success.
     * @code
     * auto result = setupBus().and_then([] { return setupDevice(); });
     * @endcode
     * 当前对象失败时不执行回调，并将错误透传给回调的 Result 类型。
     */
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
    /**
     * @brief 失败时执行恢复操作；成功时保持 `Ok()` 且不调用回调。
     * @code
     * auto result = setup().or_else([](const SetupErrors&) -> SetupResult {
     *     resetHardware();
     *     return Ok();
     * });
     * @endcode
     */
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
    /**
     * @brief 穷尽处理 void success 和 error，并返回完全相同的类型。
     * @code
     * int status = setup().match(
     *     [] { return 0; },
     *     [](const SetupErrors&) { return -1; });
     * @endcode
     */
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

template <typename NativeCode, auto Fallback>
template <typename E>
[[nodiscard]] constexpr NativeErrorProxy<NativeCode, Fallback>::operator Result<void, E>() const {
    static_assert(ErrorSetType<E>, "Native errors can only convert to Result<void, ErrorSet<...>>");

    if (code == success) {
        return Ok();
    }

    if (const auto exact = E::try_from(code, message)) {
        return Err(*exact);
    }

    const StaticErrorCause cause = error_cause(
        domain,
        name,
        result_detail::diagnostic_code(code));
    return Err(ErrorWithCause<Fallback, StaticErrorCause> {
        message,
        cause,
    });
}

/**
 * @brief 将“指定值代表成功”的原生错误码转换为闭合 Result。
 * Convert a native success-code API into a closed Result error set.
 *
 * 已列入 `E` 的原生码保留其精确类型和值。未知码先交给 `onUnexpected(code)`
 * 记录，再转换为 `Fallback`。此旧接口要求同构 ErrorSet；ESP-IDF 与异构集合
 * 优先使用 `NativeErr<Fallback>()`。
 *
 * @tparam E 目标 ErrorSet。
 * @tparam Fallback 未知原生码使用的错误，必须属于 E。
 * @tparam Success 原生 API 的成功值，默认为 0。
 * @code
 * return from_native<DriverErrors, DriverError::NativeFailure>(
 *     native_call(),
 *     [](int code) { logUnexpected(code); });
 * @endcode
 */
template <typename E, auto Fallback, auto Success = 0, typename Code, typename UnexpectedFunction>
    requires std::invocable<UnexpectedFunction, Code>
[[nodiscard]] constexpr Result<void, E>
from_native(Code code, UnexpectedFunction&& onUnexpected) {
    static_assert(E::homogeneous,
                  "from_native requires a homogeneous ErrorSet; use NativeErr for heterogeneous sets");
    if (code == static_cast<Code>(Success)) {
        return Ok();
    }

    if (const auto error = E::try_from(static_cast<typename E::code_type>(code))) {
        return Err(*error);
    }

    std::invoke(
        std::forward<UnexpectedFunction>(onUnexpected),
        code);
    return Err(E::template of<Fallback>());
}
