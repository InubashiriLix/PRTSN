# Copy this file to config/local.mk for machine-specific overrides.
# config/local.mk is ignored by git.

# Serial device on this machine.
PORT := /dev/ttyACM0
BAUD := 115200

# Common board experiments:
#
# Lower power, slower runtime:
# CPU_FREQ := 80
#
# Slower flash bus, sometimes useful for unstable hardware:
# FLASH_FREQ := 40
#
# More firmware space, no OTA:
# PARTITION_SCHEME := huge_app
#
# OTA-capable default layout, smaller app partition:
# PARTITION_SCHEME := default
#
# Show ESP32 core debug logs:
# DEBUG_LEVEL := info
#
# Erase all flash before upload:
# ERASE_FLASH := all
#
# Enable IIC/Wire only when I2C devices are actually used:
# BUILD_DEFINES += -DPRTN_ENABLE_IIC=1
