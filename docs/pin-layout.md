# Pin layout / 引脚布局

`PinLayout` lets each final application declare its physical GPIO usage once. It catches common
wiring mistakes during compilation while still returning ESP-IDF's native `gpio_num_t`.

`PinLayout` 让每个最终应用只声明一次物理 GPIO 占用。它会在编译期发现常见接线错误，同时
`Pins[...]` 返回的仍然是 ESP-IDF 原生 `gpio_num_t`，不需要包装类型或手动转换。

## Quick start / 快速调用

Put the layout near the top of the final application's `Detail` namespace:

在最终应用的 `Detail` 命名空间顶部声明布局：

```cpp
#include "src/cfg/AppConfig.h" // Includes BoardConfig and PinLayout.

namespace MyApp::Detail
{
    enum class PinId : uint8_t
    {
        StatusLed,
        Button,
        I2cSda,
        I2cScl,
    };

    inline constexpr auto Pins = ::prtn::pin::layout(
        ::prtn::pin::bind(PinId::StatusLed, GPIO_NUM_12, ::prtn::pin::Role::Output),
        ::prtn::pin::bind(PinId::Button, GPIO_NUM_9, ::prtn::pin::Role::InputPullup),
        ::prtn::pin::bind(PinId::I2cSda, GPIO_NUM_4, ::prtn::pin::Role::I2cSda),
        ::prtn::pin::bind(PinId::I2cScl, GPIO_NUM_5, ::prtn::pin::Role::I2cScl));

    struct Context
    {
        LED    led {Pins[PinId::StatusLed]};
        Button button {Pins[PinId::Button], LOW, INPUT_PULLUP, 30};
        IIC    i2c {Pins[PinId::I2cSda], Pins[PinId::I2cScl]};
    };
}
```

This is the whole calling pattern:

调用时只需要记住三个东西：

1. `PinId` names the resource / `PinId` 给资源起名。
2. `bind(id, GPIO_NUM_x, Role)` declares its wiring and use / `bind` 声明接线与用途。
3. `Pins[id]` supplies a native `gpio_num_t` to a driver / `Pins[id]` 直接传给驱动。

Do not declare a second claim beside the GPIO constant. The binding itself is the declaration.

不需要先写 `DefaultFooPin`，再调用另一个“注册函数”。`bind(...)` 本身就是唯一声明。

## Where layouts belong / 布局应该放在哪里

Create one layout in the final application that owns the actual hardware assembly. Drivers and
intermediate services accept `gpio_num_t`; they do not claim pins themselves.

每个最终应用维护一张布局表。驱动和中间 service 只接收 `gpio_num_t`，不要在驱动内部再次
注册引脚。这样组合不同设备时，所有物理占用都在同一处可见，也不需要每层重复
`static_assert`。

For a bus, bind the physical bus wires once, then pass them to the bus object. Devices on that bus
own addresses or chip-select lines, not another copy of SDA/SCL or SCK/MOSI.

对于总线，SDA/SCL 或 SCK/MOSI 只在布局中出现一次，然后传给 I²C/SPI 总线对象。挂在总线
上的设备拥有地址或各自的 CS，不应重复声明同一组总线引脚。

## Roles / 用途

| Role | Required electrical capability / 所需电气能力 |
| --- | --- |
| `Input`, `SpiMiso`, `UartRx`, `I2sDataIn` | Digital input / 数字输入 |
| `Output`, `PwmOutput`, `RmtTx` | Digital output / 数字输出 |
| `SpiSck`, `SpiMosi`, `SpiCs`, `UartTx` | Digital output / 数字输出 |
| `I2sBclk`, `I2sWs` | Digital output / 数字输出 |
| `InputPullup` | Digital input plus internal pull-up / 数字输入与内部上拉 |
| `InputPulldown` | Digital input plus internal pull-down / 数字输入与内部下拉 |
| `I2cSda`, `I2cScl` | Digital input and output / 数字输入与输出 |
| `AnalogInput` | ADC-capable input / 模拟输入能力 |
| `UsbDm`, `UsbDp` | Dedicated USB D-/D+ pin / 专用 USB 引脚 |

SPI, I²C, UART, PWM, RMT and I²S roles describe how the app uses a GPIO. They do not require that
the GPIO be a board's default peripheral route; ESP32 GPIO-matrix routing remains usable.

SPI、I²C、UART、PWM、RMT 和 I²S 的 `Role` 表示应用用途，不表示“只能使用板卡默认复用脚”。
只要电气方向允许，ESP32 的 GPIO Matrix 路由仍然可以使用。

## Compile-time failures / 编译期报错

`layout(...)` rejects:

`layout(...)` 会拒绝以下情况：

- duplicate `PinId` / 同一个 `PinId` 声明两次；
- one physical GPIO assigned twice / 同一个物理 GPIO 分配两次；
- GPIO absent from the selected board / 当前板卡不存在该 GPIO；
- GPIO listed in the board's `ReservedPins` / 使用板卡保留引脚；
- a role incompatible with the pin capability / 用途与引脚电气能力不符。

Accessing an ID that is not in the layout also fails during compilation:

访问表中不存在的 ID 同样会在编译期失败：

```cpp
enum class PinId : uint8_t { Led, Button };

inline constexpr auto Pins = ::prtn::pin::layout(
    ::prtn::pin::bind(PinId::Led, GPIO_NUM_12, ::prtn::pin::Role::Output));

Button button {Pins[PinId::Button]}; // Compile error / 编译错误
```

The diagnostic text identifies the category. For lower-level tests, call `validateLayout(...)` on
a raw `std::array` of bindings to inspect the involved binding indices and GPIO.

诊断文本会说明冲突类别；底层测试可以把原始 binding 数组传给 `validateLayout(...)`，检查
返回的 `first`、`second` 和 `gpio`。

## Defaults and reservations / 默认值与保留引脚

A driver default such as `IIC::DefaultSdaPin` or a board alias such as `prtn::b::i2c0::sda` is only
a native `gpio_num_t` value. It is a convenience default, not an ownership claim.

`IIC::DefaultSdaPin` 或 `prtn::b::i2c0::sda` 只是原生 `gpio_num_t` 默认值，不代表该引脚
已经被占用。真正的应用占用必须出现在最终应用的 `Pins` 中。

Pins that must never be assigned by applications belong in the selected board definition:

确实不能被应用使用的板载资源，应写进对应板卡定义：

```cpp
inline constexpr std::array ReservedPins {
    GPIO_NUM_48,
};
```

Use `ReservedPins` only for physical board reservations. A GPIO used by one optional application
component belongs in that application's layout instead.

`ReservedPins` 只用于板级硬保留。某个可选应用组件使用的 GPIO，仍应放在该应用自己的
布局里。

## Safety boundary / 安全边界

The compiler can validate only pins present in `Pins`. Passing a raw `GPIO_NUM_x` directly to a
driver bypasses application-level collision checking, so use `Pins[PinId::X]` consistently in final
applications.

编译器只能检查写进 `Pins` 的资源。如果最终应用绕过布局，直接把裸 `GPIO_NUM_x` 传给
驱动，就无法检查应用级冲突。因此最终应用应统一从 `Pins[PinId::X]` 取引脚。
