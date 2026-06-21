# ESP32-S3-N16R8 board/build configuration.

BOARD_NAME    ?= ESP32-S3-N16R8 Dev Board
BOARD_PACKAGE ?= esp32
BOARD_ARCH    ?= esp32
BOARD_ID      ?= esp32s3
BOARD_SEARCH  ?= ESP32S3

# Use `arduino-cli board details -b esp32:esp32:esp32s3`
# to see all values supported by the installed ESP32 core.
UPLOAD_SPEED     ?= 921600
USB_MODE         ?= hwcdc
CDC_ON_BOOT      ?= default
MSC_ON_BOOT      ?= default
DFU_ON_BOOT      ?= default
UPLOAD_MODE      ?= default
CPU_FREQ         ?= 240
FLASH_MODE       ?= qio
FLASH_SIZE       ?= 16M
PARTITION_SCHEME ?= no_ota
DEBUG_LEVEL      ?= none
PSRAM            ?= opi
LOOP_CORE        ?= 1
EVENTS_CORE      ?= 1
ERASE_FLASH      ?= none
JTAG_ADAPTER     ?= default
ZIGBEE_MODE      ?= default

BOARD_FQBN_OPTIONS ?= UploadSpeed=$(UPLOAD_SPEED),USBMode=$(USB_MODE),CDCOnBoot=$(CDC_ON_BOOT),MSCOnBoot=$(MSC_ON_BOOT),DFUOnBoot=$(DFU_ON_BOOT),UploadMode=$(UPLOAD_MODE),CPUFreq=$(CPU_FREQ),FlashMode=$(FLASH_MODE),FlashSize=$(FLASH_SIZE),PartitionScheme=$(PARTITION_SCHEME),DebugLevel=$(DEBUG_LEVEL),PSRAM=$(PSRAM),LoopCore=$(LOOP_CORE),EventsCore=$(EVENTS_CORE),EraseFlash=$(ERASE_FLASH),JTAGAdapter=$(JTAG_ADAPTER),ZigbeeMode=$(ZIGBEE_MODE)

GDB_TARGET_ARCH  ?= xtensa-esp-elf
OPENOCD_BOARD_CFG ?= board/esp32s3-builtin.cfg

BUILD_DEFINES += -DPRTN_BOARD_NAME=\"ESP32-S3-N16R8_Dev_Board\"
BUILD_DEFINES += -DPRTN_BOARD_ID_ESP32S3_N16R8_DEV=1
BUILD_DEFINES += -DPRTN_BOARD_CHIP_ESP32S3=1
BUILD_DEFINES += -DPRTN_BOARD_HAS_PSRAM=1
BUILD_DEFINES += -DPRTN_BOARD_RGB_PIN=48
BUILD_DEFINES += -include freertos/FreeRTOSConfig.h -include freertos/portmacro.h
