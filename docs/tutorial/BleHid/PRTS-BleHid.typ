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

#set document(title: "PRTN BLE HID：NKRO 键盘与鼠标", author: "PRTN Project")
#set page(
  paper: "a4",
  margin: (x: 18mm, top: 17mm, bottom: 18mm),
  fill: paper,
  header: align(right, text(size: 8pt, fill: muted)[PRTN / BLE HID / HOGP]),
  footer: context align(center, text(size: 8pt, fill: muted)[— #counter(page).display("1") —]),
)
#set text(font: "Noto Sans CJK SC", lang: "zh", region: "CN", size: 10pt, fill: ink)
#set par(justify: true, leading: .72em)
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
#let mental(body) = callout("心智模型 / Mental model", body, tone: cyan, pale: cyan-pale)
#let choice(body) = callout("设计选择 / Design choice", body, tone: green, pale: green-pale)
#let warning(body) = callout("容易踩坑 / Pitfall", body, tone: red, pale: red-pale)
#let practice(body) = callout("动手验证 / Verification", body, tone: amber, pale: amber-pale)

// Cover
#page(margin: 0pt, header: none, footer: none, fill: navy)[
  #place(top + left, dx: 19mm, dy: 20mm)[
    #block(width: 34mm, height: 4pt, fill: cyan, radius: 2pt)
  ]
  #place(center + horizon)[
    #align(center)[
      #text(font: "Noto Serif CJK SC", size: 31pt, weight: "bold", fill: white)[PRTN BLE HID]
      #v(8pt)
      #text(size: 18pt, weight: "medium", fill: rgb("#CDEEF3"))[NKRO 键盘与鼠标]
      #v(22pt)
      #block(width: 132mm, stroke: .7pt + rgb("#7FA4B9"), radius: 5pt, inset: 12pt)[
        #text(size: 11pt, fill: white)[
          面向 ESP32-S3 与 Arduino-ESP32 3.3.8：
          从 HID Report Descriptor、HOGP 和 GATT，走到一份真正可编译的组合设备。
        ]
      ]
      #v(28pt)
      #text(size: 10pt, fill: rgb("#AFC5D1"))[NKRO keyboard · relative mouse · bonding · reconnect]
    ]
  ]
  #place(bottom + left, dx: 19mm, dy: -18mm)[
    #text(size: 9pt, fill: rgb("#AFC5D1"))[PRTN Project · 2026]
  ]
]

#pagebreak()

= 阅读地图

这不是一篇“复制一个 BLE Keyboard 库然后调用 `print()`”的文章。我们的目标是看懂标准数据模型，并能回答：主机为什么把设备识别成键盘和鼠标、每一个字节在哪里被定义、为什么有时已经连接却收不到按键。

#mental[
  BLE HID 不是“往任意 Characteristic 写几个 keycode”。它是 *HID Report Descriptor* 定义数据语义，再由 *HID over GATT Profile（HOGP）* 规定这些报告如何放进标准 GATT 服务。
]

#outline(title: [目录], depth: 3, indent: 1.2em)

#pagebreak()

= 最终要做成什么

示例把 ESP32-S3 变成一个 BLE Peripheral / GATT Server。电脑或手机是 Central / GATT Client，也是 HID Host。连接并配对后，操作系统会同时创建键盘和鼠标输入设备。

#table(
  columns: (1.2fr, 1.45fr, 2.5fr),
  table.header([*参与者*], [*BLE/HID 角色*], [*职责*]),
  [ESP32-S3],
  [Peripheral、GATT Server、HID Device],
  [广播 HID 服务；向主机发送键盘和鼠标 Input Report；接收键盘 LED Output Report。],

  [电脑或手机],
  [Central、GATT Client、HID Host],
  [连接、配对、读取 Report Map、订阅 Input Report notification，并把报告交给操作系统输入栈。],

  [按键扫描器], [应用层输入源], [只产生稳定的 press/release 边沿；不需要理解 BLE。],
)

读完后，你应当能够：

1. 区分 HID descriptor、BLE attribute 和真正发送的 report body；
2. 修改 NKRO 范围、鼠标按键数或轴，而不靠猜字节；
3. 把矩阵扫描器接到 `setKey()`，并保证所有 press 都有 release；
4. 排查配对缓存、notification、报告长度和 Report ID 问题；
5. 决定什么时候需要 Just Works，什么时候必须增加 MITM 或物理授权。

= 三层协议，不要混在一起

#table(
  columns: (1.05fr, 1.65fr, 2.5fr),
  table.header([*层*], [*核心对象*], [*它负责什么*]),
  [HID], [Usage、Report Descriptor、Report], [定义“这一位是左 Ctrl”“这一个有符号字节是鼠标 X 相对位移”。],
  [HOGP / GATT], [Service、Characteristic、Descriptor], [规定 Report Map 放在哪里、报告如何被发现、订阅、读取或写入。],
  [BLE Link / Security], [广播、连接、加密、bond], [把两台设备连接起来，并决定谁可以读写受保护的 attribute。],
)

#warning[
  `inputReport` 和 `outputReport` 的方向永远站在 HID Device 视角：ESP32 → Host 是 Input；Host → ESP32 是 Output。Caps Lock 灯由电脑写回，因此它是 Output Report。
]

== HID Usage 不是字符

HID Keyboard/Keypad Usage Page 是物理键位集合。`0x04` 表示键盘上的 A 键，而不是 ASCII `'a'`。最终得到 `a`、`A`、`q` 或其他字符，取决于 Shift 等 modifier 和主机选择的键盘布局。

例如：

```cpp
setKey(Key::A, true);           // physical A-position pressed
setKey(Key::A, false);          // release it
setKey(Key::LeftShift, true);   // modifier lives in a separate byte
```

因此固件层最好暴露“usage 状态”，而不是假装自己直接发送 Unicode 字符。输入中文、日文或 compose sequence 是主机输入法的职责。

== Report Descriptor 是数据格式，不是数据

描述符里的项目像一门很小的声明式语言：

- `Usage Page` / `Usage`：这些字段在表达哪类控制；
- `Logical Minimum/Maximum`：数值范围；
- `Report Size`：每个字段多少 bit；
- `Report Count`：连续多少个字段；
- `Input`、`Output`、`Feature`：方向和语义属性；
- `Report ID`：一张 Report Map 中区分多种报告。

描述符不会随着按键变化。连接枚举时主机读取一次 Report Map，之后按这份契约解析每一条 Input Report。

= HOGP 在 GATT 中放了什么

Arduino-ESP32 的 `BLEHIDDevice` 创建 HID Service、Device Information Service 和 Battery Service。最重要的标准 attribute 如下：

#table(
  columns: (.85fr, 1.45fr, 2.7fr),
  table.header([*UUID*], [*名称*], [*用途*]),
  [`0x1812`], [HID Service], [HOGP 的主服务。广播中携带它，操作系统才会按 HID 路径发现设备。],
  [`0x2A4A`], [HID Information], [HID 版本、country code 和 flags。],
  [`0x2A4B`], [Report Map], [完整 HID Report Descriptor。],
  [`0x2A4C`], [HID Control Point], [Host 可请求 suspend / exit suspend。],
  [`0x2A4D`], [Report], [真正的 Input、Output 或 Feature Report characteristic；可以有多个实例。],
  [`0x2A4E`], [Protocol Mode], [Report Protocol 或 Boot Protocol。这个 NKRO 示例只实现 Report Protocol。],
  [`0x2908`], [Report Reference], [贴在每个 `0x2A4D` 上，声明 Report ID 和 Report Type。],
  [`0x2902`], [CCCD], [Host 用它启用或关闭 Input Report notification。],
)

== Report ID 在 BLE 中放在哪里

示例调用：

```cpp
m_keyboardInput  = m_hid->inputReport(1);
m_keyboardOutput = m_hid->outputReport(1);
m_mouseInput     = m_hid->inputReport(2);
```

`BLEHIDDevice` 会为这些 characteristic 添加 `0x2908` Report Reference。发送时 characteristic value 只包含报告正文：键盘 16 bytes，鼠标 4 bytes；不要再把 ID 塞到第一个 byte。

#warning[
  USB API 常见的 `SendReport(id, data, size)` 会单独接收 ID；BLE API 则先用 `inputReport(id)` 选定 characteristic。两套调用外形不同，不能机械照搬。
]

= 设计 NKRO 键盘报告

传统 boot keyboard 通常只有六个普通键槽位。NKRO 改用 bit map：每一个 usage 对应一位，同时按多少键不再受六槽限制。

本示例选择：

#table(
  columns: (1.2fr, 1.2fr, 2.6fr),
  table.header([*字段*], [*大小*], [*含义*]),
  [Modifiers], [1 byte], [bit 0..7 对应 usage `E0..E7`，即左右 Ctrl/Shift/Alt/GUI。],
  [Key bitmap], [15 bytes], [120 bits，对应 usage `0x00..0x77`。],
  [合计], [16 bytes], [这就是 keyboard Input Report characteristic value。],
)

usage 到位图位置的映射是：

```cpp
byteIndex = usage >> 3;
bitMask   = 1u << (usage & 0x07);
```

按下时 OR，释放时 AND NOT。Modifier 不写进这 15 bytes，而写入独立 modifier byte：

```cpp
modifierMask = 1u << (usage - 0xE0);
```

== 为什么只覆盖到 0x77

15 bytes 正好容纳 `0x00..0x77`，包括常用字母、数字、编辑键、方向键、功能键和大部分小键盘键。更高 usage 如果也需要 NKRO，可以扩大 bitmap，但必须同时修改：

- descriptor 的 Usage Maximum 和 Report Count；
- C++ report array 大小；
- usage 合法性检查；
- `static_assert` 期待的结构大小。

#choice[
  报告越大，每次按键边沿发送的数据越多。PRTN 现有 USB NKRO 也采用 modifier＋bitmap 思路；BLE 示例保持概念一致，但使用自己的 Report Map 和 GATT transport。
]

== LED Output Report

Host 会写一个 byte 回来，其中 bit 0..4 通常表示 Num Lock、Caps Lock、Scroll Lock、Compose、Kana。它不是 ESP32 主动查询出来的；必须给 keyboard output characteristic 设置 `onWrite` callback。

示例用 `std::atomic_uint8_t` 保存结果，让 BLE callback 和应用 task 之间读取不会形成 C++ data race。

= 设计鼠标报告

鼠标报告为四个 bytes：

#table(
  columns: (1.2fr, 1.2fr, 2.6fr),
  table.header([*字段*], [*大小*], [*含义*]),
  [Buttons], [5 bits＋3 bits padding], [左、右、中、后退、前进。],
  [X], [signed 8-bit], [水平相对位移，范围 -127..127。],
  [Y], [signed 8-bit], [垂直相对位移。],
  [Wheel], [signed 8-bit], [垂直滚轮相对量。],
)

descriptor 的 `Input` 值使用 `0x06`，表示 Data、Variable、*Relative*。这点决定 X/Y 是“本帧移动多少”，而不是屏幕绝对坐标。

按钮是持久状态，移动量是一次性增量。因此示例保存 `m_mouseButtons`，但不保存 x/y/wheel：

```cpp
const MouseReport report {m_mouseButtons, x, y, wheel};
m_mouseInput->setValue(reinterpret_cast<const uint8_t*>(&report), sizeof(report));
m_mouseInput->notify();
```

= 从零搭起 BLE HID

== 初始化和安全

关键顺序是：

1. `BLEDevice::init(deviceName)`；
2. 配置 pairing / bonding；
3. 创建 `BLEServer`；
4. 创建 `BLEHIDDevice`；
5. 设置 Report Map，并创建所有 report characteristics；
6. 启动 services；
7. 配置 advertising，最后开始广播。

示例使用：

```cpp
BLESecurity::setCapability(ESP_IO_CAP_NONE);
BLESecurity::setAuthenticationMode(true, false, true);
```

含义是 bonding 开、MITM 关、Secure Connections 开。对没有屏幕和确认键的设备，这会形成 Just Works 配对。

#warning[
  加密不等于可信身份。Just Works 能加密链路并保存 bond，却不能抵抗首次配对时的主动中间人。会执行敏感快捷键的产品，应加入物理配对窗口、确认键，或选择支持 MITM 的 IO capability。
]

== 创建报告 characteristic

键盘 Input 与 Output 使用相同 Report ID，因为 Type 不同；鼠标 Input 使用另一个 ID：

```cpp
m_keyboardInput  = m_hid->inputReport(1);
m_keyboardOutput = m_hid->outputReport(1);
m_mouseInput     = m_hid->inputReport(2);
```

Report Map 中出现的每种报告，都应存在对应的 GATT Report characteristic。反过来，characteristic 的 ID、Type 和 payload 必须与 Report Map 完全一致。

== 广播

组合设备使用 Generic HID appearance，并在广播中加入 HID Service UUID：

```cpp
advertising->setAppearance(GENERIC_HID);
advertising->addServiceUUID(m_hid->hidService()->getUUID());
advertising->setScanResponse(true);
BLEDevice::startAdvertising();
```

不要使用自定义 service UUID 代替 `0x1812`。自定义 GATT service 可以与 HID 并存，但操作系统不会因为某个任意 notification characteristic 就自动创建键盘。

= 代码结构：组合，而不是让 App 继承 Service

`BleNkroMouse` 拥有 server、HID device、characteristics、报告状态和两个 callback adapter。应用只持有一个 `BleNkroMouse` 对象。

```text
application / scanner
        |
        v
 BleNkroMouse
   |-- BLEHIDDevice
   |-- KeyboardReport
   |-- mouse button state
   |-- ServerCallbacks adapter
   `-- LedCallbacks adapter
```

Arduino BLE 库要求 callback 对象实现虚函数，所以两个小 adapter 必须继承库接口。这是 SDK 边界上的适配，不意味着 App、扫描器或业务 Service 也要继承 BLE 类。

== 跨 task 状态

BLE callback 可能运行在 BLE host task，而扫描和发送通常运行在 Arduino loop 或独立 FreeRTOS task。示例对 `connected` 和 `keyboardLeds` 使用 atomic。

完整产品还应保证只有一个 task 改写 keyboard report 和 mouse button state。推荐让扫描 task 成为 HID 状态唯一所有者；其他 task 通过 queue 投递语义事件，不要让多个 task 同时调用 `setKey()`。

== 不自动注入测试按键

示例启动后只广播和记录连接状态，不会定时输入字符。自动发送按键会在用户刚配对时污染当前窗口，甚至触发快捷键，既不安全也不利于排错。

接入矩阵扫描器时使用边沿事件：

```cpp
if (event.pressed)
    hid.setKey(toHidUsage(event.key), true);
else
    hid.setKey(toHidUsage(event.key), false);
```

如果扫描器重置、映射层切换或事件队列溢出，调用一次 `releaseAllKeys()`，避免主机认为某个键永久按住。

= 生命周期和错误处理

示例为了保持教程集中，`begin()` 和发送方法返回 `bool`。迁入 PRTN 正式代码时，应映射为项目的 typed `Result`，至少区分：

- already started / invalid device name；
- BLE init、server、HID characteristic 或 advertising 创建失败；
- not started / disconnected；
- invalid keyboard usage；
- report notification 失败或连接在发送期间丢失。

#warning[
  Arduino-ESP32 3.3.8 的 `BLECharacteristic::notify()` 返回 `void`，因此示例只能在调用前检查连接，不能从该调用取得逐包成功确认。断线和订阅状态必须通过 callback、协议行为及日志综合判断。
]

断线时示例会：

1. 原子地标记 disconnected；
2. 原子地记录 reset pending，由拥有报告状态的应用 task 在下一次更新前清空键盘和鼠标按钮；
3. 重新开始广播。

callback 不直接改写 `KeyboardReport`，因此不会和应用 task 形成 data race。本地清空也不会在已经断开的链路上发送 release；主机通常会在 HID 设备断开时清除输入状态，重连后固件不会把断线前的旧状态重新发出去。

= 配对、bond 和缓存

首次连接时，主机读取 GATT database 和 Report Map，完成配对，并可能长期缓存服务布局。修改 descriptor、Report ID 或 characteristic 数量后，如果主机还保存旧 bond，就可能出现：

- 已连接但没有输入设备；
- 键盘工作、鼠标不出现；
- notification 订阅了旧 handle；
- 主机用旧长度解析新报告。

#practice[
  开发阶段每次修改 Report Map 后，都从操作系统“忘记/移除”该设备，并清除 ESP32 保存的 bond 后重新配对。只重启 ESP32 通常不足以刷新主机缓存。
]

= 测试方法

== 编译

示例目录是一个独立 Arduino sketch，可使用与 PRTN 相同的 board FQBN 编译：

```sh
arduino-cli compile \
  --fqbn esp32:esp32:esp32s3 \
  docs/tutorial/BleHid/examples/BleHidNkroMouse
```

实际项目可追加 `config/boards/ESP32S3_N16R8_DEV.mk` 中的 Flash、PSRAM 和 USB 选项。`KeyboardReport` 和 `MouseReport` 都有 `static_assert`，防止 C++ padding 静默破坏 wire format。

== Linux

配对和连接：

```sh
bluetoothctl
power on
agent on
default-agent
scan on
pair <MAC>
trust <MAC>
connect <MAC>
```

观察协议与输入事件：

```sh
sudo btmon
sudo evtest
```

至少验证：

- 单键、多个普通键以及左右 modifier 同时按下；
- 每一个 press 都出现 release，`releaseAllKeys()` 能解除全部状态；
- 鼠标正负 X/Y、滚轮和五个按钮；
- Host 切换 Caps Lock 时 `keyboardLeds()` 的 bit 1 变化；
- 断线、重新广播、重连后没有幽灵按键；
- ESP32 和 Host 重启后 bond 可以恢复连接。

== Windows 和 macOS

用系统设置完成首次配对，然后分别检查键盘输入和鼠标移动。若 Report Map 改动后行为诡异，先在系统设置中删除设备再配对。Windows 还可能保留 HID/GATT 缓存；开发时改变设备名或清除设备条目可以帮助确认是否为缓存问题。

= 常见症状速查

#table(
  columns: (1.6fr, 2fr, 2.25fr),
  table.header([*症状*], [*优先检查*], [*典型原因*]),
  [能扫描，不能作为 HID 连接], [广播是否包含 `0x1812`；安全日志], [只创建了自定义 service，或 pairing/security 失败。],
  [连接了但无按键], [CCCD、Report Reference、connected flag], [Host 未订阅 notification，或 ID/Type 不匹配。],
  [键盘出现，鼠标不出现],
  [Report Map 的第二个 Application Collection；mouse inputReport(2)],
  [只声明了 descriptor，没创建对应 characteristic，或旧缓存。],

  [按一次后一直重复], [release 边沿和断线清理], [只发送 press，没有发送完整的 release 状态。],
  [键位错乱], [usage 与主机键盘布局], [把 HID usage 当成 ASCII，或在固件里硬编码字符布局。],
  [移动方向或速度异常], [signed 类型和 Relative flag], [使用了 `uint8_t`、Absolute，或结构长度不符。],
  [修改 descriptor 后彻底异常], [删除 Host 配对并清 bond], [GATT database / Report Map 被缓存。],
)

= 下一步如何接入 PRTN

教程示例故意保持独立，没有修改当前生产应用。正式接入时建议分成三层：

1. `BleHidProtocol.h`：只保存 Report Map、Report ID、packed report types 和 usage 常量；
2. `BleHidTransport`：组合 `BLEHIDDevice`，负责生命周期、连接、安全与 notification；
3. `KeyboardMouseService`：接收 scanner 事件，维护唯一的输入状态，并使用 typed `Result` 向应用报告失败。

这样现有 `HidProtocol.h` 中的通用 usage 常量可以复用，但 USB 的 `USBHID` 和 BLE 的 `BLEHIDDevice` 不会被塞进同一个 transport 类。

= 完整可编译示例

下列内容直接读取仓库中的 `.ino`，因此教程与实际编译的代码保持同一来源。

#raw(
  read("examples/BleHidNkroMouse/BleHidNkroMouse.ino"),
  lang: "cpp",
  block: true,
)

= 官方资料

- #link("https://www.bluetooth.com/specifications/specs/hid-over-gatt-profile/")[Bluetooth SIG: HID over GATT Profile] —— HOGP 角色、安全、服务与过程。
- #link("https://www.bluetooth.com/specifications/specs/hid-service/")[Bluetooth SIG: HID Service] —— HID Service 的 characteristics 和 descriptors。
- #link("https://usb.org/document-library/device-class-definition-hid-111")[USB-IF: Device Class Definition for HID 1.11] —— HID report descriptor 与 report protocol 的基础定义。
- #link("https://usb.org/document-library/hid-usage-tables-15")[USB-IF: HID Usage Tables] —— Keyboard、Mouse、Consumer 等 usage page 的权威表格。
- #link("https://github.com/espressif/arduino-esp32/tree/master/libraries/BLE")[Espressif: Arduino-ESP32 BLE library] —— 本教程使用的 `BLEHIDDevice`、security 和 server API 来源。

#mental[
  最可靠的开发顺序是：先让 Report Map 和 packed C++ types 严格一致，再让 GATT 中的 Report ID/Type 与它一致，最后才接扫描器和业务逻辑。三层逐一验证，比“连接成功后不断改随机字节”快得多。
]
