# PRTN BLE Agent Client

Linux/BlueZ command-line client for exercising the Agent Panel protocol and its
display/WS2812 hardware behavior.

用于测试 Agent Panel 协议，并把 Codex CLI 生命周期同步到 LCD/WS2812 的
Linux/BlueZ 客户端。

## Source layout / 源码分层

The executable is split by feature; `main.rs` only parses the CLI and dispatches
to one top-level operation.

程序按功能拆分，`main.rs` 只负责解析参数并分派顶层操作：

| Module | Responsibility / 职责 |
| --- | --- |
| `cli.rs` | CLI schema and shared runtime configuration / 命令行结构和运行配置 |
| `commands.rs` | Manual commands and the hardware demo / 人工命令与硬件测试流程 |
| `ble.rs` | BlueZ discovery, pairing, GATT transport and responses / BLE 发现、配对和 GATT 传输 |
| `protocol.rs` | Typed Agent Panel wire encoding and validation / 协议帧编码和校验 |
| `codex_hook.rs` | Codex hook JSON mapping and local IPC forwarding / Hook JSON 映射与本地 IPC |
| `daemon.rs` | Unix socket server, BLE worker and synchronization / daemon 与 BLE 同步循环 |
| `session.rs` | Per-Codex-session desired Agent state / 每个 Codex 会话的目标状态 |
| `transcript.rs` | Incremental Ctrl-C/abort transcript detection / 增量检测中断事件 |

## Quick start

Build and scan:

```sh
make ble-client
make ble-scan
```

Run the complete seven-Agent test after flashing the firmware:

```sh
make ble-demo
```

The demo registers slots `0..6`, then runs every slot through every protocol
state (a complete 7×7 matrix of 49 transitions). It updates the LCD text with
the active state name, verifies every command response, and removes all slots
before disconnecting.

Individual operations are available through the wrapper:

```sh
tools/ble-agent-client.sh register --key 1 --name codex
tools/ble-agent-client.sh state --id 0 --state working
tools/ble-agent-client.sh text --id 0 --text building
tools/ble-agent-client.sh unregister --id 0
```

Names are limited to 15 UTF-8 bytes, text to 18 UTF-8 bytes, and Agent IDs to
`0..6`. The client validates these limits before writing to BLE and waits for a
typed four-byte Event TX response after every command.

The process uses the system BlueZ D-Bus service and Just Works pairing. The
current user must be allowed to control the local Bluetooth adapter.

## Codex CLI hooks / Codex CLI 自动状态同步

The integration is installed in the user-level `~/.codex/hooks.json`, so it
applies to Codex CLI sessions in every repository. It uses lifecycle hooks
instead of a skill: a skill depends on the model deciding to call it, while a
hook runs mechanically at the corresponding CLI lifecycle event.

集成安装在用户级 `~/.codex/hooks.json`，因此所有仓库中的 Codex CLI 会话都会
生效。这里刻意使用生命周期 hook，而不是 skill：skill 需要模型主动决定调用，
hook 会在固定生命周期事件上自动执行。

Install the release binary into the stable user-level Cargo bin directory
before enabling the hooks:

启用 hook 前，将 release 客户端安装到稳定的用户级 Cargo bin 目录：

```sh
cargo install --path tools/ble-agent-client --locked --force
```

The global hook invokes `~/.cargo/bin/ble-agent-client`; cleaning this
repository's `target/` directory therefore does not break Codex integration.

全局 hook 调用 `~/.cargo/bin/ble-agent-client`，因此清理本仓库的 `target/`
目录不会破坏 Codex 集成。

Then completely restart Codex, run `/hooks`, and trust the user-level hook
definition. The first lifecycle event automatically starts a background
`ble-agent-client serve` process. Later hooks only send a small JSON event over
the local Unix socket; they do not repeatedly scan, connect, or disconnect BLE.

然后彻底重启 Codex，执行 `/hooks` 并信任用户级 hook。第一个生命周期
事件会自动启动后台 `ble-agent-client serve`；后续 hook 只通过本地 Unix socket
发送一个很小的 JSON 事件，不会反复扫描、连接和断开 BLE。

Default runtime files / 默认运行时文件：

```text
$XDG_RUNTIME_DIR/prtn-ble-agent.sock
$XDG_RUNTIME_DIR/prtn-ble-agent.log
```

If `XDG_RUNTIME_DIR` is unavailable, the client falls back to the system temp
directory. The socket is created with mode `0600`.

如果系统没有 `XDG_RUNTIME_DIR`，客户端会退回系统临时目录；socket 权限固定为
`0600`。

Lifecycle mapping / 生命周期映射：

| Codex event | Agent state | Display text |
| --- | --- | --- |
| `SessionStart` | `Idle` | `idle` |
| `UserPromptSubmit` | `Working` | `working` |
| `PermissionRequest` | `WaitPermission` | `approval` |
| `PreToolUse(request_user_input)` | `WaitOption` | `your input` |
| `PostToolUse` | `Working` | `working` |
| other `PreToolUse` | `Working` | `working` |
| `Stop` | `Done` | `done` |
| `SessionEnd` | unregister | cleared |

The daemon also follows each session's Codex transcript from the byte offset at
which it first sees that transcript. A `turn_aborted` event, including Ctrl-C
turn cancellation, immediately returns the slot to `Idle` with the text
`interrupted`. Starting at the current byte offset prevents old cancellation
events from affecting resumed sessions.

daemon 还会从首次发现 transcript 时的字节位置开始跟踪各会话。发现
`turn_aborted`（包括 Ctrl-C 取消当前 turn）后，会立即把 slot 恢复为 `Idle`，
文字显示 `interrupted`。从当前字节位置开始读取，可避免恢复旧会话时误处理历史
中断事件。

Each Codex `session_id` is deterministically converted into a nonzero 32-bit
Agent key. This lets firmware recover the same slot during its 30-second lease
when the daemon reconnects. Up to seven simultaneous Codex sessions can be
displayed because the panel protocol has seven slots.

每个 Codex `session_id` 都会稳定转换成非零 32 位 Agent key。daemon 重连时，
固件可以在 30 秒 lease 内恢复原 slot。协议只有七个 slot，因此最多同时展示七个
Codex 会话。

For foreground diagnostics, stop the automatically started daemon and run:

需要前台诊断时，可以停止自动启动的 daemon，然后运行：

```sh
tools/ble-agent-client/target/release/ble-agent-client serve
```

`hook` is intended for Codex and reads exactly one hook JSON object from stdin.
It normally has no user-visible output.
