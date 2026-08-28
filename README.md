# PRTSN

- **PRTSN = PRTS Node = Personal Rhodesisland Terminal Service Node**

This is a layered Arduino CLI project for the **AirM2M CORE ESP32-C3** board.

The goal is to keep hardware capabilities, concrete devices, services, algorithms, and app orchestration separated enough that the project can grow without turning every module into a dependency hub.

## Board

FQBN:

```text
esp32:esp32:AirM2M_CORE_ESP32C3
```

## Structure

```text
prtn/
├── PRTSN.ino
├── Makefile
├── README.md
├── config/
│   ├── project.mk
│   └── local.mk
└── src/
    ├── app/
    │   ├── inc/PrtnApp.h
    │   └── src/PrtnApp.cpp
    ├── cfg/
    │   ├── AppConfig.h
    │   ├── BoardConfig.h
    │   ├── BuildConfig.h
    │   ├── ProfileConfig.h
    │   └── ProjectConfig.h
    ├── ctl/
    │   ├── inc/HeartbeatController.h
    │   ├── inc/EspNowEchoController.h
    │   └── src/HeartbeatController.cpp
    ├── dom/
    │   ├── NodeInfo.h
    │   └── NodeInfo.cpp
    ├── dvc/
    │   ├── inc/LED.h
    │   ├── inc/Serial.h
    │   ├── src/LED.cpp
    │   └── src/Serial.cpp
    ├── fw/
    │   └── README.md
    ├── svc/
    │   └── README.md
    ├── alg/
    │   └── README.md
```

## Layer idea

```text
app       -> static object ownership, setup order, FreeRTOS task creation
ctl       -> focused business controllers such as heartbeat or ESP-NOW echo
dom       -> pure data and node state types
dvc       -> concrete devices such as LEDs and serial console
fw        -> low-level GPIO/PWM/I2C/CAN/I2S/UART/timer adapters
svc       -> WiFi, MQTT, OTA, telemetry, time sync and other system services
alg       -> Arduino-free numeric/control algorithms, future Eigen-backed code
cfg       -> profile selection and app-level constexpr configuration
```

## Build

### Nix development shell

With Nix flakes enabled, enter the development environment with:

```bash
nix develop
```

If you use direnv, run `direnv allow` once; the checked-in `.envrc` will then
load the same shell automatically. The shell provides Arduino CLI and language
server tooling, clang/clangd, Rust/Cargo, jq, and Python with Pillow.

The ESP32 Arduino Core 3.3.8 and its cross-compilers, debugger, OpenOCD and
support tools are pinned by the flake and stored in the Nix store. No
`~/.arduino15` setup is required:

```bash
make check
make compdb
```

The shell points Arduino CLI at the immutable Nix-provided core and keeps only
disposable download/user state under the repository's ignored `.cache/`
directory.

Access to upload and monitor hardware also requires your user account to have
permission to open the relevant `/dev/ttyUSB*` or `/dev/ttyACM*` device.

Project build defaults live in:

```text
config/project.mk
```

Machine-specific overrides can be placed in:

```text
config/local.mk
```

`config/local.mk` is ignored by git. Use it for values such as `PORT`, temporary CPU frequency, debug level, or partition experiments.

Node role selection is intentionally narrow:

```make
BUILD_DEFINES += -DPRTN_NODE_PROFILE=PRTN_NODE_PROFILE_AMA10
# BUILD_DEFINES += -DPRTN_NODE_PROFILE=PRTN_NODE_PROFILE_M3
```

Most runtime defaults live in `src/cfg/AppConfig.h` or in the owning class as `static constexpr` defaults.

GPIO ownership and compile-time pin conflict checks are documented in
[docs/pin-layout.md](docs/pin-layout.md)（包含中文调用说明）.

```bash
make check
make build
```

Show the active configuration:

```bash
make info
```

Show board menu options supported by the installed ESP32 Arduino core:

```bash
make board-options
```

## Upload

```bash
make list-ports
make upload PORT=/dev/ttyACM0
```

or:

```bash
make upload PORT=/dev/ttyUSB0
```

## Monitor

```bash
make monitor PORT=/dev/ttyACM0
```
