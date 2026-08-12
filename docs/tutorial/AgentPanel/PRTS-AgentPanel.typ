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

#set document(title: "PRTS Agent Panel：从 BLE 心智模型到 GATT 实现", author: "PRTN Project")
#set page(
  paper: "a4",
  margin: (x: 18mm, top: 17mm, bottom: 18mm),
  fill: paper,
  header: align(right, text(size: 8pt, fill: muted)[PRTN / Agent Panel / BLE GATT]),
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
#show raw: set text(font: "JetBrains Maple Mono", size: 7.5pt)
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
#let mental(body) = callout("心智模型", body, tone: cyan, pale: cyan-pale)
#let choice(body) = callout("为什么这样设计", body, tone: green, pale: green-pale)
#let warning(body) = callout("容易踩坑", body, tone: red, pale: red-pale)
#let practice(body) = callout("动手验证", body, tone: amber, pale: amber-pale)
#let term(name, desc) = [#text(weight: "bold", fill: navy)[#name] #desc]

// Cover
#page(margin: 0pt, header: none, footer: none, fill: navy)[
  #place(top + left, dx: 19mm, dy: 20mm)[
    #block(width: 34mm, height: 4pt, fill: cyan, radius: 2pt)
  ]
  #place(center)[
    #align(center)[
      #text(font: "Noto Serif CJK SC", size: 32pt, weight: "bold", fill: white)[PRTS Agent Panel]
      #v(8pt)
      #text(size: 18pt, weight: "medium", fill: rgb("#CDEEF3"))[从 BLE 心智模型到 GATT 实现]
      #v(22pt)
      #block(width: 130mm, stroke: .7pt + rgb("#7FA4B9"), radius: 5pt, inset: 12pt)[
        #text(size: 11pt, fill: white)[
          一份面向 ESP32-S3 与跨平台 Rust broker 的中文教程：
          先理解广播、ATT、GATT、配对与可靠性，再写出边界清楚、没有魔法的代码。
        ]
      ]
      #v(28pt)
      #text(size: 10pt, fill: rgb("#AFC5D1"))[协议版本 1.0 · Arduino-ESP32 3.3.8 · btleplug 0.12]
    ]
  ]
  #place(bottom + left, dx: 19mm, dy: -18mm)[
    #text(size: 9pt, fill: rgb("#AFC5D1"))[PRTN Project · 2026]
  ]
]

#pagebreak()

= 阅读地图

这不是一篇“复制一段 BLE 代码就结束”的文章。它试图让你在看见任何 GATT 设计时，都能回答三个问题：数据在哪里、谁能改变它、失败究竟发生在哪一层。

#mental[
  把整套系统先压缩成一句话：*一个 Rust broker 通过一条加密 BLE 连接，把多个本地 agent 的语义命令串行送给面板；面板返回每条命令的应用层结果。*
]

#outline(title: [目录], depth: 3, indent: 1.2em)

#pagebreak()

= 先把问题说清楚

== 产品边界

Agent Panel 是一个物理设备，但它同时显示最多 7 个逻辑 agent。屏幕为旋转后的 128×64 SSD1306：纵向 128 像素中预留 16 像素，余下 112 像素按每行 16 像素正好容纳 7 行。灯光与屏幕渲染由已有业务状态机负责，BLE 层只交付以下语义：

- agent 注册和注销；
- agent 状态变化；
- 一小段 UTF-8 文本；
- 命令成功或失败。

面板不是聊天服务器，也不做任意文件传输。这个边界很重要：GATT 设计越贴近真实需求，越不容易长成一个低效而难以版本化的“万能串口”。

== 参与者与方向

#table(
  columns: (1.15fr, 1.35fr, 2.5fr),
  table.header([*参与者*], [*BLE 角色*], [*职责*]),
  [ESP32-S3 面板], [Peripheral / GATT Server], [广播服务；保存 7 个 slot；执行命令；通过 Indication 返回结果。],
  [Rust broker], [Central / GATT Client], [扫描、连接、订阅、串行化命令；在 BLE 与本机 agent 之间做复用。],
  [本机 agent], [不是 BLE 参与者], [通过 loopback TCP + NDJSON 使用 broker；每条连接代表一个 agent。],
  [操作系统], [安全与适配器管理者], [完成首次 Just Works 配对、保存 bond、管理蓝牙权限。],
)

#choice[
  不让每个 agent 各自连接 BLE。BLE 连接数、系统权限、重连与配对状态都属于昂贵的共享资源；broker 是唯一所有者，agent 只看到稳定的本地接口。
]

== 成功标准

读完后，你应当能够：

1. 不再混淆 Central、Peripheral、Client、Server；
2. 从需求推导 Characteristic，而不是从示例代码倒推产品；
3. 解释默认 MTU 下为什么把帧限制在 20 字节；
4. 区分 ATT 写入确认和业务执行结果；
5. 解释配对、加密、bond 各自在保护什么；
6. 看懂并修改文末的 ESP32 与 Rust 参考实现。

= BLE 的分层心智模型

== 一张图看完整栈

#align(center)[
  #grid(
    columns: (1fr, 12mm, 1fr),
    row-gutter: 4pt,
    column-gutter: 4pt,
    block(fill: green-pale, stroke: .6pt + green, inset: 7pt, radius: 3pt)[*应用协议*\REGISTER / SET_STATE / SET_TEXT],
    align(center)[↔],
    block(fill: green-pale, stroke: .6pt + green, inset: 7pt, radius: 3pt)[*应用协议*\slot registry / command result],

    block(fill: cyan-pale, stroke: .6pt + cyan, inset: 7pt, radius: 3pt)[*GATT Client*\发现、读、写、订阅],
    align(center)[↔],
    block(fill: cyan-pale, stroke: .6pt + cyan, inset: 7pt, radius: 3pt)[*GATT Server*\Service / Characteristic],

    block(fill: amber-pale, stroke: .6pt + amber, inset: 7pt, radius: 3pt)[*ATT*\handle、opcode、MTU],
    align(center)[↔],
    block(fill: amber-pale, stroke: .6pt + amber, inset: 7pt, radius: 3pt)[*ATT*\attribute database],

    block(fill: red-pale, stroke: .6pt + red, inset: 7pt, radius: 3pt)[*Link / Security*\连接、加密、重传],
    align(center)[↔],
    block(fill: red-pale, stroke: .6pt + red, inset: 7pt, radius: 3pt)[*Link / Security*\广播、bond、射频],

    align(center)[Rust broker], [], align(center)[ESP32 Panel],
  )
]

#mental[
  GATT 不是“传输协议本身”，而是建立在 ATT 之上的数据模型与操作约定。你的 REGISTER 帧又建立在 GATT Characteristic 之上。排错时永远问：这是链路、ATT、GATT，还是应用协议的问题？
]

== 四个最容易混淆的角色

#table(
  columns: (1fr, 1.5fr, 2.2fr),
  table.header([*词*], [*它回答的问题*], [*本项目*]),
  [Central], [谁发起连接？], [Rust broker。],
  [Peripheral], [谁广播、等待连接？], [ESP32 面板。],
  [GATT Client], [谁发起 read/write/subscribe？], [Rust broker。],
  [GATT Server], [谁持有 Attribute Database？], [ESP32 面板。],
)

Central 通常也是 GATT Client，但这是常见组合，不是定义上的同义词。角色属于不同层：Central/Peripheral 描述链路建立，Client/Server 描述 GATT 操作。

== Attribute、Service、Characteristic、Descriptor

- #term("Attribute：", [ATT 数据库中的最小记录，由 handle、type、value、permission 构成。])
- #term("Service：", [把一组有关能力放在同一个命名空间；本项目只有一个自定义 Primary Service。])
- #term("Characteristic：", [包含声明与 value，并规定 Read、Write、Indicate 等性质。])
- #term("Descriptor：", [补充 Characteristic 元数据。订阅 Notify/Indicate 实际会写 CCCD。])

UUID 只是“这是什么”的标识，不携带权限、方向或长度。一个 UUID 设计得再漂亮，也不能替代线上的帧格式和状态机。

== 广播、扫描响应与连接

未连接时，ESP32 周期性广播。经典 Advertising 数据空间很小，因此本设计把 128-bit Service UUID 放入 Advertising，把设备名 `PRTN-AgentPanel` 放入 Scan Response。broker 先按 Service UUID 过滤，再用名称辅助日志展示。

#align(center)[
  #table(
    columns: (1.2fr, 2.1fr, 1.6fr),
    table.header([*阶段*], [*发生什么*], [*失败含义*]),
    [Advertising], [面板声明“我提供这个 Service”。], [未必是连接故障，可能只是扫描权限或过滤条件错误。],
    [Connecting], [Central 建立链路。], [射频、超时、适配器或设备已被占用。],
    [Discovery], [Client 读取 GATT 结构。], [固件 UUID/缓存不一致。],
    [Subscribe], [Client 写 Event TX 的 CCCD。], [没有订阅就不能等待应用结果。],
    [Command], [Write With Response + Indication。], [须继续区分 ATT 与应用层错误。],
  )
]

== ATT MTU 与“20 字节”从哪里来

ATT 默认 MTU 是 23。一次普通 ATT Write Request 的 payload 要扣除 1 字节 opcode 和 2 字节 handle，所以 Characteristic value 常见上限为：

#align(center)[#text(size: 18pt, weight: "bold", fill: navy)[23 − 3 = 20 bytes]]

协商更大 MTU 完全可行，但它会引入平台差异、分片与更多边界测试。Agent Panel 的命令天然很小，因此 v1 主动把每帧压在 20 字节内：不是 BLE 只能发 20 字节，而是我们选择用更简单、可预测的协议换取可靠性。

#warning[
  “字符数”不等于“UTF-8 字节数”。名称限制 15 字节、动态文本限制 18 字节。中文通常占 3 字节，必须先验证 UTF-8，再按字节限制；不能从中间截断一个 code point。
]

== Read、Write、Notify、Indicate

#table(
  columns: (1fr, 1.4fr, 1.3fr, 2fr),
  table.header([*操作*], [*方向*], [*链路语义*], [*适用场景*]),
  [Read], [Client → Server → Client], [请求/响应], [不常变化的 Protocol Info。],
  [Write Request], [Client → Server], [ATT 层有响应], [Command RX；证明 server 接收了写请求。],
  [Write Command], [Client → Server], [无 ATT 响应], [高吞吐、可容忍丢失；本项目不用。],
  [Notify], [Server → Client], [不要求确认], [高频传感数据。],
  [Indicate], [Server → Client], [Client 确认], [低频但重要的命令执行结果。],
)

#mental[
  Write With Response 的成功，只说明 ATT 写入被接受；它不证明 REGISTER 已分配 slot。应用结果必须由 Event TX 另行返回。两层确认解决的是两个不同问题。
]

== 配对、加密与 bond

- #term("Pairing：", [双方协商密钥的过程。])
- #term("Encryption：", [当前连接启用链路加密。])
- #term("Bonding：", [把长期密钥保存下来，让下次连接不必重新配对。])
- #term("MITM protection：", [防止主动中间人；通常需要显示器、键盘或 OOB 能力。])

本设备没有可信输入/输出能力，因此 v1 使用 Just Works + bonding + Secure Connections，不宣称 MITM 防护。首次配对交给 Windows、macOS 或 Linux 的系统 UI/工具；`btleplug` 只负责跨平台 GATT，不假装拥有统一的配对 API。

只允许一个 bonded host。清除 bond 必须通过物理按钮或串口等本地动作完成，绝不暴露远程 GATT 命令，否则任何已连上的客户端都可能把设备“踢出家门”。

= 从需求推导 GATT

== 为什么不是“一名 agent 一组 Characteristic”

如果为 7 个 slot 分别建立 name、state、text、result Characteristic，会得到 28 个 value，还要处理未使用 slot、动态身份、重复订阅和版本迁移。GATT 数据库是静态的，而 agent 生命周期是动态的；把动态实体硬编码成静态 attribute 是抽象错位。

Control Point 模式只需要四个 Characteristic：一个只读能力描述、一个命令入口、一个结果出口，加上 Service 本身。agent id 放进应用帧。

#choice[
  GATT 负责“有哪几条通道以及怎么访问”，应用协议负责“这条消息是什么意思”。静态结构交给 GATT，动态实体交给紧凑帧。
]

== 最终 GATT 表

#table(
  columns: (1.25fr, 2.25fr, 1.15fr, 1.4fr),
  text(size: 8pt)[*名称*], text(size: 8pt)[*UUID*], text(size: 8pt)[*性质*], text(size: 8pt)[*权限/用途*],
  [Primary Service], text(size: 7pt)[6e291bc2-80c8-484d-a505-49509c3c868e], [—], [Agent Panel v1],
  [Protocol Info], text(size: 7pt)[c2ef6547-91b4-4367-8b1c-0514c5bb8f42], [Read], [加密读；固定 8 字节],
  [Command RX], text(size: 7pt)[b6c66dd2-4047-473c-93f3-d97d9405330c], [Write], [加密写；Write With Response],
  [Event TX], text(size: 7pt)[1c69273f-d0fb-447a-90c1-0c472a0b7b53], [Indicate], [加密；应用结果],
)

== Protocol Info：先读能力，再说话

Protocol Info 固定 8 字节：

#table(
  columns: (.7fr, 1.25fr, 1fr, 2.5fr),
  table.header([*偏移*], [*字段*], [*v1 值*], [*含义*]),
  [0], [major], [1], [不兼容变化时递增。],
  [1], [minor], [0], [向后兼容新增时递增。],
  [2], [max_agents], [7], [slot 数。],
  [3], [max_name_bytes], [15], [REGISTER 名称上限。],
  [4], [max_text_bytes], [18], [SET_TEXT 文本上限。],
  [5], [features], [0x0F], [bit0 lease、bit1 text、bit2 encrypted write、bit3 result indicate。],
  [6..7], [lease_seconds], [30 LE], [断线保留窗口。],
)

线上的 8 字节为：`01 00 07 0F 12 0F 1E 00`。

== Command RX 帧

所有多字节整数使用 little-endian；第一字节永远是 opcode。

#table(
  columns: (1.15fr, .7fr, 2.8fr, 1.1fr),
  table.header([*命令*], [*opcode*], [*payload*], [*总长度*]),
  [REGISTER], [0x01], [`agent_key:u32 LE + name[1..15]`], [6..20],
  [SET_STATE], [0x02], [`agent_id:u8 + state:u8`], [3],
  [SET_TEXT], [0x03], [`agent_id:u8 + text[0..18]`], [2..20],
  [UNREGISTER], [0x04], [`agent_id:u8`], [2],
)

`agent_key` 是 broker 为本地连接生成的非零随机 32-bit 值。它不是安全凭据，而是短期重连身份：同一 key 重复 REGISTER 必须返回同一个 slot，并更新名称。

== Agent State

#table(
  columns: (.7fr, 1.5fr, 3fr),
  table.header([*值*], [*名称*], [*语义*]),
  [0], [OFF], [slot 存在但不主动显示工作活动。],
  [1], [IDLE], [已注册，等待任务。],
  [2], [WORKING], [正在执行。],
  [3], [WAIT_PERMISSION], [等待用户授权。],
  [4], [WAIT_OPTION], [等待用户选择。],
  [5], [DONE], [任务完成；渲染层可在一段时间后回到 IDLE。],
  [6], [ERROR], [任务或工具链失败。],
)

BLE 协议不规定颜色、动画、蜂鸣或 DONE 停留时长。它只传达语义枚举，避免把硬件表现写死在线上协议里。

== Event TX 结果帧

每条命令恰好对应一次 4 字节 Indication：

```text
byte 0: 0x80
byte 1: request_opcode
byte 2: status
byte 3: agent_id，无法确定时为 0xFF
```

#table(
  columns: (.7fr, 1.55fr, 3fr),
  table.header([*值*], [*状态*], [*含义*]),
  [0], [OK], [业务动作已完成。],
  [1], [INVALID_OPCODE], [未知命令。],
  [2], [INVALID_LENGTH], [长度不符合对应 opcode。],
  [3], [INVALID_UTF8], [名称或文本不是完整合法 UTF-8。],
  [4], [NO_FREE_SLOT], [7 个 slot 均被占用或仍在 lease 中。],
  [5], [UNKNOWN_AGENT], [agent id 不存在。],
  [6], [INVALID_STATE], [状态值不在 0..6。],
  [7], [BUSY], [面板暂不能处理；broker 可以有界重试。],
  [8], [INTERNAL_ERROR], [内部不变量或设备操作失败。],
)

broker 必须先订阅 Event TX，再允许写 Command RX；且全局只保留一个 outstanding command。因此结果只需回显 opcode，不需要 transaction id。若未来允许流水线并发，协议 major 必须升级并增加 transaction id。

== Slot、lease 与恢复

#align(center)[
  #grid(
    columns: (1fr, 10mm, 1fr, 10mm, 1fr),
    block(fill: cyan-pale, stroke: .6pt + cyan, inset: 7pt, radius: 3pt)[*空闲*\最低 id 优先],
    align(center)[→],
    block(fill: green-pale, stroke: .6pt + green, inset: 7pt, radius: 3pt)[*在线*\key + name + state],
    align(center)[→],
    block(fill: amber-pale, stroke: .6pt + amber, inset: 7pt, radius: 3pt)[*保留*\断线后 30 秒],
  )
]

- 正常 UNREGISTER：立即释放。
- BLE 断线：所有 active slot 进入 30 秒 lease，期间不分给新 key。
- 同一 key 在 lease 内 REGISTER：取回原 slot、更新名称并恢复 active。
- lease 到期：释放 slot，并通知渲染层移除。
- ESP32 重启：内存 registry 丢失；broker 连接后重新注册所有本地 agent。
- broker 重启：本地 agent 的 TCP 连接断开；客户端重连并获得新 key。

= 一条命令到底怎么走

== 首次连接

#table(
  columns: (.6fr, 1.2fr, 1.2fr, 2.5fr),
  table.header([*步*], [*发起方*], [*接收方*], [*动作*]),
  [1], [ESP32], [空气], [广播 Service UUID；名称在 Scan Response。],
  [2], [broker], [ESP32], [连接；若无 bond，由 OS 发起 Just Works pairing。],
  [3], [broker], [ESP32], [discover services；核对三个 Characteristic。],
  [4], [broker], [Event TX], [subscribe，写 CCCD 开启 Indication。],
  [5], [broker], [Protocol Info], [read 并校验 major、限制与 feature。],
  [6], [broker], [Command RX], [REGISTER，Write With Response。],
  [7], [ESP32], [Event TX], [Indicate `80 01 00 id`。],
)

== SET_STATE 的双重确认

#align(center)[
  #block(width: 100%, fill: rgb("#F5F8FA"), stroke: .6pt + line, inset: 10pt, radius: 4pt)[
    #grid(
      columns: (1fr, 12mm, 1fr),
      [*Rust broker*], [], align(right)[*ESP32*],
      [编码 `02 id state`], align(center)[—— Write Request →], [BLE stack 收到 value],
      [收到 ATT Write Response], align(center)[← ATT response ——], [ATT 层确认写入],
      [等待应用结果], align(center)[················], [解析、查 slot、更新状态],
      [匹配 opcode，完成 future], align(center)[← Indication `80 02 00 id` ——], [应用层确认成功],
      [链路确认 Indication], align(center)[—— confirmation →], [发送完成],
    )
  ]
]

#warning[
  不要在 BLE 回调中做耗时渲染或等待另一个锁。回调只做定长解析、registry 更新和小型事件投递。`indicate()` 本身会等待确认，正式项目可把结果发送放到专用任务；参考代码为了看清机制保持同步，但明确标出这条边界。
]

= ESP32：先看穿库的魔法

== 最小 GATT “X 光片”

下面不是最终实现，只展示四件事：建 Service、建 Characteristic、设置加密权限、在写回调里 Indicate。

```cpp
#include <Arduino.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLESecurity.h>

constexpr char SERVICE_UUID[] = "6e291bc2-80c8-484d-a505-49509c3c868e";
constexpr char COMMAND_UUID[] = "b6c66dd2-4047-473c-93f3-d97d9405330c";
constexpr char EVENT_UUID[]   = "1c69273f-d0fb-447a-90c1-0c472a0b7b53";

BLECharacteristic* eventTx = nullptr;

class CommandCallbacks final : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* commandRx) override {
    const uint8_t* bytes = commandRx->getData();
    const size_t size = commandRx->getLength();
    const uint8_t opcode = size == 0 ? 0 : bytes[0];
    const uint8_t result[] = {0x80, opcode, 0x00, 0xFF};
    eventTx->setValue(result, sizeof(result));
    eventTx->indicate();
  }
};

void setup() {
  BLEDevice::init("PRTN-AgentPanel");
  BLESecurity::setCapability(ESP_IO_CAP_NONE);
  BLESecurity::setAuthenticationMode(true, false, true); // bond, !MITM, SC

  BLEServer* server = BLEDevice::createServer();
  server->advertiseOnDisconnect(true);
  BLEService* service = server->createService(SERVICE_UUID);

  auto* commandRx = service->createCharacteristic(
      COMMAND_UUID, BLECharacteristic::PROPERTY_WRITE);
  eventTx = service->createCharacteristic(
      EVENT_UUID, BLECharacteristic::PROPERTY_INDICATE);

  // Arduino-ESP32 的 Bluedroid 后端不能靠 PROPERTY_WRITE_ENC 表达权限。
  commandRx->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);
  eventTx->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
  eventTx->addDescriptor(new BLE2902());
  commandRx->setCallbacks(new CommandCallbacks());

  service->start();
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();
}

void loop() { delay(1000); }
```

逐层读它：`PROPERTY_WRITE` 声明 GATT 操作；`ESP_GATT_PERM_WRITE_ENCRYPTED` 是访问控制；`onWrite` 是从 BLE transport 跨入应用协议的边界；`indicate()` 是应用响应的 transport。它没有 registry，所以不是产品代码。

== 最终代码的边界

最终参考实现分成三个概念文件，代码仍可按项目风格合并：

```text
AgentPanelProtocol.h   线上的常量、定长类型、UTF-8 验证和 decode
AgentRegistry.h        7 个 slot、lease、业务 sink；完全不知道 BLE
AgentPanelGatt.*       Arduino BLE 适配、权限、广告和回调
```

这种分层让协议解析可在 PC 上单测，让 registry 不依赖无线栈，让 GATT 回调只负责桥接。

== 协议类型与解码

```cpp
// AgentPanelProtocol.h
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "src/fw/inc/Result.h"

namespace agent_panel {

inline constexpr char kServiceUuid[] =
    "6e291bc2-80c8-484d-a505-49509c3c868e";
inline constexpr char kInfoUuid[] =
    "c2ef6547-91b4-4367-8b1c-0514c5bb8f42";
inline constexpr char kCommandUuid[] =
    "b6c66dd2-4047-473c-93f3-d97d9405330c";
inline constexpr char kEventUuid[] =
    "1c69273f-d0fb-447a-90c1-0c472a0b7b53";

inline constexpr std::size_t kAgentCount = 7;
inline constexpr std::size_t kMaxName = 15;
inline constexpr std::size_t kMaxText = 18;
inline constexpr uint32_t kLeaseMs = 30'000;
inline constexpr uint8_t kNoAgent = 0xFF;

enum class Op : uint8_t {
  Register = 0x01, SetState = 0x02, SetText = 0x03, Unregister = 0x04,
};

enum class AgentState : uint8_t {
  Off = 0, Idle, Working, WaitPermission, WaitOption, Done, Error,
};

enum class Status : uint8_t {
  Ok = 0, InvalidOpcode, InvalidLength, InvalidUtf8, NoFreeSlot,
  UnknownAgent, InvalidState, Busy, InternalError,
};

struct Command {
  Op op{};
  uint32_t key{};
  uint8_t agentId{kNoAgent};
  AgentState state{AgentState::Off};
  uint8_t size{};
  std::array<uint8_t, kMaxText> bytes{};
};

[[nodiscard]] inline bool validUtf8(const uint8_t* p, std::size_t n) {
  std::size_t i = 0;
  while (i < n) {
    const uint8_t c = p[i++];
    if (c <= 0x7F) continue;
    unsigned trail = 0;
    uint32_t value = 0;
    uint32_t minimum = 0;
    if ((c & 0xE0) == 0xC0) { trail = 1; value = c & 0x1F; minimum = 0x80; }
    else if ((c & 0xF0) == 0xE0) { trail = 2; value = c & 0x0F; minimum = 0x800; }
    else if ((c & 0xF8) == 0xF0) { trail = 3; value = c & 0x07; minimum = 0x10000; }
    else return false;
    if (i + trail > n) return false;
    while (trail--) {
      const uint8_t t = p[i++];
      if ((t & 0xC0) != 0x80) return false;
      value = (value << 6) | (t & 0x3F);
    }
    if (value < minimum || value > 0x10FFFF ||
        (value >= 0xD800 && value <= 0xDFFF)) return false;
  }
  return true;
}

[[nodiscard]] inline uint32_t readLe32(const uint8_t* p) {
  return uint32_t{p[0]} | (uint32_t{p[1]} << 8) |
         (uint32_t{p[2]} << 16) | (uint32_t{p[3]} << 24);
}

[[nodiscard]] inline Result<Command, Status>
decode(const uint8_t* p, std::size_t n) {
  if (n == 0 || n > 20) return Err(Status::InvalidLength);
  Command out{};
  out.op = static_cast<Op>(p[0]);

  switch (out.op) {
    case Op::Register: {
      if (n < 6 || n > 20) return Err(Status::InvalidLength);
      out.key = readLe32(p + 1);
      if (out.key == 0) return Err(Status::InvalidLength);
      out.size = static_cast<uint8_t>(n - 5);
      if (!validUtf8(p + 5, out.size)) return Err(Status::InvalidUtf8);
      for (uint8_t i = 0; i < out.size; ++i) out.bytes[i] = p[5 + i];
      return Ok(out);
    }
    case Op::SetState:
      if (n != 3) return Err(Status::InvalidLength);
      if (p[2] > static_cast<uint8_t>(AgentState::Error))
        return Err(Status::InvalidState);
      out.agentId = p[1];
      out.state = static_cast<AgentState>(p[2]);
      return Ok(out);
    case Op::SetText:
      if (n < 2 || n > 20) return Err(Status::InvalidLength);
      out.agentId = p[1];
      out.size = static_cast<uint8_t>(n - 2);
      if (!validUtf8(p + 2, out.size)) return Err(Status::InvalidUtf8);
      for (uint8_t i = 0; i < out.size; ++i) out.bytes[i] = p[2 + i];
      return Ok(out);
    case Op::Unregister:
      if (n != 2) return Err(Status::InvalidLength);
      out.agentId = p[1];
      return Ok(out);
  }
  return Err(Status::InvalidOpcode);
}

[[nodiscard]] inline std::array<uint8_t, 4>
resultFrame(uint8_t opcode, Status status, uint8_t id = kNoAgent) {
  return {0x80, opcode, static_cast<uint8_t>(status), id};
}

inline constexpr std::array<uint8_t, 8> kProtocolInfo{
  1, 0, 7, 15, 18, 0x0F, 30, 0
};

} // namespace agent_panel
```

#warning[
  `Result` 在这里表达“解码要么得到 Command，要么得到 Status”，但它不替代线上验证。来自 BLE 的每一个字节仍是不可信输入。类型系统负责让本地分支清楚，parser 负责守住外部边界。
]

== Registry 与业务 sink

```cpp
// AgentRegistry.h
#pragma once

#include <array>
#include <cstdint>
#include "AgentPanelProtocol.h"

namespace agent_panel {

class AgentSink {
public:
  virtual ~AgentSink() = default;
  virtual void onRegistered(uint8_t id, const uint8_t* name, uint8_t size) = 0;
  virtual void onState(uint8_t id, AgentState state) = 0;
  virtual void onText(uint8_t id, const uint8_t* text, uint8_t size) = 0;
  virtual void onRemoved(uint8_t id) = 0;
};

class Registry {
  struct Slot {
    bool used{};
    bool active{};
    uint32_t key{};
    uint32_t leaseStarted{};
    AgentState state{AgentState::Idle};
    uint8_t nameSize{};
    std::array<uint8_t, kMaxName> name{};
  };

  std::array<Slot, kAgentCount> slots_{};
  AgentSink& sink_;

  [[nodiscard]] static bool elapsed(uint32_t now, uint32_t then, uint32_t ms) {
    return static_cast<uint32_t>(now - then) >= ms; // millis() wrap-safe
  }

  void setName(Slot& slot, const uint8_t* name, uint8_t size) {
    slot.nameSize = size;
    for (uint8_t i = 0; i < size; ++i) slot.name[i] = name[i];
  }

public:
  explicit Registry(AgentSink& sink) : sink_(sink) {}

  [[nodiscard]] Result<uint8_t, Status>
  registerAgent(uint32_t key, const uint8_t* name, uint8_t size) {
    for (uint8_t id = 0; id < slots_.size(); ++id) {
      Slot& slot = slots_[id];
      if (slot.used && slot.key == key) {
        slot.active = true;
        setName(slot, name, size);
        sink_.onRegistered(id, name, size);
        return Ok(id);
      }
    }
    for (uint8_t id = 0; id < slots_.size(); ++id) {
      Slot& slot = slots_[id];
      if (!slot.used) {
        slot.used = true;
        slot.active = true;
        slot.key = key;
        slot.state = AgentState::Idle;
        setName(slot, name, size);
        sink_.onRegistered(id, name, size);
        return Ok(id);
      }
    }
    return Err(Status::NoFreeSlot);
  }

  [[nodiscard]] Status setState(uint8_t id, AgentState state) {
    if (id >= slots_.size() || !slots_[id].used || !slots_[id].active)
      return Status::UnknownAgent;
    slots_[id].state = state;
    sink_.onState(id, state);
    return Status::Ok;
  }

  [[nodiscard]] Status setText(uint8_t id, const uint8_t* text, uint8_t size) {
    if (id >= slots_.size() || !slots_[id].used || !slots_[id].active)
      return Status::UnknownAgent;
    sink_.onText(id, text, size);
    return Status::Ok;
  }

  [[nodiscard]] Status unregisterAgent(uint8_t id) {
    if (id >= slots_.size() || !slots_[id].used)
      return Status::UnknownAgent;
    slots_[id] = Slot{};
    sink_.onRemoved(id);
    return Status::Ok;
  }

  void beginLease(uint32_t now) {
    for (Slot& slot : slots_) {
      if (slot.used && slot.active) {
        slot.active = false;
        slot.leaseStarted = now;
      }
    }
  }

  void reap(uint32_t now) {
    for (uint8_t id = 0; id < slots_.size(); ++id) {
      Slot& slot = slots_[id];
      if (slot.used && !slot.active && elapsed(now, slot.leaseStarted, kLeaseMs)) {
        slot = Slot{};
        sink_.onRemoved(id);
      }
    }
  }
};

} // namespace agent_panel
```

`AgentSink` 是 BLE 和渲染的防火墙。生产实现可以把这些回调投递到 FreeRTOS queue；OLED 与 WS2812 消费事件，却不需要知道 opcode、UUID 或 ATT。

== GATT 适配层

```cpp
// AgentPanelGatt.h
#pragma once

#include <BLECharacteristic.h>
#include <BLEServer.h>
#include "AgentRegistry.h"

namespace agent_panel {

class AgentPanelGatt final : private BLEServerCallbacks,
                             private BLECharacteristicCallbacks {
  Registry registry_;
  BLEServer* server_{};
  BLECharacteristic* eventTx_{};
  bool connected_{};

  void onConnect(BLEServer*) override { connected_ = true; }
  void onDisconnect(BLEServer*) override;
  void onWrite(BLECharacteristic* characteristic) override;
  void send(uint8_t opcode, Status status, uint8_t id = kNoAgent);

public:
  explicit AgentPanelGatt(AgentSink& sink) : registry_(sink) {}
  void begin();
  void poll(uint32_t now) { registry_.reap(now); }
};

} // namespace agent_panel
```

```cpp
// AgentPanelGatt.cpp
#include "AgentPanelGatt.h"

#include <Arduino.h>
#include <BLE2902.h>
#include <BLEAdvertising.h>
#include <BLEDevice.h>
#include <BLESecurity.h>

namespace agent_panel {

void AgentPanelGatt::begin() {
  BLEDevice::init("PRTN-AgentPanel");
  BLESecurity::setCapability(ESP_IO_CAP_NONE);
  BLESecurity::setAuthenticationMode(true, false, true);

  server_ = BLEDevice::createServer();
  server_->setCallbacks(this);
  server_->advertiseOnDisconnect(true);
  BLEService* service = server_->createService(kServiceUuid);

  auto* info = service->createCharacteristic(
      kInfoUuid, BLECharacteristic::PROPERTY_READ);
  auto* commandRx = service->createCharacteristic(
      kCommandUuid, BLECharacteristic::PROPERTY_WRITE);
  eventTx_ = service->createCharacteristic(
      kEventUuid, BLECharacteristic::PROPERTY_INDICATE);

  info->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
  commandRx->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);
  eventTx_->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
  info->setValue(kProtocolInfo.data(), kProtocolInfo.size());
  commandRx->setCallbacks(this);
  eventTx_->addDescriptor(new BLE2902());

  service->start();
  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(kServiceUuid);
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();
}

void AgentPanelGatt::onDisconnect(BLEServer*) {
  connected_ = false;
  registry_.beginLease(millis());
  // advertiseOnDisconnect(true) 已要求 server 恢复广播。
}

void AgentPanelGatt::send(uint8_t opcode, Status status, uint8_t id) {
  if (!connected_) return;
  const auto frame = resultFrame(opcode, status, id);
  eventTx_->setValue(frame.data(), frame.size());
  eventTx_->indicate();
}

void AgentPanelGatt::onWrite(BLECharacteristic* characteristic) {
  const uint8_t* p = characteristic->getData();
  const std::size_t n = characteristic->getLength();
  const uint8_t hintedOpcode = n == 0 ? 0 : p[0];
  const auto parsed = decode(p, n);
  if (parsed.is_err()) {
    send(hintedOpcode, parsed.error());
    return;
  }

  const Command command = parsed.value();
  switch (command.op) {
    case Op::Register: {
      auto result = registry_.registerAgent(
          command.key, command.bytes.data(), command.size);
      if (result.is_ok())
        send(static_cast<uint8_t>(command.op), Status::Ok, result.value());
      else send(static_cast<uint8_t>(command.op), result.error());
      return;
    }
    case Op::SetState: {
      const Status status = registry_.setState(command.agentId, command.state);
      send(static_cast<uint8_t>(command.op), status, command.agentId);
      return;
    }
    case Op::SetText: {
      const Status status = registry_.setText(
          command.agentId, command.bytes.data(), command.size);
      send(static_cast<uint8_t>(command.op), status, command.agentId);
      return;
    }
    case Op::Unregister: {
      const Status status = registry_.unregisterAgent(command.agentId);
      send(static_cast<uint8_t>(command.op), status, command.agentId);
      return;
    }
  }
}

} // namespace agent_panel
```

在应用入口中只需要构造一个实现 `AgentSink` 的渲染桥，并周期性执行 `poll(millis())`。切勿把 BLE 对象的生命周期藏在临时变量里。

== 关于同步 Indication 的现实边界

Arduino-ESP32 的 `BLECharacteristic::indicate()` 会等待客户端 confirmation，库内有超时。示例同步调用便于展示“一写一结果”，但更稳妥的产品结构是：

1. `onWrite` 只解析并把 `Command` 放进定长队列；
2. Agent Panel task 更新 registry 与渲染事件；
3. BLE TX task 从结果队列调用 `indicate()`；
4. 队列满时返回 BUSY，绝不无限阻塞或动态扩容。

这个优化不改变 wire protocol，可以在硬件联调后再做。

= Rust broker：把 BLE 变成本地服务

== broker 的三层职责

#table(
  columns: (1.2fr, 2fr, 2fr),
  table.header([*层*], [*输入/输出*], [*不应该知道*]),
  [PanelClient], [typed command ↔ BLE bytes], [TCP、JSON、agent 进程。],
  [Broker], [本地 agent 生命周期 ↔ PanelClient], [具体 BLE adapter API。],
  [IPC], [NDJSON ↔ broker request], [UUID、ATT、slot registry 实现。],
)

依赖建议：

```toml
[package]
name = "prtn-agent-broker"
version = "0.1.0"
edition = "2024"

[dependencies]
anyhow = "1"
btleplug = "0.12"
futures-util = "0.3"
rand = "0.9"
serde = { version = "1", features = ["derive"] }
serde_json = "1"
thiserror = "2"
tokio = { version = "1", features = ["full"] }
uuid = "1"
```

== 线上的 Rust 类型与编码

```rust
// protocol.rs
use thiserror::Error;

pub const SERVICE: uuid::Uuid = uuid::uuid!("6e291bc2-80c8-484d-a505-49509c3c868e");
pub const INFO: uuid::Uuid = uuid::uuid!("c2ef6547-91b4-4367-8b1c-0514c5bb8f42");
pub const COMMAND: uuid::Uuid = uuid::uuid!("b6c66dd2-4047-473c-93f3-d97d9405330c");
pub const EVENT: uuid::Uuid = uuid::uuid!("1c69273f-d0fb-447a-90c1-0c472a0b7b53");

#[derive(Clone, Copy, Debug, serde::Deserialize)]
#[serde(rename_all = "snake_case")]
#[repr(u8)]
pub enum AgentState {
    Off = 0, Idle, Working, WaitPermission, WaitOption, Done, Error,
}

#[derive(Clone, Debug)]
pub enum Command {
    Register { key: u32, name: String },
    SetState { id: u8, state: AgentState },
    SetText { id: u8, text: String },
    Unregister { id: u8 },
}

impl Command {
    pub fn opcode(&self) -> u8 {
        match self {
            Self::Register { .. } => 0x01,
            Self::SetState { .. } => 0x02,
            Self::SetText { .. } => 0x03,
            Self::Unregister { .. } => 0x04,
        }
    }

    pub fn encode(&self) -> Result<Vec<u8>, ProtocolError> {
        let mut out = Vec::with_capacity(20);
        out.push(self.opcode());
        match self {
            Self::Register { key, name } => {
                if *key == 0 || name.is_empty() || name.len() > 15 {
                    return Err(ProtocolError::InvalidInput("register"));
                }
                out.extend_from_slice(&key.to_le_bytes());
                out.extend_from_slice(name.as_bytes());
            }
            Self::SetState { id, state } => out.extend([*id, *state as u8]),
            Self::SetText { id, text } => {
                if text.len() > 18 { return Err(ProtocolError::InvalidInput("text")); }
                out.push(*id);
                out.extend_from_slice(text.as_bytes());
            }
            Self::Unregister { id } => out.push(*id),
        }
        Ok(out)
    }
}

#[derive(Debug)]
pub struct CommandResult {
    pub opcode: u8,
    pub status: u8,
    pub agent_id: Option<u8>,
}

impl CommandResult {
    pub fn decode(bytes: &[u8]) -> Result<Self, ProtocolError> {
        let [0x80, opcode, status, id] = bytes else {
            return Err(ProtocolError::MalformedResult);
        };
        Ok(Self {
            opcode: *opcode,
            status: *status,
            agent_id: (*id != 0xFF).then_some(*id),
        })
    }
}

#[derive(Debug, Error)]
pub enum ProtocolError {
    #[error("invalid input for {0}")]
    InvalidInput(&'static str),
    #[error("malformed command result")]
    MalformedResult,
    #[error("panel rejected opcode {opcode:#04x} with status {status}")]
    Rejected { opcode: u8, status: u8 },
}
```

这里的 `String::len()` 返回 UTF-8 字节数，正好对应 wire limit。输入来自 serde，已经保证自身是合法 UTF-8。

== PanelClient：唯一 BLE 所有者

```rust
// panel.rs
use std::time::Duration;

use anyhow::{Context, Result, bail};
use btleplug::{
    api::{Central, CharPropFlags, Manager as _, Peripheral as _, ScanFilter, WriteType},
    platform::{Adapter, Manager, Peripheral},
};
use futures_util::StreamExt;
use tokio::time::{sleep, timeout};

use crate::protocol::{self, Command, CommandResult, ProtocolError};

pub struct PanelClient {
    peripheral: Peripheral,
    command: btleplug::api::Characteristic,
    events: std::pin::Pin<Box<dyn futures_util::Stream<
        Item = btleplug::api::ValueNotification> + Send>>,
}

impl PanelClient {
    async fn adapter() -> Result<Adapter> {
        Manager::new().await?.adapters().await?
            .into_iter().next().context("no BLE adapter")
    }

    pub async fn connect() -> Result<Self> {
        let adapter = Self::adapter().await?;
        adapter.start_scan(ScanFilter { services: vec![protocol::SERVICE] }).await?;

        let peripheral = timeout(Duration::from_secs(15), async {
            loop {
                for p in adapter.peripherals().await? {
                    if p.properties().await?.is_some_and(|x|
                        x.services.contains(&protocol::SERVICE)) {
                        return Ok::<_, anyhow::Error>(p);
                    }
                }
                sleep(Duration::from_millis(250)).await;
            }
        }).await.context("scan timeout")??;

        peripheral.connect().await?;
        peripheral.discover_services().await?;
        let chars = peripheral.characteristics();
        let info = chars.iter().find(|c| c.uuid == protocol::INFO)
            .cloned().context("Protocol Info missing")?;
        let command = chars.iter().find(|c| c.uuid == protocol::COMMAND)
            .cloned().context("Command RX missing")?;
        let event = chars.iter().find(|c| c.uuid == protocol::EVENT)
            .cloned().context("Event TX missing")?;

        if !command.properties.contains(CharPropFlags::WRITE) ||
           !event.properties.contains(CharPropFlags::INDICATE) {
            bail!("unexpected characteristic properties");
        }

        // 先订阅，再允许发命令，消除“结果比订阅更早”这一竞态。
        peripheral.subscribe(&event).await?;
        let events = peripheral.notifications().await?;
        let bytes = peripheral.read(&info).await?;
        validate_info(&bytes)?;
        Ok(Self { peripheral, command, events })
    }

    pub async fn execute(&mut self, command: Command) -> Result<CommandResult> {
        let opcode = command.opcode();
        let bytes = command.encode()?;
        self.peripheral.write(&self.command, &bytes, WriteType::WithResponse).await?;

        let result = timeout(Duration::from_secs(2), async {
            while let Some(notification) = self.events.next().await {
                if notification.uuid != protocol::EVENT { continue; }
                let result = CommandResult::decode(&notification.value)?;
                if result.opcode == opcode { return Ok::<_, anyhow::Error>(result); }
            }
            bail!("notification stream ended")
        }).await.context("application result timeout")??;

        if result.status != 0 {
            return Err(ProtocolError::Rejected {
                opcode: result.opcode, status: result.status,
            }.into());
        }
        Ok(result)
    }
}

fn validate_info(b: &[u8]) -> Result<()> {
    if b.len() != 8 { bail!("Protocol Info length is {}", b.len()); }
    if b[0] != 1 { bail!("unsupported protocol major {}", b[0]); }
    if b[2] < 7 || b[3] < 15 || b[4] < 18 || b[5] & 0x0F != 0x0F {
        bail!("panel capabilities do not satisfy v1");
    }
    Ok(())
}
```

#warning[
  上面的 `PanelClient` 必须由单一 broker task 独占。不要把它塞进一个到处 clone 的 mutex 后再允许任意调用方并发等待通知；那会产生“谁拿走了谁的结果”的竞态。
]

== 用 actor 串行化所有命令

```rust
// broker.rs
use std::collections::HashMap;

use anyhow::{Context, Result};
use tokio::sync::{mpsc, oneshot};

use crate::{
    panel::PanelClient,
    protocol::{AgentState, Command, CommandResult, ProtocolError},
};

// IPC 永远使用稳定的 key；会因 ESP 重启而变化的 slot id 只留在 broker 内部。
#[derive(Clone)]
pub enum Request {
    Register { key: u32, name: String },
    SetState { key: u32, state: AgentState },
    SetText { key: u32, text: String },
    Unregister { key: u32 },
}

struct Session {
    name: String,
    panel_id: u8,
}

struct Envelope {
    request: Request,
    reply: oneshot::Sender<Result<CommandResult>>,
}

#[derive(Clone)]
pub struct BrokerHandle(mpsc::Sender<Envelope>);

impl BrokerHandle {
    pub async fn call(&self, request: Request) -> Result<CommandResult> {
        let (reply, receive) = oneshot::channel();
        self.0.send(Envelope { request, reply }).await?;
        receive.await?
    }
}

pub fn spawn(mut panel: PanelClient) -> BrokerHandle {
    let (tx, mut rx) = mpsc::channel::<Envelope>(32);
    tokio::spawn(async move {
        let mut sessions = HashMap::<u32, Session>::new();
        while let Some(envelope) = rx.recv().await {
            let request = envelope.request;
            let mut answer = dispatch(&mut panel, &mut sessions, &request).await;

            // ProtocolError 是确定的输入/业务拒绝；transport error 才触发恢复。
            let transport_failed = answer.as_ref().err()
                .is_some_and(|e| e.downcast_ref::<ProtocolError>().is_none());
            if transport_failed {
                answer = async {
                    panel = reconnect_and_restore(&mut sessions).await?;
                    dispatch(&mut panel, &mut sessions, &request).await
                }.await;
            }
            let _ = envelope.reply.send(answer);
        }
    });
    BrokerHandle(tx)
}

async fn dispatch(
    panel: &mut PanelClient,
    sessions: &mut HashMap<u32, Session>,
    request: &Request,
) -> Result<CommandResult> {
    match request {
        Request::Register { key, name } => {
            let result = panel.execute(Command::Register {
                key: *key, name: name.clone(),
            }).await?;
            let panel_id = require_id(&result)?;
            sessions.insert(*key, Session { name: name.clone(), panel_id });
            Ok(result)
        }
        Request::SetState { key, state } => {
            let id = session_id(sessions, *key)?;
            panel.execute(Command::SetState { id, state: *state }).await
        }
        Request::SetText { key, text } => {
            let id = session_id(sessions, *key)?;
            panel.execute(Command::SetText { id, text: text.clone() }).await
        }
        Request::Unregister { key } => {
            let id = session_id(sessions, *key)?;
            let result = panel.execute(Command::Unregister { id }).await?;
            sessions.remove(key);
            Ok(result)
        }
    }
}

async fn reconnect_and_restore(
    sessions: &mut HashMap<u32, Session>,
) -> Result<PanelClient> {
    let mut panel = PanelClient::connect().await.context("BLE reconnect failed")?;
    // 顺序不重要；每次结果都会刷新 key -> panel_id 映射。
    for (key, session) in sessions.iter_mut() {
        let result = panel.execute(Command::Register {
            key: *key, name: session.name.clone(),
        }).await?;
        session.panel_id = require_id(&result)?;
    }
    Ok(panel)
}

fn session_id(sessions: &HashMap<u32, Session>, key: u32) -> Result<u8> {
    sessions.get(&key).map(|s| s.panel_id).context("unknown local session")
}

fn require_id(result: &CommandResult) -> Result<u8> {
    result.agent_id.context("panel returned no agent id")
}
```

这个 actor 同时解决两件事：mpsc receiver 保证 BLE 命令全局串行；`key -> panel_id` 映射隔离了会变化的 slot id。transport 失败时，它重连、重新 REGISTER 所有仍存活的 session、刷新映射，然后重试当前命令。本协议四个操作都是幂等或“赋值式”的，因此允许这一次有界重放；绝不能把这个结论无条件套到未来的“追加消息”命令。

== 本地 TCP + NDJSON 接口

默认监听 `127.0.0.1:47671`。一条 TCP 连接就是一名 agent；第一条消息必须是 register，之后无需重复 id。客户端不能流水线发送，必须读完当前响应再发下一条。

```json
{"op":"register","name":"planner"}
{"ok":true,"agent_id":0}
{"op":"set_state","state":"working"}
{"ok":true}
{"op":"set_text","text":"正在检索"}
{"ok":true}
{"op":"unregister"}
{"ok":true}
```

错误统一为：

```json
{"ok":false,"error":"invalid_text","message":"text exceeds 18 UTF-8 bytes"}
```

IPC 类型与连接处理：

```rust
// ipc.rs
use anyhow::{Context, Result};
use rand::Rng as _;
use serde::{Deserialize, Serialize};
use tokio::{
    io::{AsyncBufReadExt, AsyncWriteExt, BufReader},
    net::{TcpListener, TcpStream},
};

use crate::{
    broker::{self, BrokerHandle, Request},
    protocol::AgentState,
};

#[derive(Deserialize)]
#[serde(tag = "op", rename_all = "snake_case")]
enum Input {
    Register { name: String },
    SetState { state: AgentState },
    SetText { text: String },
    Unregister,
}

#[derive(Serialize)]
struct Output<'a> {
    ok: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    agent_id: Option<u8>,
    #[serde(skip_serializing_if = "Option::is_none")]
    error: Option<&'a str>,
    #[serde(skip_serializing_if = "Option::is_none")]
    message: Option<String>,
}

pub async fn serve(broker: BrokerHandle) -> Result<()> {
    let listener = TcpListener::bind("127.0.0.1:47671").await?;
    loop {
        let (stream, _) = listener.accept().await?;
        let broker = broker.clone();
        tokio::spawn(async move {
            if let Err(error) = session(stream, broker).await {
                eprintln!("agent session ended: {error:#}");
            }
        });
    }
}

async fn session(stream: TcpStream, broker: BrokerHandle) -> Result<()> {
    let (read, mut write) = stream.into_split();
    let mut lines = BufReader::new(read).lines();
    let mut registered = false;
    let mut key = rand::rng().random::<u32>();
    if key == 0 { key = 1; }

    while let Some(line) = lines.next_line().await? {
        let input: Input = serde_json::from_str(&line).context("invalid JSON")?;
        let response = match (registered, input) {
            (false, Input::Register { name }) => {
                match broker.call(Request::Register { key, name }).await {
                    Ok(result) => {
                        registered = true;
                        Output { ok: true, agent_id: result.agent_id,
                            error: None, message: None }
                    }
                    Err(e) => failure("register_failed", e),
                }
            }
            (false, _) => failure_text("not_registered", "first request must be register"),
            (true, Input::SetState { state }) => {
                simple(broker.call(Request::SetState { key, state }).await)
            }
            (true, Input::SetText { text }) => {
                simple(broker.call(Request::SetText { key, text }).await)
            }
            (true, Input::Unregister) => {
                let out = simple(broker.call(Request::Unregister { key }).await);
                if out.ok { registered = false; }
                out
            }
            (true, Input::Register { .. }) =>
                failure_text("already_registered", "connection already owns an agent"),
        };
        write.write_all(serde_json::to_string(&response)?.as_bytes()).await?;
        write.write_all(b"\n").await?;
    }

    if registered {
        let _ = broker.call(Request::Unregister { key }).await;
    }
    Ok(())
}

fn simple(result: Result<crate::protocol::CommandResult>) -> Output<'static> {
    match result {
        Ok(_) => Output { ok: true, agent_id: None, error: None, message: None },
        Err(e) => failure("panel_error", e),
    }
}

fn failure(code: &'static str, e: anyhow::Error) -> Output<'static> {
    failure_text(code, e.to_string())
}

fn failure_text(code: &'static str, message: impl Into<String>) -> Output<'static> {
    Output { ok: false, agent_id: None, error: Some(code), message: Some(message.into()) }
}
```

#warning[
  示例把 socket 断开解释为立即 UNREGISTER。这与 BLE 断线 lease 不冲突：前者表示本地 agent 生命周期真的结束；后者表示 broker 与面板之间的共享 transport 暂时坏了。
]

== main 与最小客户端

```rust
// main.rs
mod broker;
mod ipc;
mod panel;
mod protocol;

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let panel = panel::PanelClient::connect().await?;
    let broker = broker::spawn(panel);
    ipc::serve(broker).await
}
```

任何语言都可以使用本地接口。下面的 Python 只用于人工联调：

```python
import json, socket

with socket.create_connection(("127.0.0.1", 47671)) as s:
    f = s.makefile("rw", encoding="utf-8", newline="\n")
    for request in [
        {"op": "register", "name": "编程"},
        {"op": "set_state", "state": "working"},
        {"op": "set_text", "text": "编译中"},
        {"op": "set_state", "state": "done"},
    ]:
        f.write(json.dumps(request, ensure_ascii=False) + "\n")
        f.flush()
        print(json.loads(f.readline()))
```

= 错误模型：不要把所有失败揉成一个 bool

== 四层错误

#table(
  columns: (1.2fr, 1.8fr, 2.2fr),
  table.header([*层*], [*例子*], [*处理者*]),
  [系统/适配器], [无蓝牙权限、adapter 关闭、未配对], [启动诊断；提示用户走 OS 设置。],
  [BLE/GATT transport], [扫描超时、断线、subscribe 失败], [PanelClient 重连与退避。],
  [应用协议], [INVALID_UTF8、NO_FREE_SLOT], [broker 映射为稳定错误码，通常不重试。],
  [本地 IPC], [JSON 语法错、未注册先 set_state], [只关闭或拒绝对应 TCP session。],
)

错误边界决定重试策略。`NO_FREE_SLOT` 重连一百次也不会好；瞬时断线却不应直接让所有 agent 失败。Rust 中保留分层 error enum/context，最终在 NDJSON 边界再映射为面向 agent 的稳定 code。

== 超时不是一个数字

建议分别定义：

- scan timeout：15 秒；
- connect/discovery timeout：10 秒；
- application result timeout：2 秒；
- reconnect backoff：250ms 起步，指数增加至 8 秒并加 jitter；
- lease：协议声明的 30 秒。

它们属于不同状态，不能用一个“万能 timeout”贯穿整个程序。broker 重连超过 lease 后仍可 REGISTER，只是 slot id 可能改变；因此本地 session 不能把 id 当永久身份。

= 调试与验证手册

== 从外到内排查

1. 系统能否扫描到 `PRTN-AgentPanel`？若不能，先查广播与权限。
2. 广播里是否出现 Service UUID？名称可能只在 Scan Response。
3. 是否能连接并完成系统配对？加密权限会触发安全流程。
4. discovery 后是否有三个 UUID，性质是否正确？
5. 是否先成功 subscribe Event TX？
6. Protocol Info 是否严格为 8 字节且 major 为 1？
7. Write With Response 是否成功？
8. 是否在 2 秒内收到匹配 opcode 的 4 字节 Indication？
9. status 非零时按应用错误处理，不要重新归咎于“蓝牙不稳定”。

== 黄金测试向量

#table(
  columns: (1.25fr, 2.5fr, 1.6fr),
  table.header([*场景*], [*Command RX 十六进制*], [*预期 Event TX*]),
  [注册 `AI`，key=1], [`01 01 00 00 00 41 49`], [`80 01 00 00`],
  [slot 0 → WORKING], [`02 00 02`], [`80 02 00 00`],
  [slot 0 文本清空], [`03 00`], [`80 03 00 00`],
  [slot 0 注销], [`04 00`], [`80 04 00 00`],
  [非法状态 7], [`02 00 07`], [`80 02 06 FF`],
  [未知 opcode], [`7F`], [`80 7F 01 FF`],
  [截断 UTF-8], [`03 00 E4 B8`], [`80 03 03 FF`],
)

最后几项说明了为什么 GATT 适配层在 decode 前单独保存原始 `p[0]`：错误结果也必须回显未知 opcode，不能先假设它属于合法的 `Op` enum。空帧没有 opcode，因此回显 0。

#practice[
  在接 OLED/灯之前，用一个 `AgentSink` 将所有事件打印到 Serial。依次跑完黄金向量、7 槽占满、重复 key、BLE 断线 20 秒重连、断线 35 秒重连。等 registry 确定正确，再接渲染层。
]

== 必测状态与边界

#table(
  columns: (1.45fr, 3.6fr),
  table.header([*类别*], [*用例*]),
  [parser], [空帧、21 字节、每种合法长度、overlong UTF-8、surrogate、截断 sequence、U+10FFFF 边界。],
  [registry], [最低空闲 id、相同 key 幂等、不同 key 占满 7 槽、正常注销、未知 id。],
  [lease], [29.999 秒保留、30 秒释放、`millis()` wrap-around、lease 内 reclaim。],
  [transport], [未 subscribe、Indication timeout、断线发生在 Write Response 之后。],
  [broker], [多个 TCP session 并发但 BLE 单通道串行、broker 重启、ESP 重启。],
  [security], [未 bond 访问加密 Characteristic、已 bond 自动重连、本地清 bond 后重新配对。],
)

== 平台差异的正确位置

Linux 可能需要 BlueZ 与相应权限；macOS 首次使用会弹出蓝牙授权；Windows 的配对与设备缓存有自己的 UI。不要把这些差异渗透进协议层。让 `btleplug` adapter 层返回带上下文的诊断，README/日志告诉用户去哪里授权即可。

= 版本演进原则

== 什么变化需要 major

以下变化不向后兼容，应提升 major：改变任一现有帧字段含义、允许并发并加入 transaction id、改变大小端、删除 Characteristic、把结果从 Indicate 改成无确认 Notify。

以下通常可以提升 minor 并用 feature bit 协商：新增 opcode、新增可选状态、增加只在新 feature 下使用的 Characteristic。旧 broker 看见 major=1 且未知 feature 时，只使用自己理解的子集。

== 不要把 MTU 升级当作协议升级

未来协商 MTU 允许更长文本时，应定义显式分片协议：message id、chunk index、total、取消和超时。仅仅“发现 MTU 大了就多塞一些字节”会让相同 protocol version 在不同平台产生不同能力，难以复现。v1 保持每帧不超过 20 字节。

= 交付前检查表

- [ ] Advertising 包含 Service UUID，设备名位于 Scan Response。
- [ ] 三个 Characteristic 的 UUID、properties 与 encrypted permissions 正确。
- [ ] 系统完成 Just Works bond；远程没有清 bond 命令。
- [ ] broker 先 subscribe、再 read info、最后开放 IPC。
- [ ] 所有命令小于等于 20 字节，字符串按 UTF-8 字节验证。
- [ ] 一条 BLE 命令对应一个 Indication；全局只有一个 outstanding command。
- [ ] BLE 断线进入 30 秒 lease，本地 socket 断开立即注销。
- [ ] OLED/WS2812 不出现在 GATT callback 的耗时路径。
- [ ] 7 槽、重复 key、ESP/broker 重启与所有错误码均有测试。

= 延伸阅读与事实来源

本教程的协议布局是 PRTN 的设计选择；BLE 机制与库行为应回到官方资料核对：

- #link("https://www.bluetooth.com/bluetooth-le-primer/")[Bluetooth SIG：Bluetooth LE Primer] —— 从物理层到 GATT 的整体导览。
- #link("https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-61/out/en/host/generic-attribute-profile--gatt-.html")[Bluetooth Core Specification：GATT] —— GATT procedure、Client/Server 与可靠性语义。
- #link("https://www.bluetooth.com/specifications/assigned-numbers/")[Bluetooth SIG Assigned Numbers] —— 标准 UUID 与分配值的权威入口。
- #link("https://docs.espressif.com/projects/esp-idf/en/release-v5.0/esp32/api-guides/ble/get-started/ble-connection.html")[Espressif：BLE Connection] —— 连接与默认 ATT MTU 示例。
- #link("https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/ble/get-started/ble-data-exchange.html")[Espressif：BLE Data Exchange] —— GATT 数据交换流程。
- #link("https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/ble/index.html")[Espressif：ESP32-S3 BLE Overview] —— ESP32-S3 BLE 栈入口。
- #link("https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/bluetooth/esp_gap_ble.html")[Espressif：GAP BLE API] —— 安全参数、广播与 GAP 事件。
- #link("https://docs.rs/crate/btleplug/latest")[docs.rs：btleplug 0.12] —— 跨平台 Rust BLE GATT Client。
- #link("https://docs.rs/btleplug/latest/btleplug/api/trait.Peripheral.html")[btleplug Peripheral trait] —— connect、discover、read、write、subscribe 与 notifications API。

#v(12pt)
#align(center)[
  #block(width: 120mm, fill: navy, radius: 5pt, inset: 12pt)[
    #align(center, text(fill: white, weight: "bold")[
      先让每一层只承担一种责任，漂亮的代码会自然出现。
    ])
  ]
]
