#set document(title: "PRTN ResultV2", author: "PRTN Project")
#set page(paper: "a4", margin: 20mm)
#set text(font: "Noto Sans CJK SC", lang: "zh", size: 10pt)
#set par(justify: true, leading: .75em)
#show raw: set text(font: "JetBrains Maple Mono", size: 8pt)

= PRTN ResultV2：组合式错误 / Composed errors

`Result<T, E>` 表示成功值 `T` 或错误 `E`。错误侧必须是闭合的
`ErrorSet`，不能直接使用枚举、字符串或任意结构体。

`Result<T, E>` contains either a success value `T` or an error `E`. The error
type must be a closed `ErrorSet`; raw enums, strings, and arbitrary structs are
not accepted.

```cpp
enum class SensorError : uint8_t {
    Disconnected,
    Timeout,
};

using ReadErrors = ErrorSet<
    SensorError::Disconnected,
    SensorError::Timeout>;

using ReadResult = Result<int, ReadErrors>;
```

== 最简单的返回 / Basic returns

```cpp
ReadResult read(bool connected) {
    if (!connected)
        return Err<SensorError::Disconnected>();
    return Ok(42);
}
```

`Err<Code>()` 在编译期检查 `Code` 是否属于返回类型声明的集合。
`code()` 返回类型化顶层错误码，`is<Code>()` 用于分支判断。

`Err<Code>()` checks at compile time that `Code` belongs to the declared error
set. Use `code()` for the typed top-level code and `is<Code>()` for branching.

```cpp
if (result.is_err() && result.error().is<SensorError::Timeout>()) {
    // recovery logic
}
```

== 人工消息 / Manual context

错误可以附带静态字符串消息。消息不拥有内存，因此只接受具有静态生命周期
的字符串；通常直接使用字符串字面量。

An error may carry a static message. Messages are non-owning, so use string
literals or other static-lifetime strings.

```cpp
return Err<SensorError::Timeout>(
    "sensor did not answer before the deadline");

error.has_message();
error.message();                 // "" when absent
error.message_or("no details");
```

不要传入局部字符数组、临时 `std::string` 或动态生成的缓冲区；接口会尽量在
编译期拒绝这些用法。

Do not pass local character arrays, temporary `std::string` objects, or generated
buffers. The API rejects these unsafe lifetimes at compile time where possible.

== 包装底层错误 / Wrap a lower-layer failure

上层产生自己的类型化错误码，同时把底层错误作为 cause 组合进去。业务逻辑
只判断顶层 code；cause chain 只用于诊断。

An upper layer creates its own typed code and composes the lower error as a cause.
Application control flow uses only the top-level code; causes are diagnostic data.

```cpp
using DriverErrors = ErrorSet<DriverError::Timeout>;
using DeviceErrors = ErrorSet<DeviceError::TransportFailed>;

Result<void, DeviceErrors> setupDevice() {
    const Result<void, DriverErrors> lower = setupDriver();
    if (lower.is_err()) {
        return lower.propagate<DeviceError::TransportFailed>(
            "device transport setup failed");
    }
    return Ok();
}
```

结果中的链为 `DeviceError`（顶层）→ `DriverError`（cause）。如果底层已经有
cause，它们会被展平成单链，并按距离顶层由近到远保存。

The result contains `DeviceError` as its top-level code and `DriverError` as the
first cause. Existing lower causes are flattened into one nearest-first chain.

```cpp
for (size_t i = 0; i < error.cause_count(); ++i) {
    const ErrorFrame frame = error.cause(i);
    log("%s::%s code=%ld: %s",
        frame.domain, frame.name, long(frame.numericCode), frame.message);
}

if (error.truncated())
    log("older causes were truncated");
```

== 错误名称 / Error names

GCC 和 Clang 会在编译期自动提取 enum 类型名与枚举项名称，不需要为每个类
编写 `ErrorTraits` 或字符串 `switch`。人工消息与枚举名称会分别保留。

GCC and Clang extract enum type and enumerator names at compile time. Per-class
`ErrorTraits` and string switches are unnecessary. Manual context and enum names
are retained separately.

```cpp
error.domain();       // "SensorError"
error.name();         // "Timeout"
error.numeric_code(); // underlying numeric value
```

只有 enum 数值别名或需要面向用户的自定义文案时，才使用可选的
`ErrorNameOverride<Code>`。普通业务错误不需要注册。

旧驱动仍返回自定义 `Error` 结构时，用 `error_cause()` 建立适配 frame，无需立即
重写旧驱动 API。

Use `error_cause()` to adapt a legacy driver's custom error structure without
rewriting that driver's public API immediately.

```cpp
return Err<DeviceError::TransportFailed>(
    "RMT setup failed",
    error_cause("RMT", rmtError.native, RMT::detailName(rmtError.detail)));
```

== 深度策略 / Trace depth policy

因果链使用对象内固定数组，不分配堆内存。默认容量为 4，最大容量为 8。

Cause chains use fixed inline storage and never allocate. The default capacity is
4 and the maximum is 8.

```cpp
using SetupErrors = ErrorSet<
    SetupError::AlreadyStarted,
    SetupError::DriverFailed>;

using TinyErrors = TracedErrorSet<2,
    SetupError::AlreadyStarted,
    SetupError::DriverFailed>;
```

同一返回函数中的不同错误也可以使用不同的 trace 深度。裸错误继续使用集合默认
深度；`TraceErrorSet<Depth, Code>` 只覆盖一个错误：

Different errors in one function may use different trace depths. A raw entry uses
the set default, while `TraceErrorSet<Depth, Code>` overrides one error only:

```cpp
using UpdateErrors = ErrorSet<
    UpdateError::NotStarted, // default: 4
    TraceErrorSet<
        error_trace_depth::AMPUTATION,
        UpdateError::ExpectedFailure>,
    TraceErrorSet<
        error_trace_depth::CHASE_IT_DOWN,
        UpdateError::DriverFailure>>;

using UpdateResult = Result<void, UpdateErrors>;
```

返回点仍然只写裸错误码。trace 策略属于 API 声明，不允许由单个 `Err<>` 临时覆盖：

```cpp
return Err<UpdateError::ExpectedFailure>("not ready");

return lower.propagate<UpdateError::DriverFailure>(
    "driver update failed");
```

`UpdateErrors::trace_depth` 是集合内最大深度，也决定对象的内联存储大小。
`trace_depth_for<Code>` 返回某个错误的声明深度，`active_trace_depth()` 返回当前
错误的深度。逐错误策略控制保留行为，但不会让同一 Result 的不同状态具有不同
对象尺寸。

When converting to a shallower policy, excess causes are discarded and
`truncated()` becomes true. Converting that value to a deeper policy later cannot
recover discarded frames.

- `AMPUTATION = 0`：不保存 cause，但保留顶层 code 和人工消息。
- `RUN_FOR_YOUR_LIFE = 4`：默认模式。
- `CHASE_IT_DOWN = 8`：最大诊断模式。
- 任意 `0..8` 的整数深度也合法；容量不足时 `truncated()` 为 true。

== 集合扩大与组合器 / Widening and combinators

错误的身份由“类型 + 数值”组成。不同 enum 即使底层数字相同也能放在同一个
集合中，并由 `is()` 和 `match()` 准确区分。同一 enum、同一数值的两个拼写是
C++ 枚举别名，会被视为同一个错误。

错误真子集可以扩大为包含它的集合，code、message 和 trace 都会保留。
`map`、`map_err`、`and_then`、`or_else` 和 `match` 继续可用；`map_err`
必须产生另一个错误集合。

Error identity consists of its type and value. Different enum types remain distinct
even when their numeric values match. Same-type, same-value spellings are C++ enum
aliases and represent one error. Subset widening preserves code, message, and trace.

```cpp
using ReadErrors = ErrorSet<SensorError::Timeout>;
using AllErrors = ErrorSet<SensorError::Timeout, SensorError::Disconnected>;

AllErrors widened = ReadErrors::of<SensorError::Timeout>();

int category = widened.match(
    on<SensorError::Timeout>([] { return 1; }),
    on<SensorError::Disconnected>([] { return 2; }));
```

`ErrorSet::match` 必须把集合中的每个错误恰好处理一次，并且所有分支返回完全
相同的类型。这样新增错误码时，遗漏的调用点会由编译器指出。

`ErrorSet::match` handles every allowed code exactly once and requires identical
return types. Adding a code therefore exposes incomplete handling at compile time.

== Result 状态与访问 / State and access

先用 `is_ok()`、`is_err()` 或显式 bool 判断状态。`value()`/`unwrap()` 只能在成功
状态调用，`error()`/`unwrap_err()`/`propagate()` 只能在失败状态调用；固件版本不
通过异常修复错误状态访问。

Check the state with `is_ok()`, `is_err()`, or explicit bool first. Accessors do
not throw: calling the value side on an error, or the error side on success, is a
programming error.

```cpp
const ReadResult result = read();
if (result.is_err()) {
    logError(result.error());
    return;
}
use(result.value());

const int value = result.value_or(-1);
```

== Result 组合器速查 / Combinator guide

- `map(f)`：仅转换成功值；失败原样透传。`f` 返回 void 时得到 `Result<void,E>`。
- `map_err(f)`：仅转换错误；`f` 必须返回另一个 `ErrorSet`。
- `map_or(default, f)`：成功时转换为普通值，失败时得到默认值。
- `map_err_or(default, f)`：失败时转换为普通值，成功时得到默认值。
- `and_then(f)`：成功后继续一个返回 Result 的操作；用于串联可能失败的步骤。
- `or_else(f)`：失败时执行恢复操作；成功值不变。
- `match(ok, err)`：把两种状态折叠成同一种返回类型，两支返回类型必须完全相同。

```cpp
auto doubled = read().map([](int value) { return value * 2; });

auto encoded = read().and_then([](int value) -> EncodeResult {
    return encode(value);
});

auto recovered = read().or_else([](const ReadErrors&) -> ReadResult {
    return Ok(0);
});

const char* state = read().match(
    [](int) { return "ok"; },
    [](const ReadErrors&) { return "failed"; });
```

`Result<void,E>` 提供同一套组合器，但成功 lambda 不接收参数：

```cpp
using SetupResult = Result<void, SetupErrors>;

auto ready = setupBus()
    .and_then([] { return setupDevice(); })
    .map([] { return DeviceState::Ready; });
```

`map_err` 不是添加 cause 的首选方法。跨层包装请用 `propagate<UpperCode>()`，它会
自动保存底层错误；同层错误子集扩大则直接 `return lower.propagate()`。

`map_err` is not the preferred cause-wrapping API. Use `propagate<UpperCode>()`
across layers so the lower error is retained automatically. Use parameterless
`propagate()` when only widening a same-layer error subset.

```cpp
if (driverResult.is_err()) {
    return driverResult.propagate<SetupError::DriverFailed>(
        "device setup failed");
}
```

== ESP 原生错误 / Native ESP errors

`StdError` 保持与 `esp_err_t` 的数值兼容。返回 `Result<void, E>` 时，可以直接
包装 ESP 调用：

```cpp
return NativeErr<DeviceError::DriverFailed>(
    esp_driver_call(),
    "ESP driver call failed");
```

`ESP_OK` 自动转换为 `Ok()`。目标集合包含对应 `StdError` 时保留该类型化错误；
否则使用指定 fallback，并把真实 ESP 数值与名称加入 cause chain。未知原生码
不会丢失。

`StdError::INVALID_ARGS` 与 `StdError::INVALID_ARGUMENT` 是同值别名，统一显示为
规范名称 `INVALID_ARGUMENT`。
