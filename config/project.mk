# PRTN board/build configuration.
#
# This file is tracked by git and should describe the default project setup.
# Put machine-specific overrides such as PORT in config/local.mk.

# Arduino board identity.
BOARD_PACKAGE := esp32
BOARD_ARCH    := esp32
BOARD_ID      := AirM2M_CORE_ESP32C3

# ESP32 Arduino board menu options.
#
# Use `arduino-cli board details -b esp32:esp32:AirM2M_CORE_ESP32C3`
# to see all values supported by the installed ESP32 core.
UPLOAD_SPEED     := 921600
CDC_ON_BOOT      := default
CPU_FREQ         := 160
FLASH_FREQ       := 80
PARTITION_SCHEME := no_ota
DEBUG_LEVEL      := none
ERASE_FLASH      := none

# Project paths.
SKETCH    := .
BUILD_DIR := build

# Serial monitor defaults.
PORT ?= /dev/ttyACM0
BAUD ?= 115200

# Optional compile-time defines passed to C and C++.
# Example:
#   BUILD_DEFINES := -DPRTN_WIFI_AP_SSID=\"PRTN-DEV\" -DPRTN_DEBUG_WIFI=1
#
# Feature gates:
#   -DPRTN_ENABLE_IIC=1  Enable IIC implementation and link Arduino Wire.
#                        Leave it disabled when no I2C device is used; Wire adds
#                        code and global objects even if no IIC object is created.
BUILD_DEFINES :=

# Optional optimization override.
#
# Common optimization levels:
#   -O0     No optimization. Fast compile, easiest debugging, largest firmware.
#   -Og     Debug-friendly optimization. Good when stepping through code.
#   -Os     Optimize for firmware size. Common embedded release choice.
#   -O1     Light optimization.
#   -O2     Balanced performance optimization. May increase firmware size.
#   -O3     Aggressive performance optimization. Often not worth it on MCU firmware.
#   -Ofast  Very aggressive; may break strict C/C++ and floating-point semantics.
#
# Project recommendation:
#   default/release: BUILD_OPT_FLAGS := -Os
#   debugging      : BUILD_OPT_FLAGS := -Og
#   size pressure  : BUILD_OPT_FLAGS := -Os
#   perf experiment: BUILD_OPT_FLAGS := -O2
BUILD_OPT_FLAGS := -Os
