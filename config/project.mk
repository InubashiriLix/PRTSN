# PRTN project/build configuration.
#
# This file is tracked by git and should describe the default project setup.
# Put machine-specific overrides such as PORT in config/local.mk.

# Board configuration selected by the top-level Makefile.
#
# Board files live in config/boards/*.mk. Override this in config/local.mk
# or on the command line, for example:
#   make BOARD_CONFIG=ESP32S3_N16R8_DEV info
BOARD_CONFIG ?= ESP32S3_N16R8_DEV

# Project paths.
SKETCH    := .
BUILD_DIR ?= build/$(BOARD_CONFIG)

# Serial monitor defaults.
# ESP32-S3 boards using a USB-UART bridge commonly enumerate as /dev/ttyUSB*.
# AirM2M ESP32-C3 boards may enumerate as /dev/ttyACM*.
PORT ?= /dev/ttyUSB0
BAUD ?= 115200

# Optional compile-time defines passed to C and C++.
# Examples:
#   BUILD_DEFINES := -DPRTN_NODE_PROFILE=PRTN_NODE_PROFILE_M3
#   BUILD_DEFINES += -DPRTN_NODE_PROFILE=PRTN_NODE_PROFILE_AMA10
#
# Feature gates:
#   -DPRTN_ENABLE_IIC=1          Enable the ESP-IDF driver based IIC implementation.
#   -DPRTN_ENABLE_ARDUINO_IIC=1  Enable the legacy Arduino Wire based IIC implementation.
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
