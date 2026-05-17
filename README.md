# PRTSN

- **PRTSN = PRTS Node = Personal Rhodesisland Terminal Service Node**

This is a layered Arduino CLI project for the **AirM2M CORE ESP32-C3** board.

The goal is to keep hardware capabilities, concrete devices, services, algorithms, and task orchestration separated enough that the project can grow without turning every module into a dependency hub.

## Board

FQBN:

```text
esp32:esp32:AirM2M_CORE_ESP32C3
```

## Structure

```text
prtn/
├── prtn.ino
├── Makefile
├── README.md
└── src/
    ├── ctl/
    │   ├── inc/NodeController.h
    │   ├── inc/HeartbeatController.h
    │   ├── src/NodeController.cpp
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
    │   ├── inc/Algorithm.h
    │   └── README.md
    └── cfg/
        ├── BoardConfig.h
        └── BuildConfig.h
```

## Layer idea

```text
ctl       -> task ownership, scheduling, orchestration, state transitions
dom       -> pure data and node state types
dvc       -> concrete devices such as LEDs and serial console
fw        -> low-level GPIO/PWM/I2C/CAN/I2S/UART/timer adapters
svc       -> WiFi, MQTT, OTA, telemetry, time sync and other system services
alg       -> Arduino-free numeric/control algorithms, future Eigen-backed code
cfg       -> board/build constants
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
