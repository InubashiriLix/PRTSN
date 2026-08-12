#include <cassert>
#include <string_view>
#include <type_traits>

#include "src/fw/inc/Result.h"

enum class DriverError : unsigned char
{
    Timeout = 1,
    Disconnected,
};

enum class DeviceError : unsigned char
{
    TransportFailed = 1,
    InvalidState,
};

enum class ServiceError : unsigned char
{
    SetupFailed,
    Busy,
};

enum class AliasedError : int
{
    InvalidArgs     = 5,
    InvalidArgument = 5,
};

enum class NativeCode : int
{
    Ok      = 0,
    Known   = 7,
    Unknown = 99,
};

enum class PolicyError : unsigned char
{
    DefaultTrace,
    NoTrace,
    TwoTrace,
    DeepTrace,
};

namespace
{
    using DriverErrors  = ErrorSet<DriverError::Timeout, DriverError::Disconnected>;
    using DriverSubset  = ErrorSet<DriverError::Timeout>;
    using DeviceErrors  = ErrorSet<DeviceError::TransportFailed, DeviceError::InvalidState>;
    using ServiceErrors = ErrorSet<ServiceError::SetupFailed, ServiceError::Busy>;
    using MixedErrors   = ErrorSet<DriverError::Timeout, DeviceError::TransportFailed>;
    using NativeErrors  = ErrorSet<NativeCode::Known, ServiceError::SetupFailed>;
    using PolicyErrors  = ErrorSet<
        PolicyError::DefaultTrace,
        TraceErrorSet<error_trace_depth::AMPUTATION, PolicyError::NoTrace>,
        TraceErrorSet<2, PolicyError::TwoTrace>,
        TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, PolicyError::DeepTrace>>;

    using DriverResult     = Result<int, DriverErrors>;
    using DriverVoidResult = Result<void, DriverErrors>;
    using DeviceResult     = Result<int, DeviceErrors>;
    using ServiceResult    = Result<int, ServiceErrors>;

    static_assert(ErrorSetType<DriverErrors>);
    static_assert(!ErrorSetType<DriverError>);
    static_assert(std::is_trivially_copyable_v<DriverErrors>);
    static_assert(std::is_trivially_copyable_v<DriverResult>);
    static_assert(std::is_convertible_v<DriverSubset, DriverErrors>);
    static_assert(!std::is_convertible_v<DriverErrors, DriverSubset>);
    static_assert(DriverErrors::homogeneous);
    static_assert(!MixedErrors::homogeneous);
    static_assert(result_detail::same_error<AliasedError::InvalidArgs, AliasedError::InvalidArgument>());
    static_assert(!result_detail::same_error<DriverError::Timeout, DeviceError::TransportFailed>());
    static_assert(result_detail::same_error<
                  PolicyError::DeepTrace,
                  TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, PolicyError::DeepTrace>>());
    static_assert(PolicyErrors::trace_depth == error_trace_depth::CHASE_IT_DOWN);
    static_assert(PolicyErrors::trace_depth_for<PolicyError::DefaultTrace> ==
                  error_trace_depth::RUN_FOR_YOUR_LIFE);
    static_assert(PolicyErrors::trace_depth_for<PolicyError::NoTrace> == 0);
    static_assert(PolicyErrors::trace_depth_for<PolicyError::TwoTrace> == 2);
    static_assert(PolicyErrors::trace_depth_for<PolicyError::DeepTrace> ==
                  error_trace_depth::CHASE_IT_DOWN);
    static_assert(PolicyErrors::homogeneous);
    static_assert(std::is_trivially_copyable_v<PolicyErrors>);

    using NoTrace     = TracedErrorSet<error_trace_depth::AMPUTATION, DriverError::Timeout>;
    using TwoTrace    = TracedErrorSet<2, DriverError::Timeout>;
    using BottomTrace = TracedErrorSet<error_trace_depth::CHASE_IT_DOWN, DriverError::Timeout>;
    static_assert(sizeof(NoTrace) < sizeof(TwoTrace));
    static_assert(sizeof(TwoTrace) < sizeof(BottomTrace));

    constexpr DriverResult readDriver(bool fail) {
        if (fail) {
            return Err<DriverError::Timeout>("driver timed out");
        }
        return Ok(21);
    }

    constexpr DriverVoidResult initializeDriver(bool fail) {
        if (fail) {
            return Err<DriverError::Disconnected>("driver initialization failed");
        }
        return Ok();
    }

    constexpr DeviceResult readDevice(bool fail) {
        const auto driver = readDriver(fail);
        if (driver.is_err()) {
            return driver.propagate<DeviceError::TransportFailed>(
                "device transport failed");
        }
        return Ok(driver.value() * 2);
    }

    constexpr ServiceResult readService(bool fail) {
        const auto device = readDevice(fail);
        if (device.is_err()) {
            return device.propagate<ServiceError::SetupFailed>(
                "service setup failed");
        }
        return Ok(device.value());
    }

    constexpr int classify(const DriverErrors& error) {
        return error.match(
            on<DriverError::Timeout>([] { return 1; }),
            on<DriverError::Disconnected>([] { return 2; }));
    }

    constexpr int classifyMixed(const MixedErrors& error) {
        return error.match(
            on<DriverError::Timeout>([] { return 10; }),
            on<DeviceError::TransportFailed>([] { return 20; }));
    }

    static_assert(readService(false).value() == 42);
    static_assert(readDriver(true).error().has_message());
    static_assert(readDriver(true).error().is<DriverError::Timeout>());
    static_assert(readDriver(true).error().code() == DriverError::Timeout);
    static_assert(classify(readDriver(true).error()) == 1);
    static_assert(readService(true).error().cause_count() == 2);
    static_assert(readService(true).error().cause(0).numericCode == static_cast<int>(DeviceError::TransportFailed));
    static_assert(readService(true).error().cause(1).numericCode == static_cast<int>(DriverError::Timeout));

    template <PolicyError Code>
    constexpr Result<void, PolicyErrors> wrapServiceWithPolicy() {
        const auto lower = readService(true);
        return lower.propagate<Code>("policy wrapper");
    }

    constexpr auto defaultPolicy   = wrapServiceWithPolicy<PolicyError::DefaultTrace>();
    constexpr auto noTracePolicy   = wrapServiceWithPolicy<PolicyError::NoTrace>();
    constexpr auto twoTracePolicy  = wrapServiceWithPolicy<PolicyError::TwoTrace>();
    constexpr auto deepTracePolicy = wrapServiceWithPolicy<PolicyError::DeepTrace>();
    static_assert(defaultPolicy.error().cause_count() == 3);
    static_assert(!defaultPolicy.error().truncated());
    static_assert(defaultPolicy.error().active_trace_depth() == 4);
    static_assert(noTracePolicy.error().cause_count() == 0);
    static_assert(noTracePolicy.error().truncated());
    static_assert(noTracePolicy.error().active_trace_depth() == 0);
    static_assert(twoTracePolicy.error().cause_count() == 2);
    static_assert(twoTracePolicy.error().truncated());
    static_assert(twoTracePolicy.error().active_trace_depth() == 2);
    static_assert(deepTracePolicy.error().cause_count() == 3);
    static_assert(!deepTracePolicy.error().truncated());
    static_assert(deepTracePolicy.error().active_trace_depth() == 8);
    static_assert(deepTracePolicy.error().is<PolicyError::DeepTrace>());
    static_assert(deepTracePolicy.error().code() == PolicyError::DeepTrace);

    constexpr auto policyMapped      = deepTracePolicy.map([] { return 7; });
    constexpr auto policyMappedError = deepTracePolicy.map_err([](const PolicyErrors&) {
        return DriverErrors::of<DriverError::Disconnected>();
    });
    constexpr auto policyChained     = deepTracePolicy.and_then([]() -> Result<int, PolicyErrors> {
        return Ok(9);
    });
    constexpr auto policyRecovered   = deepTracePolicy.or_else([](const PolicyErrors&) -> Result<void, PolicyErrors> {
        return Ok();
    });
    static_assert(policyMapped.error().is<PolicyError::DeepTrace>());
    static_assert(policyMappedError.error().is<DriverError::Disconnected>());
    static_assert(policyChained.error().is<PolicyError::DeepTrace>());
    static_assert(policyRecovered.is_ok());

    constexpr int classifyPolicy(const PolicyErrors& error) {
        return error.match(
            on<PolicyError::DefaultTrace>([] { return 1; }),
            on<PolicyError::NoTrace>([] { return 2; }),
            on<PolicyError::TwoTrace>([] { return 3; }),
            on<PolicyError::DeepTrace>([] { return 4; }));
    }
    static_assert(classifyPolicy(deepTracePolicy.error()) == 4);

    using DeepOnly = ErrorSet<
        TraceErrorSet<error_trace_depth::CHASE_IT_DOWN, PolicyError::DeepTrace>>;
    using ShallowDeep = ErrorSet<TraceErrorSet<2, PolicyError::DeepTrace>>;

    constexpr Result<void, DeepOnly> makeDeepOnly() {
        const auto lower = readService(true);
        return lower.propagate<PolicyError::DeepTrace>("deep source");
    }

    constexpr Result<void, ShallowDeep> narrowDeepPolicy() {
        const auto source = makeDeepOnly();
        return source.propagate();
    }

    constexpr Result<void, DeepOnly> widenDeepPolicyAgain() {
        const auto source = narrowDeepPolicy();
        return source.propagate();
    }

    static_assert(makeDeepOnly().error().cause_count() == 3);
    static_assert(narrowDeepPolicy().error().cause_count() == 2);
    static_assert(narrowDeepPolicy().error().truncated());
    static_assert(widenDeepPolicyAgain().error().cause_count() == 2);
    static_assert(widenDeepPolicyAgain().error().truncated());

    constexpr auto mixedDriver = MixedErrors::of<DriverError::Timeout>();
    constexpr auto mixedDevice = MixedErrors::of<DeviceError::TransportFailed>();
    static_assert(mixedDriver.is<DriverError::Timeout>());
    static_assert(!mixedDriver.is<DeviceError::TransportFailed>());
    static_assert(classifyMixed(mixedDriver) == 10);
    static_assert(classifyMixed(mixedDevice) == 20);
    static_assert(mixedDriver.code_as<DriverError>().has_value());
    static_assert(!mixedDriver.code_as<DeviceError>().has_value());
    static_assert(MixedErrors::try_from(DriverError::Timeout).has_value());
    static_assert(MixedErrors::try_from(DeviceError::TransportFailed).has_value());

    static_assert(std::string_view(DriverErrors::of<DriverError::Timeout>().domain()) == "DriverError");
    static_assert(std::string_view(DriverErrors::of<DriverError::Timeout>().name()) == "Timeout");

    constexpr auto widened = DriverErrors {DriverSubset::of<DriverError::Timeout>()};
    static_assert(widened.is<DriverError::Timeout>());

    constexpr Result<void, DriverErrors> propagateSubset() {
        const Result<int, DriverSubset> lower = Err<DriverError::Timeout>("subset failure");
        return lower.propagate();
    }
    static_assert(propagateSubset().error().is<DriverError::Timeout>());
    static_assert(propagateSubset().error().has_message());

    // Value Result combinators: verify both success and error branches.
    static_assert(readDriver(false).is_ok());
    static_assert(readDriver(true).is_err());
    static_assert(static_cast<bool>(readDriver(false)));
    static_assert(!static_cast<bool>(readDriver(true)));
    static_assert(readDriver(false).unwrap() == 21);
    static_assert(readDriver(true).unwrap_err().is<DriverError::Timeout>());
    static_assert(readDriver(false).value_or(-1) == 21);
    static_assert(readDriver(true).value_or(-1) == -1);

    constexpr auto mappedValue      = readDriver(false).map([](int value) { return value * 2; });
    constexpr auto mappedValueError = readDriver(true).map([](int value) { return value * 2; });
    constexpr auto mappedVoid       = readDriver(false).map([](int) {});
    constexpr auto mappedVoidError  = readDriver(true).map([](int) {});
    static_assert(mappedValue.value() == 42);
    static_assert(mappedValueError.error().is<DriverError::Timeout>());
    static_assert(mappedVoid.is_ok());
    static_assert(mappedVoidError.error().is<DriverError::Timeout>());

    constexpr auto mappedErrorSuccess = readDriver(false).map_err([](const DriverErrors&) {
        return ServiceErrors::of<ServiceError::Busy>();
    });
    constexpr auto mappedError        = readDriver(true).map_err([](const DriverErrors&) {
        return ServiceErrors {ErrorConstant<ServiceError::Busy> {.message = "mapped error"}};
    });
    static_assert(mappedErrorSuccess.value() == 21);
    static_assert(mappedError.error().is<ServiceError::Busy>());
    static_assert(mappedError.error().has_message());

    static_assert(readDriver(false).map_or(-1, [](int value) { return value * 2; }) == 42);
    static_assert(readDriver(true).map_or(-1, [](int value) { return value * 2; }) == -1);
    static_assert(readDriver(false).map_err_or(0, [](const DriverErrors&) { return 9; }) == 0);
    static_assert(readDriver(true).map_err_or(0, [](const DriverErrors&) { return 9; }) == 9);

    constexpr auto chained            = readDriver(false).and_then([](int value) -> DriverResult {
        return Ok(value + 1);
    });
    constexpr auto chainedInputError  = readDriver(true).and_then([](int value) -> DriverResult {
        return Ok(value + 1);
    });
    constexpr auto chainedOutputError = readDriver(false).and_then([](int) -> DriverResult {
        return Err<DriverError::Disconnected>();
    });
    static_assert(chained.value() == 22);
    static_assert(chainedInputError.error().is<DriverError::Timeout>());
    static_assert(chainedOutputError.error().is<DriverError::Disconnected>());

    constexpr auto untouched = readDriver(false).or_else([](const DriverErrors&) -> DriverResult {
        return Ok(7);
    });
    constexpr auto recovered = readDriver(true).or_else([](const DriverErrors&) -> DriverResult {
        return Ok(7);
    });
    static_assert(untouched.value() == 21);
    static_assert(recovered.value() == 7);

    static_assert(readDriver(false).match(
                      [](int value) { return value; },
                      [](const DriverErrors&) { return -1; }) == 21);
    static_assert(readDriver(true).match(
                      [](int value) { return value; },
                      [](const DriverErrors&) { return -1; }) == -1);

    // Void Result has the same combinator surface, minus value_or().
    constexpr auto voidMappedValue      = initializeDriver(false).map([] { return 8; });
    constexpr auto voidMappedValueError = initializeDriver(true).map([] { return 8; });
    constexpr auto voidMappedVoid       = initializeDriver(false).map([] {});
    constexpr auto voidMappedVoidError  = initializeDriver(true).map([] {});
    static_assert(voidMappedValue.value() == 8);
    static_assert(voidMappedValueError.error().is<DriverError::Disconnected>());
    static_assert(voidMappedVoid.is_ok());
    static_assert(voidMappedVoidError.error().is<DriverError::Disconnected>());

    constexpr auto voidMappedErrorSuccess = initializeDriver(false).map_err([](const DriverErrors&) {
        return ServiceErrors::of<ServiceError::Busy>();
    });
    constexpr auto voidMappedError        = initializeDriver(true).map_err([](const DriverErrors&) {
        return ServiceErrors::of<ServiceError::Busy>();
    });
    static_assert(voidMappedErrorSuccess.is_ok());
    static_assert(voidMappedError.error().is<ServiceError::Busy>());

    static_assert(initializeDriver(false).map_or(-1, [] { return 8; }) == 8);
    static_assert(initializeDriver(true).map_or(-1, [] { return 8; }) == -1);
    static_assert(initializeDriver(false).map_err_or(0, [](const DriverErrors&) { return 9; }) == 0);
    static_assert(initializeDriver(true).map_err_or(0, [](const DriverErrors&) { return 9; }) == 9);

    constexpr auto voidChained            = initializeDriver(false).and_then([]() -> DriverResult {
        return Ok(11);
    });
    constexpr auto voidChainedError       = initializeDriver(true).and_then([]() -> DriverResult {
        return Ok(11);
    });
    constexpr auto voidChainedOutputError = initializeDriver(false).and_then([]() -> DriverResult {
        return Err<DriverError::Timeout>();
    });
    static_assert(voidChained.value() == 11);
    static_assert(voidChainedError.error().is<DriverError::Disconnected>());
    static_assert(voidChainedOutputError.error().is<DriverError::Timeout>());

    constexpr auto voidUntouched = initializeDriver(false).or_else([](const DriverErrors&) -> DriverVoidResult {
        return Ok();
    });
    constexpr auto voidRecovered = initializeDriver(true).or_else([](const DriverErrors&) -> DriverVoidResult {
        return Ok();
    });
    static_assert(voidUntouched.is_ok());
    static_assert(voidRecovered.is_ok());

    static_assert(initializeDriver(false).match(
                      [] { return 1; },
                      [](const DriverErrors&) { return -1; }) == 1);
    static_assert(initializeDriver(true).match(
                      [] { return 1; },
                      [](const DriverErrors&) { return -1; }) == -1);

    using OneCauseService = TracedErrorSet<1, ServiceError::SetupFailed>;
    constexpr Result<void, OneCauseService> truncatedChain() {
        const auto device = readDevice(true);
        return device.propagate<ServiceError::SetupFailed>("top");
    }
    static_assert(truncatedChain().error().cause_count() == 1);
    static_assert(truncatedChain().error().truncated());

    constexpr Result<void, NoTrace> amputatedChain() {
        return Err<DriverError::Timeout>(
            "top",
            error_cause("native", "E_FAIL", -1, "native failure"));
    }
    static_assert(amputatedChain().error().cause_count() == 0);
    static_assert(amputatedChain().error().truncated());

    constexpr Result<void, NativeErrors> nativeResult(NativeCode code) {
        return NativeErrorProxy<NativeCode, ServiceError::SetupFailed> {
            code,
            NativeCode::Ok,
            "native_api",
            code == NativeCode::Known ? "Known" : "Unknown",
            "native call failed",
        };
    }

    static_assert(nativeResult(NativeCode::Ok).is_ok());
    static_assert(nativeResult(NativeCode::Known).error().is<NativeCode::Known>());
    static_assert(nativeResult(NativeCode::Known).error().has_message());
    static_assert(nativeResult(NativeCode::Unknown).error().is<ServiceError::SetupFailed>());
    static_assert(nativeResult(NativeCode::Unknown).error().cause_count() == 1);
    static_assert(nativeResult(NativeCode::Unknown).error().cause(0).numericCode == 99);

    constexpr auto nativeOk = from_native<DriverErrors, DriverError::Disconnected>(
        0,
        [](int) {});
    static_assert(nativeOk.is_ok());
}

int main() {
    const auto failure = readService(true);
    assert(failure.is_err());
    assert(failure.error().is<ServiceError::SetupFailed>());
    assert(std::string_view(failure.error().message()) == "service setup failed");
    assert(std::string_view(failure.error().domain()) == "ServiceError");
    assert(std::string_view(failure.error().name()) == "SetupFailed");
    assert(failure.error().cause_count() == 2);
    assert(std::string_view(failure.error().cause(0).domain) == "DeviceError");
    assert(std::string_view(failure.error().cause(0).name) == "TransportFailed");
    assert(std::string_view(failure.error().cause(1).domain) == "DriverError");
    assert(std::string_view(failure.error().cause(1).name) == "Timeout");
}
