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
├── PRTN.ino
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
    │   ├── inc/SerialConsole.h
    │   ├── src/LED.cpp
    │   └── src/SerialConsole.cpp
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
