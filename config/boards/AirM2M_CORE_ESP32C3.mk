# AirM2M CORE ESP32-C3 board/build configuration.

BOARD_NAME    ?= AirM2M CORE ESP32-C3
BOARD_PACKAGE ?= esp32
BOARD_ARCH    ?= esp32
BOARD_ID      ?= AirM2M_CORE_ESP32C3
BOARD_SEARCH  ?= AirM2M

# Use `arduino-cli board details -b esp32:esp32:AirM2M_CORE_ESP32C3`
# to see all values supported by the installed ESP32 core.
UPLOAD_SPEED     ?= 921600
CDC_ON_BOOT      ?= default
CPU_FREQ         ?= 160
FLASH_FREQ       ?= 80
PARTITION_SCHEME ?= no_ota
DEBUG_LEVEL      ?= none
ERASE_FLASH      ?= none

BOARD_FQBN_OPTIONS ?= UploadSpeed=$(UPLOAD_SPEED),CDCOnBoot=$(CDC_ON_BOOT),CPUFreq=$(CPU_FREQ),FlashFreq=$(FLASH_FREQ),PartitionScheme=$(PARTITION_SCHEME),DebugLevel=$(DEBUG_LEVEL),EraseFlash=$(ERASE_FLASH)

GDB_TARGET_ARCH  ?= riscv32-esp-elf
OPENOCD_BOARD_CFG ?= board/esp32c3-builtin.cfg

BUILD_DEFINES += -DPRTN_BOARD_NAME=\"AirM2M_CORE_ESP32-C3\"
BUILD_DEFINES += -DPRTN_BOARD_ID_AIRM2M_CORE_ESP32C3=1
BUILD_DEFINES += -DPRTN_BOARD_CHIP_ESP32C3=1
