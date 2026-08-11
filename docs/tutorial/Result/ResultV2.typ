#let ink = rgb("#172033")
#let muted = rgb("#5E6879")
#let navy = rgb("#173B57")
#let cyan = rgb("#17A2B8")
#let cyan-pale = rgb("#E9F7FA")
#let amber = rgb("#D28A16")
#let amber-pale = rgb("#FFF6DF")
#let red = rgb("#BD4654")
#let red-pale = rgb("#FFF0F2")
#let green = rgb("#27866D")
#let green-pale = rgb("#EAF7F2")
#let paper = rgb("#FBFCFE")
#let line = rgb("#DCE3EA")

#set document(
  title: "PRTN ResultV2：面向 MCU 的类型安全错误处理",
  author: "PRTN Project",
)
#set page(
  paper: "a4",
  margin: (x: 18mm, top: 17mm, bottom: 18mm),
  fill: paper,
  header: align(right, text(size: 8pt, fill: muted)[PRTN / ResultV2 / C++20]),
  footer: context align(center, text(size: 8pt, fill: muted)[— #counter(page).display("1") —]),
)
#set text(font: "Noto Sans CJK SC", lang: "zh", region: "CN", size: 10pt, fill: ink)
#set par(justify: true, leading: 0.72em)
#set heading(numbering: "1.1", outlined: true)
#set list(indent: 1.2em, body-indent: .55em, spacing: .35em)
#set enum(indent: 1.2em, body-indent: .55em, spacing: .35em)
#set table(stroke: line, inset: 5pt)
#show heading.where(level: 1): set text(font: "Noto Serif CJK SC", size: 20pt, weight: "bold", fill: navy)
#show heading.where(level: 2): set text(font: "Noto Serif CJK SC", size: 14pt, weight: "bold", fill: navy)
#show heading.where(level: 3): set text(size: 11pt, weight: "bold", fill: green)
#show raw: set text(font: "JetBrains Maple Mono", size: 7.2pt)
#show raw.where(block: true): it => block(
  width: 100%,
  fill: rgb("#F2F5F8"),
  stroke: (left: 2pt + cyan),
  inset: 9pt,
  radius: 3pt,
  breakable: true,
  it,
)
#show link: set text(fill: cyan)

#let callout(title, body, tone: cyan, pale: cyan-pale) = block(
  width: 100%,
  fill: pale,
  stroke: .7pt + tone,
  radius: 4pt,
  inset: 9pt,
  breakable: true,
)[
  #text(weight: "bold", fill: tone)[#title]
  #v(3pt)
  #body
]
#let idea(body) = callout("核心思路 / Core idea", body, tone: cyan, pale: cyan-pale)
#let warning(body) = callout("注意 / Watch out", body, tone: red, pale: red-pale)
#let practice(body) = callout("动手验证 / Try it", body, tone: amber, pale: amber-pale)
#let design(body) = callout("设计取舍 / Design choice", body, tone: green, pale: green-pale)
#let en(body) = block(
  width: 100%,
  inset: (left: 8pt),
  stroke: (left: .8pt + line),
  text(size: 9pt, fill: muted, lang: "en", body),
)

// Cover
#page(margin: 0pt, header: none, footer: none, fill: navy)[
  #place(top + left, dx: 19mm, dy: 20mm)[
    #block(width: 34mm, height: 4pt, fill: cyan, radius: 2pt)
  ]
  #place(top + center, dy: 48mm)[
    #align(center)[
      #text(font: "Noto Serif CJK SC", size: 34pt, weight: "bold", fill: white)[ResultV2]
      #v(8pt)
      #text(size: 18pt, weight: "medium", fill: rgb("#CDEEF3"))[面向 MCU 的类型安全错误处理]
      #v(5pt)
      #text(size: 12pt, fill: rgb("#AFC5D1"))[Type-safe error handling for MCUs]
      #v(22pt)
      #block(width: 132mm, stroke: .7pt + rgb("#7FA4B9"), radius: 5pt, inset: 12pt)[
        #text(size: 10.5pt, fill: white)[
          用 C++20、封闭错误集合和穷尽匹配，把固件失败路径变成编译器能检查的接口。

          Build firmware failure paths the compiler can check, using C++20,
          closed error sets, and exhaustive matching.
        ]
      ]
      #v(28pt)
      #text(size: 10pt, fill: rgb("#AFC5D1"))[PRTN custom Result · C++20 · allocation-free]
    ]
  ]
  #place(bottom + left, dx: 19mm, dy: -18mm)[
    #text(size: 9pt, fill: rgb("#AFC5D1"))[PRTN Project · 2026]
  ]
]

#pagebreak()

= 阅读地图 / Reading map

这份教程针对 `src/fw/inc/Result.h` 当前实现。它不是 `std::expected` 的完整替代品，而是一套刻意受限、适合 PRTN 固件的值类型：不分配堆内存，支持 `constexpr`，并要求成功值和错误值都可平凡复制。

#en[
  This tutorial targets the current implementation in `src/fw/inc/Result.h`.
  It is not a complete replacement for `std::expected`; it is a deliberately
  constrained value type for PRTN firmware: allocation-free, `constexpr`-friendly,
  and restricted to trivially copyable values and errors.
]

#idea[
  `Result<T, E>` 表示“要么得到 `T`，要么得到 `E`”。ResultV2 再用 `ErrorSet<...>` 把“可能出现哪些错误”写进类型。

  `Result<T, E>` means “either a `T` or an `E`.” ResultV2 additionally uses
  `ErrorSet<...>` to encode the exact set of possible errors in the type.
]

#outline(title: [目录 / Contents], depth: 3, indent: 1.2em)

#pagebreak()

= 先从兼容模式开始 / Start with compatibility mode

== 最小的 `Result<T, E>` / The smallest `Result<T, E>`

如果已有一个普通错误枚举，可以直接把它当作 `E`。成功通过 `Ok(value)` 构造，失败通过 `Err(error)` 构造。

#en[
  An existing error enum can be used directly as `E`. Construct success with
  `Ok(value)` and failure with `Err(error)`.
]

```cpp
#include "src/fw/inc/Result.h"

enum class ReadError : unsigned char {
    disconnected,
    timeout,
};

Result<int, ReadError> read_sensor(bool connected) {
    if (!connected) {
        return Err(ReadError::disconnected);
    }
    return Ok(42);
}
```

返回表达式不必写出完整 `Result` 构造。`Ok(42)` 先产生轻量的 `OkValue<int>`，返回类型再把它转换为 `Result<int, ReadError>`；`Err(...)` 同理。

#en[
  The return expression does not need to spell out the full `Result` constructor.
  `Ok(42)` first creates a lightweight `OkValue<int>`, which the return type converts
  to `Result<int, ReadError>`; `Err(...)` works the same way.
]

== 检查和取值 / Inspecting and extracting

```cpp
auto result = read_sensor(true);

if (result.is_ok()) {
    int value = result.value();
}

if (result) {                 // explicit operator bool()
    int copied = result.unwrap();
}

int always_a_value = result.value_or(-1);
```

#table(
  columns: (1.2fr, 2fr, 2.1fr),
  table.header([*操作 / Operation*], [*成功时 / On success*], [*失败时 / On error*]),
  [`is_ok()` / `is_err()`], [返回状态 / reports state], [返回状态 / reports state],
  [`value()`], [返回 `const T&`], [不可读取 / must not be read],
  [`error()`], [不可读取 / must not be read], [返回 `const E&`],
  [`unwrap()`], [复制并返回 `T`], [不可调用 / must not be called],
  [`unwrap_err()`], [不可调用 / must not be called], [复制并返回 `E`],
  [`value_or(fallback)`], [返回成功值], [返回 fallback],
)

#warning[
  这个实现为减小 MCU 成本，`value()`、`error()`、`unwrap()` 和 `unwrap_err()` 不做运行时状态检查。调用前必须确认分支；错误分支读 `value_` 属于未定义行为。

  To keep MCU overhead low, these accessors do not perform runtime state checks.
  Confirm the active branch first; reading `value_` from an error result is undefined behavior.
]

== 一次处理两个分支 / Handle both branches once

`match` 把成功和失败路径放在同一个表达式里。两条 lambda 必须返回完全相同的类型。

#en[
  `match` keeps success and failure handling in one expression. Both lambdas must
  return exactly the same type.
]

```cpp
int display_value(Result<int, ReadError> result) {
    return result.match(
        [](int value) {
            return value;
        },
        [](ReadError) {
            return -1;
        });
}
```

= ResultV2：封闭错误集合 / Closed error sets

== 把错误契约放进类型 / Put the error contract in the type

普通枚举允许它的每一个枚举值出现在每一个函数中。`ErrorSet` 更精确：一个函数只声明自己真正可能返回的错误码。

#en[
  A plain enum permits every enumerator in every function. `ErrorSet` is more precise:
  each function declares only the codes it can actually return.
]

```cpp
enum class DeviceError : unsigned char {
    disconnected,
    invalid_state,
    no_memory,
};

using ReadErrors = ErrorSet<
    DeviceError::disconnected,
    DeviceError::invalid_state>;

using ServiceErrors = ErrorSet<
    DeviceError::disconnected,
    DeviceError::invalid_state,
    DeviceError::no_memory>;

using ReadResult = Result<int, ReadErrors>;
```

`ErrorSet` 至少包含一个码，不能重复，所有码必须能归一到公共底层类型。对象只保存一个原生错误码，因此 `sizeof(ReadErrors)` 等于该公共类型的大小。

#en[
  An `ErrorSet` must contain at least one unique code, and all codes must share a
  common type. The object stores only one native code, so its size is the size of
  that common type.
]

== 构造和查询 / Construction and queries

```cpp
ReadResult read_value(bool fail) {
    if (fail) {
        return Err<DeviceError::disconnected>();
    }
    return Ok(42);
}

constexpr auto error =
    ReadErrors::of<DeviceError::invalid_state>();

static_assert(error.is<DeviceError::invalid_state>());
static_assert(ReadErrors::contains(DeviceError::disconnected));
static_assert(!ReadErrors::contains(DeviceError::no_memory));
```

`Err<Code>()` 生成一个 `ErrorConstant<Code>`，只有目标 `ErrorSet` 确实包含该码时才能构造。把 `no_memory` 返回为 `ReadResult` 会在编译期失败。

#en[
  `Err<Code>()` creates an `ErrorConstant<Code>`. It can construct the target
  `ErrorSet` only when that code is allowed. Returning `no_memory` as a `ReadResult`
  therefore fails at compile time.
]

== 穷尽匹配错误 / Exhaustive error matching

```cpp
int classify(ReadErrors error) {
    return error.match(
        on<DeviceError::disconnected>([] {
            return 1;
        }),
        on<DeviceError::invalid_state>([] {
            return 2;
        }));
}
```

`ErrorSet::match` 的每个分支都是零参数函数。编译器验证三件事：每个允许的错误恰好出现一次；没有集合外的错误；所有分支返回同一类型。

#en[
  Each arm of `ErrorSet::match` is a nullary function. The compiler verifies that
  every allowed error appears exactly once, no outside error is present, and every
  arm returns the same type.
]

#design[
  `Result::match` 负责“成功还是失败”；进入失败分支后，`ErrorSet::match` 再负责“具体是哪种失败”。这两层匹配分别对应两个不同问题。

  `Result::match` answers “success or failure.” Inside the error branch,
  `ErrorSet::match` answers “which exact failure.” The two layers solve different problems.
]

```cpp
int consume(ReadResult result) {
    return result.match(
        [](int value) {
            return value;
        },
        [](ReadErrors error) {
            return -classify(error);
        });
}
```

== 安全扩大错误集合 / Safely widen an error set

较小集合可以隐式构造较大集合，反方向不成立。这让底层函数的失败沿调用链向上传递，同时防止上层错误被静默丢弃。

#en[
  A smaller set can implicitly construct a larger set, but not the reverse. This
  lets lower-layer failures travel upward without silently discarding upper-layer errors.
]

```cpp
ServiceErrors widen(ReadErrors error) {
    return error;
}

static_assert(std::is_convertible_v<ReadErrors, ServiceErrors>);
static_assert(!std::is_convertible_v<ServiceErrors, ReadErrors>);
```

= 转换结果 / Transforming results

== `map`：只转换成功值 / Transform only success

`map` 在成功时调用函数，在失败时原样传播错误。函数返回 `void` 时，结果会变成 `Result<void, E>`。

#en[
  `map` invokes the function only on success and propagates errors unchanged. If the
  function returns `void`, the output becomes `Result<void, E>`.
]

```cpp
auto plus_one = read_value(false).map([](int value) {
    return value + 1;
});

auto consumed = read_value(false).map([](int value) {
    send_to_display(value);
});
// consumed: Result<void, ReadErrors>
```

== `map_err`：只转换错误 / Transform only the error

```cpp
auto widened = read_value(true).map_err([](ReadErrors error) {
    return ServiceErrors {error};
});

auto status = read_value(true).map_err([](ReadErrors error) {
    return classify(error); // Result<int, int>
});
```

`map_err` 的函数不能返回 `void`，因为失败分支必须仍然携带某个错误值。

#en[
  The `map_err` function cannot return `void`, because the failure branch must still
  carry an error value.
]

== `map_or` 和 `map_err_or` / Collapse to a plain value

这两个操作不再返回 `Result`，而是把某一分支映射为普通值，另一分支使用默认值。

#en[
  These operations stop returning a `Result`: one branch is mapped to a plain value,
  while the other uses a default.
]

```cpp
int doubled = read_value(false).map_or(-1, [](int value) {
    return value * 2;
});

int error_class = read_value(true).map_err_or(
    0,
    [](ReadErrors error) {
        return classify(error);
    });
```

#table(
  columns: (1.2fr, 1.8fr, 1.8fr),
  table.header([*操作*], [*成功分支*], [*错误分支*]),
  [`map(f)`], [`Ok(f(value))`], [传播原错误],
  [`map_err(f)`], [传播原值], [`Err(f(error))`],
  [`map_or(d, f)`], [`f(value)`], [`d`],
  [`map_err_or(d, f)`], [`d`], [`f(error)`],
)

= 串联和恢复 / Chaining and recovery

== `and_then`：串联可能失败的步骤 / Chain fallible steps

当第二步也返回 `Result` 时使用 `and_then`。第一步失败就短路；第一步成功才调用第二步。

#en[
  Use `and_then` when the next step also returns a `Result`. An initial error
  short-circuits the chain; the second step runs only after success.
]

```cpp
using ServiceResult = Result<int, ServiceErrors>;

ServiceResult double_value(int value) {
    return Ok(value * 2);
}

auto result = read_value(false).and_then(double_value);
```

目标结果必须能接收源错误类型。这里 `ServiceErrors` 是 `ReadErrors` 的超集，所以第一步的错误能够安全传播。

#en[
  The target result must accept the source error type. Here `ServiceErrors` is a
  superset of `ReadErrors`, so an error from the first step can propagate safely.
]

== `or_else`：从错误恢复 / Recover from an error

`or_else` 与 `and_then` 对称：成功值原样通过，只有错误分支会调用恢复函数。恢复结果必须能接收原成功值类型。

#en[
  `or_else` is the mirror of `and_then`: success passes through untouched, and only
  an error invokes the recovery function. The recovery result must accept the original value type.
]

```cpp
ReadResult recover(ReadErrors) {
    return Ok(7);
}

auto recovered = read_value(true).or_else(recover);
// recovered.unwrap() == 7
```

= 无返回值操作 / Operations without a value

`Result<void, E>` 表示操作只有成功/失败，没有成功载荷。使用 `Ok()`，成功回调不接收参数；其余组合操作保持相同心智模型。

#en[
  `Result<void, E>` represents an operation with success or failure but no success
  payload. Use `Ok()`; success callbacks take no argument, while the other combinators
  keep the same mental model.
]

```cpp
using InitResult = Result<void, ReadErrors>;

InitResult initialize(bool fail) {
    if (fail) {
        return Err<DeviceError::invalid_state>();
    }
    return Ok();
}

auto version = initialize(false).map([] {
    return 2;
}); // Result<int, ReadErrors>

int status = initialize(true).match(
    [] {
        return 0;
    },
    [](ReadErrors error) {
        return classify(error);
    });
```

= 接入原生错误码 / Adapting native error codes

ESP-IDF、Arduino 库和 C API 常返回“零表示成功”的整数码。`from_native` 把它转换为 `Result<void, ErrorSet>`：已知错误保持原值，未知错误先上报，再收敛为指定 fallback。

#en[
  ESP-IDF, Arduino libraries, and C APIs often return integer codes where zero means
  success. `from_native` converts one into `Result<void, ErrorSet>`: known errors are
  preserved; unknown errors are reported and then narrowed to a chosen fallback.
]

```cpp
constexpr int ERR_TIMEOUT = 0x101;
constexpr int ERR_IO      = 0x102;

using NativeErrors = ErrorSet<ERR_TIMEOUT, ERR_IO>;

int unexpected_code = 0;
auto result = from_native<NativeErrors, ERR_IO>(
    native_call(),
    [&](int code) {
        unexpected_code = code;
        log_unexpected(code);
    });
```

#table(
  columns: (1.4fr, 1.8fr, 2fr),
  table.header([*原生返回值*], [*Result*], [*回调*]),
  [默认成功码 `0`], [`Ok()`], [不调用],
  [集合中的已知错误], [`Err(known)`], [不调用],
  [集合外的未知错误], [`Err(fallback)`], [调用一次并传入原码],
)

如果原生 API 使用非零成功码，可设置第三个模板参数：

#en[If the native API uses a non-zero success code, set the third template argument:]

```cpp
auto result = from_native<NativeErrors, ERR_IO, 1>(code, on_unexpected);
```

= 完整可编译示例 / Complete compilable example

下面的程序只依赖 C++20、标准库和项目的 `Result.h`，覆盖封闭错误、穷尽匹配、错误集扩展、串联、恢复、void result 与原生错误码适配。

#en[
  The following program depends only on C++20, the standard library, and the project's
  `Result.h`. It covers closed errors, exhaustive matching, widening, chaining,
  recovery, void results, and native-code adaptation.
]

```cpp
// BEGIN COMPLETE_EXAMPLE
#include <cassert>
#include <type_traits>

#include "src/fw/inc/Result.h"

enum class DeviceError : unsigned char {
    disconnected,
    invalid_state,
    no_memory,
};

using ReadErrors = ErrorSet<
    DeviceError::disconnected,
    DeviceError::invalid_state>;

using ServiceErrors = ErrorSet<
    DeviceError::disconnected,
    DeviceError::invalid_state,
    DeviceError::no_memory>;

using ReadResult = Result<int, ReadErrors>;
using ServiceResult = Result<int, ServiceErrors>;
using InitResult = Result<void, ReadErrors>;

constexpr ReadResult read_value(bool fail) {
    if (fail) {
        return Err<DeviceError::disconnected>();
    }
    return Ok(21);
}

constexpr int classify(ReadErrors error) {
    return error.match(
        on<DeviceError::disconnected>([] { return 1; }),
        on<DeviceError::invalid_state>([] { return 2; }));
}

constexpr ServiceResult double_value(int value) {
    return Ok(value * 2);
}

constexpr ServiceResult allocate_value(int) {
    return Err<DeviceError::no_memory>();
}

constexpr ReadResult recover(ReadErrors) {
    return Ok(7);
}

constexpr InitResult initialize(bool fail) {
    if (fail) {
        return Err<DeviceError::invalid_state>();
    }
    return Ok();
}

static_assert(std::is_trivially_copyable_v<ReadResult>);
static_assert(std::is_convertible_v<ReadErrors, ServiceErrors>);
static_assert(!std::is_convertible_v<ServiceErrors, ReadErrors>);

static_assert(read_value(false).unwrap() == 21);
static_assert(read_value(true).unwrap_err()
                  .is<DeviceError::disconnected>());
static_assert(read_value(false)
                  .map([](int value) { return value + 1; })
                  .unwrap() == 22);
static_assert(read_value(false).and_then(double_value).unwrap() == 42);
static_assert(read_value(false).and_then(allocate_value).unwrap_err()
                  .is<DeviceError::no_memory>());
static_assert(read_value(true).or_else(recover).unwrap() == 7);
static_assert(initialize(false).map([] { return 2; }).unwrap() == 2);

int main() {
    auto ok = read_value(false);
    auto failed = read_value(true);

    assert(ok.is_ok());
    assert(ok.value_or(-1) == 21);
    assert(failed.is_err());
    assert(classify(failed.unwrap_err()) == 1);

    int unexpected = 0;
    using NativeErrors = ErrorSet<0x101, 0x102>;
    auto native = from_native<NativeErrors, 0x102>(
        0x999,
        [&](int code) { unexpected = code; });

    assert(native.is_err());
    assert(native.unwrap_err().is<0x102>());
    assert(unexpected == 0x999);
}
// END COMPLETE_EXAMPLE
```

#practice[
  在仓库根目录，把代码保存为临时 `.cpp` 后运行：

  From the repository root, save the listing as a temporary `.cpp` and run:

  ```sh
  c++ -std=c++20 -Wall -Wextra -Wpedantic -I. example.cpp -o example
  ./example
  ```
]

= 约束和选择指南 / Constraints and choosing an operation

== 类型约束 / Type constraints

`Result<T, E>` 在编译期要求 `T` 和 `E` 都是 trivially copyable；`Result<void, E>` 只要求 `E`。这允许 union 不引入手工析构和复杂生命周期管理。

#en[
  `Result<T, E>` requires both `T` and `E` to be trivially copyable at compile time;
  `Result<void, E>` requires only `E`. This keeps the union free from manual destruction
  and complex lifetime management.
]

```cpp
static_assert(std::is_trivially_copyable_v<Result<int, ReadErrors>>);

// std::string is not supported as T:
// Result<std::string, ReadErrors> invalid;
```

指针、整数、枚举和只含这些成员的简单结构通常合适。拥有堆内存、虚函数或非平凡析构的类型通常不合适。

#en[
  Pointers, integers, enums, and simple structs composed of them are usually suitable.
  Types that own heap memory, use virtual functions, or require non-trivial destruction
  are usually unsuitable.
]

== 快速选择 / Quick chooser

#table(
  columns: (2.4fr, 1.3fr, 1.8fr),
  table.header([*意图 / Intent*], [*使用 / Use*], [*结果 / Output*]),
  [检查成功或失败], [`is_ok`, `is_err`], [`bool`],
  [提供失败默认值], [`value_or`], [`T`],
  [同时处理成功与失败], [`match`], [共同返回类型],
  [改变成功值], [`map`], [`Result<U, E>`],
  [改变错误类型], [`map_err`], [`Result<T, G>`],
  [成功时继续可能失败的步骤], [`and_then`], [下一个 `Result`],
  [失败时尝试恢复], [`or_else`], [恢复 `Result`],
  [匹配具体封闭错误码], [`ErrorSet::match`], [共同返回类型],
  [包装零成功原生码], [`from_native`], [`Result<void, E>`],
)

= 常见编译错误 / Common compile-time failures

== 漏掉错误分支 / Missing an error arm

```cpp
// ErrorSet::match must handle every error exactly once
error.match(
    on<DeviceError::disconnected>([] { return 1; }));
```

补上 `invalid_state`，并确保没有重复分支。

#en[Add the `invalid_state` arm and make sure no arm is duplicated.]

== `and_then` 无法传播源错误 / Target cannot accept the source error

```cpp
// Target Result must accept ReadErrors.
auto result = read_value(false).and_then([](int value) {
    return Result<int, ErrorSet<DeviceError::no_memory>> {Ok(value)};
});
```

让目标错误集合包含源集合的所有错误，通常使用更大的服务层 `ErrorSet`。

#en[
  Make the target error set include every source error, usually by using a wider
  service-layer `ErrorSet`.
]

== `match` 返回类型不一致 / Mismatched match return types

```cpp
// int versus long: not exactly the same type
result.match(
    [](int value) { return value; },
    [](ReadErrors) { return -1L; });
```

显式统一类型，例如两边都返回 `int`。实现要求 `same_as`，仅仅“可相互转换”还不够。

#en[
  Make both return types explicitly identical, for example `int`. The implementation
  requires `same_as`; mere convertibility is not enough.
]

= 总结 / Summary

- `Result<T, E>` 让失败成为返回类型的一部分，而不是隐藏的全局状态。
- `ErrorSet<...>` 进一步把每个函数允许的具体错误编码进类型。
- `match` 提供显式分支；`map`、`and_then` 和 `or_else` 让调用链保持线性。
- `Result<void, E>` 表达没有成功载荷的操作，`from_native` 连接传统 C 风格 API。
- 这些安全性依赖正确检查活跃分支；`unwrap` 本身不会防止误用。

#en[
  - `Result<T, E>` makes failure part of the return type instead of hidden global state.
  - `ErrorSet<...>` encodes each function's exact allowed errors in that type.
  - `match` makes branches explicit; `map`, `and_then`, and `or_else` keep pipelines linear.
  - `Result<void, E>` models payload-free success, and `from_native` bridges C-style APIs.
  - This safety still depends on checking the active branch; `unwrap` does not guard misuse.
]

#align(center)[
  #block(width: 70%, fill: green-pale, stroke: .7pt + green, radius: 5pt, inset: 12pt)[
    #align(center)[
      #text(weight: "bold", fill: green)[让错误契约由编译器维护。]
      #v(3pt)
      #text(size: 9pt, fill: muted)[Let the compiler maintain the error contract.]
    ]
  ]
]
