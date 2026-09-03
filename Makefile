include config/project.mk
-include config/local.mk

override BOARD_CONFIG := $(strip $(BOARD_CONFIG))
include config/boards/$(BOARD_CONFIG).mk

override BOARD_PACKAGE      := $(strip $(BOARD_PACKAGE))
override BOARD_ARCH         := $(strip $(BOARD_ARCH))
override BOARD_ID           := $(strip $(BOARD_ID))
override BOARD_SEARCH       := $(strip $(BOARD_SEARCH))
override BOARD_FQBN_OPTIONS := $(strip $(BOARD_FQBN_OPTIONS))
override SKETCH             := $(strip $(SKETCH))
override BUILD_DIR          := $(strip $(BUILD_DIR))
override COMPDB_BUILD_DIR   := $(strip $(or $(COMPDB_BUILD_DIR),$(BUILD_DIR)/compdb))
override PORT               := $(strip $(PORT))
override BAUD               := $(strip $(BAUD))
override GDB_TARGET_ARCH    := $(strip $(GDB_TARGET_ARCH))
override OPENOCD_BOARD_CFG  := $(strip $(OPENOCD_BOARD_CFG))

BASE_FQBN := $(BOARD_PACKAGE):$(BOARD_ARCH):$(BOARD_ID)
FQBN := $(BASE_FQBN)$(if $(BOARD_FQBN_OPTIONS),:$(BOARD_FQBN_OPTIONS))

BUILD_FLAGS = $(strip $(BUILD_DEFINES) $(BUILD_OPT_FLAGS))
BUILD_PROPERTIES = $(if $(BUILD_FLAGS),--build-property compiler.c.extra_flags="$(BUILD_FLAGS)" --build-property compiler.cpp.extra_flags="$(BUILD_FLAGS)")

SERIAL_TUI_DIR := tools/serial-tui
SERIAL_TUI_BIN := $(SERIAL_TUI_DIR)/target/release/serial-tui
BLE_CLIENT_DIR := tools/ble-agent-client
BLE_CLIENT_BIN := $(BLE_CLIENT_DIR)/target/release/ble-agent-client

GIF ?=
GIF_OUT ?= src/app/assets/boot-anim.h
GIF_NAME ?= LcdAnimation
GIF_WIDTH ?= 80
GIF_HEIGHT ?= 160
GIF_FPS ?= 60
GIF_FIT ?= contain

ESP32_TOOLS := $(strip $(ARDUINO_ESP32_TOOLS))
ifeq ($(ESP32_TOOLS),)
$(error ARDUINO_ESP32_TOOLS is not set; enter the development environment with 'nix develop')
endif
OPENOCD_ROOT := $(lastword $(sort $(wildcard $(ESP32_TOOLS)/openocd-esp32/*)))
OPENOCD_BIN := $(OPENOCD_ROOT)/bin/openocd
OPENOCD_SCRIPTS := $(OPENOCD_ROOT)/share/openocd/scripts
GDB_TARGET_ARCH ?= riscv32-esp-elf
GDB_ROOT := $(lastword $(sort $(wildcard $(ESP32_TOOLS)/$(GDB_TARGET_ARCH)-gdb/*)))
GDB_BIN := $(GDB_ROOT)/bin/$(GDB_TARGET_ARCH)-gdb
DEBUG_ELF := $(BUILD_DIR)/PRTSN.ino.elf
DEBUG_GDB_PORT ?= 3333

FORMAT_FILES := $(shell git ls-files '*.h' '*.hpp' '*.c' '*.cpp' '*.cc' '*.cxx' '*.ino')

RESET  := \033[0m
BOLD   := \033[1m
DIM    := \033[2m
RED    := \033[31m
GREEN  := \033[32m
YELLOW := \033[33m
CYAN   := \033[36m

.PHONY: help info board-options check compdb build debug-build debug-server debug-gdb upload monitor monitor-arduino serial-tui ble-client ble-scan ble-demo tools gif-to-rgb565 list-ports clean clean-log format format-check

define section
	@printf "\n$(BOLD)$(CYAN)==> %s$(RESET)\n" "$(1)"
endef

define ok
	@printf "$(GREEN)✓$(RESET) %s\n" "$(1)"
endef

define warn
	@printf "$(YELLOW)!$(RESET) %s\n" "$(1)"
endef

define fail
	@printf "$(RED)✗$(RESET) %s\n" "$(1)"
endef

define cmd
	@printf '$(DIM)$ %s$(RESET)\n' '$(1)'
endef

help:
	@printf "\n$(BOLD)PRTN$(RESET) - PRTS Node / PRTS 节点\n"
	@printf "$(DIM)Minimal layered Arduino CLI project for $(BOARD_NAME).$(RESET)\n"
	@printf "$(DIM)适用于 $(BOARD_NAME) 的极简分层 Arduino CLI 项目。$(RESET)\n\n"

	@printf "$(BOLD)Targets / 可用命令$(RESET)\n"
	@printf "  $(CYAN)make check$(RESET)        Check toolchain, ESP32 core, board and port\n"
	@printf "                    检查 arduino-cli、ESP32 core、板卡和串口\n"
	@printf "  $(CYAN)make compdb$(RESET)       Generate compile_commands.json for clangd\n"
	@printf "                    为 clangd 生成 compile_commands.json\n"
	@printf "  $(CYAN)make build$(RESET)        Compile firmware\n"
	@printf "                    编译固件\n"
	@printf "  $(CYAN)make debug-build$(RESET)  Compile firmware with debug-friendly flags\n"
	@printf "                    使用适合单步调试的参数编译固件\n"
	@printf "  $(CYAN)make debug-server$(RESET) Start $(BOARD_NAME) OpenOCD server\n"
	@printf "                    启动 $(BOARD_NAME) OpenOCD 调试服务器\n"
	@printf "  $(CYAN)make debug-gdb$(RESET)    Connect terminal GDB to OpenOCD\n"
	@printf "                    使用终端 GDB 连接 OpenOCD\n"
	@printf "  $(CYAN)make upload$(RESET)       Upload firmware to $(BOLD)$(PORT)$(RESET)\n"
	@printf "                    上传固件到 $(BOLD)$(PORT)$(RESET)\n"
	@printf "  $(CYAN)make monitor$(RESET)      Open serial monitor at $(BOLD)$(BAUD)$(RESET)\n"
	@printf "                    使用 Rust TUI 打开串口监视器，波特率 $(BOLD)$(BAUD)$(RESET)\n"
	@printf "  $(CYAN)make monitor-arduino$(RESET) Open Arduino CLI serial monitor\n"
	@printf "                    使用 arduino-cli 原生串口监视器\n"
	@printf "  $(CYAN)make serial-tui$(RESET)   Build Rust serial TUI tool\n"
	@printf "                    构建 Rust 串口 TUI 工具\n"
	@printf "  $(CYAN)make ble-client$(RESET)   Build the Agent Panel BLE test client\n"
	@printf "                    构建 Agent Panel BLE 测试客户端\n"
	@printf "  $(CYAN)make ble-scan$(RESET)     Scan for nearby BLE devices\n"
	@printf "                    扫描附近的 BLE 设备\n"
	@printf "  $(CYAN)make ble-demo$(RESET)     Run the seven-Agent hardware test\n"
	@printf "                    运行七个 Agent 的硬件验收测试\n"
	@printf "  $(CYAN)make tools$(RESET)        Build local development tools\n"
	@printf "                    构建本地开发工具\n"
	@printf "  $(CYAN)make gif-to-rgb565 GIF=input.gif$(RESET) Convert GIF to LCD RGB565 header\n"
	@printf "                    转换 GIF 为 LCD RGB565 头文件\n"
	@printf "  $(CYAN)make list-ports$(RESET)   List connected boards and ports\n"
	@printf "                    列出已连接的板子和串口\n"
	@printf "  $(CYAN)make info$(RESET)         Show current project configuration\n"
	@printf "                    显示当前项目配置\n"
	@printf "  $(CYAN)make board-options$(RESET) Show available board menu options\n"
	@printf "                    显示当前板卡可用配置项\n"
	@printf "  $(CYAN)make clean$(RESET)        Remove local build output\n"
	@printf "                    删除本地构建输出\n"
	@printf "  $(CYAN)make clean-log$(RESET)    Remove Neovim LSP log\n"
	@printf "                    删除 Neovim LSP 日志\n\n"
	@printf "  $(BOLD)make format$(RESET)       Format source files with clang-format\n"
	@printf "                    使用 clang-format 格式化源代码\n"
	@printf "  $(BOLD)make format-check$(RESET) Check source files format with clang-format\n"
	@printf "                    使用clang-format检查源码格式\n"


	@printf "$(BOLD)Config / 当前配置$(RESET)\n"
	@printf "  FQBN      = $(FQBN)\n"
	@printf "  Config    = config/project.mk + config/boards/$(BOARD_CONFIG).mk + optional config/local.mk\n"
	@printf "  PORT      = $(PORT)\n"
	@printf "  BAUD      = $(BAUD)\n"
	@printf "  SKETCH    = $(SKETCH)\n"
	@printf "  BUILD_DIR = $(BUILD_DIR)\n\n"

	@printf "$(BOLD)Recommended first run / 推荐首次运行$(RESET)\n"
	@printf "  make check\n"
	@printf "  make compdb\n"
	@printf "  make build\n\n"

	@printf "$(BOLD)Examples / 示例$(RESET)\n"
	@printf "  make upload\n"
	@printf "  make monitor\n"
	@printf "  make upload PORT=/dev/ttyACM0\n\n"

info:
	$(call section,Project configuration / 项目配置)
	@printf "Project / 项目      : $(BOLD)PRTN$(RESET) / PRTS Node\n"
	@printf "Board   / 板卡      : $(BOLD)$(BOARD_NAME)$(RESET)\n"
	@printf "Board config        : $(BOARD_CONFIG)\n"
	@printf "Base FQBN          : $(BASE_FQBN)\n"
	@printf "Full FQBN          : $(FQBN)\n"
	@printf "Partition / 分区   : $(PARTITION_SCHEME)\n"
	@printf "CPU freq / 主频    : $(CPU_FREQ) MHz\n"
	@printf "Flash mode/freq    : $(if $(FLASH_MODE),$(FLASH_MODE),<board default>) / $(if $(FLASH_FREQ),$(FLASH_FREQ) MHz,<board default>)\n"
	@printf "Flash size         : $(if $(FLASH_SIZE),$(FLASH_SIZE),<board default>)\n"
	@printf "PSRAM              : $(if $(PSRAM),$(PSRAM),<board default>)\n"
	@printf "USB CDC on boot    : $(CDC_ON_BOOT)\n"
	@printf "Upload speed       : $(UPLOAD_SPEED)\n"
	@printf "Debug level        : $(DEBUG_LEVEL)\n"
	@printf "Erase flash        : $(ERASE_FLASH)\n"
	@printf "Build flags        : $(if $(BUILD_FLAGS),$(BUILD_FLAGS),<arduino default>)\n"
	@printf "Port   / 串口      : $(PORT)\n"
	@printf "Baud   / 波特率    : $(BAUD)\n"
	@printf "Sketch             : $(SKETCH)\n"
	@printf "Build dir / 构建目录: $(BUILD_DIR)\n"
	@printf "Debug ELF          : $(DEBUG_ELF)\n"
	@printf "OpenOCD            : $(OPENOCD_BIN)\n"
	@printf "GDB                : $(GDB_BIN)\n"

board-options:
	$(call section,Board options / 板卡配置项)
	$(call cmd,arduino-cli board details -b $(BASE_FQBN))
	@arduino-cli board details -b $(BASE_FQBN)

check:
	$(call section,Checking toolchain / 检查工具链)
	@command -v arduino-cli >/dev/null || { \
		printf "$(RED)✗ arduino-cli not found / 未找到 arduino-cli$(RESET)\n"; \
		printf "Install / 安装:\n  paru -S arduino-cli\n"; \
		exit 1; \
	}
	$(call ok,arduino-cli found / 已找到 arduino-cli)

	@command -v clangd >/dev/null || { \
		printf "$(YELLOW)! clangd not found / 未找到 clangd，LSP 可能不可用$(RESET)\n"; \
		printf "Install / 安装:\n  sudo pacman -S clang\n"; \
		exit 0; \
	}
	$(call ok,clangd found / 已找到 clangd)

	@command -v clang-format >/dev/null || { \
		printf "$(YELLOW)! clang-format not found / 未找到 clang-format，make format 可能不可用, 同时如果启动了pre-commit 和 pre-push, 可能失败$(RESET)\n"; \
		printf "Install / 安装:\n  sudo pacman -S clang-format\n"; \
		exit 0; \
	}
	$(call ok,clang-format found / 已找到 clang-format)

	@command -v arduino-language-server >/dev/null || { \
		printf "$(YELLOW)! arduino-language-server not found / 未找到 arduino-language-server$(RESET)\n"; \
		printf "Install / 安装:\n  sudo pacman -S arduino-language-server\n"; \
		exit 0; \
	}
	$(call ok,arduino-language-server found / 已找到 arduino-language-server)

	$(call section,Checking ESP32 Arduino core / 检查 ESP32 Arduino core)
	@arduino-cli core list | grep -q "esp32:esp32" || { \
		printf "$(RED)✗ Missing core: esp32:esp32 / 缺少 ESP32 core$(RESET)\n"; \
		printf "See the Nix development shell setup in README.md.\n"; \
		printf "请参考 README.md 中的 Nix 开发环境配置。\n"; \
		exit 1; \
	}
	$(call ok,esp32:esp32 core installed / ESP32 core 已安装)

	$(call section,Checking board FQBN / 检查板卡 FQBN)
	@arduino-cli board listall | grep -q "$(BASE_FQBN)" || { \
		printf "$(RED)✗ Board not found / 未找到板卡: $(BASE_FQBN)$(RESET)\n"; \
		printf "Try / 尝试:\n  arduino-cli board listall | grep -i $(BOARD_SEARCH)\n"; \
		exit 1; \
	}
	$(call ok,board available / 板卡可用: $(BASE_FQBN))

	$(call section,Checking serial port / 检查串口)
	@if [ -e "$(PORT)" ]; then \
		printf "$(GREEN)✓$(RESET) serial port exists / 串口存在: $(PORT)\n"; \
	else \
		printf "$(YELLOW)!$(RESET) serial port not found / 未找到串口: $(PORT)\n"; \
		printf "Connected boards / 当前连接设备:\n"; \
		arduino-cli board list || true; \
	fi

compdb:
	$(call section,Generating compile_commands.json / 生成 clangd 编译数据库)
	@command -v jq >/dev/null || { \
		printf "$(RED)✗ jq not found / 未找到 jq，无法修正 Arduino 生成的源码路径$(RESET)\n"; \
		exit 1; \
	}
	$(call cmd,rm -rf $(COMPDB_BUILD_DIR))
	@rm -rf "$(COMPDB_BUILD_DIR)"
	$(call cmd,arduino-cli compile --fqbn $(FQBN) $(BUILD_PROPERTIES) --only-compilation-database --build-path $(COMPDB_BUILD_DIR) $(SKETCH))
	@arduino-cli compile --fqbn $(FQBN) $(BUILD_PROPERTIES) --only-compilation-database --build-path $(COMPDB_BUILD_DIR) $(SKETCH)
	@if [ -f "$(COMPDB_BUILD_DIR)/compile_commands.json" ]; then \
		if ! jq --arg copied_prefix "$(abspath $(COMPDB_BUILD_DIR))/sketch/src/" --arg source_prefix "$(CURDIR)/src/" 'map((if (.file | startswith($$copied_prefix)) then .file = ($$source_prefix + (.file | ltrimstr($$copied_prefix))) else . end) | .arguments = [.arguments[] | if (startswith($$copied_prefix) and (endswith(".c") or endswith(".cc") or endswith(".cpp") or endswith(".cxx"))) then $$source_prefix + ltrimstr($$copied_prefix) else . end])' "$(COMPDB_BUILD_DIR)/compile_commands.json" > "$(COMPDB_BUILD_DIR)/compile_commands.source.json"; then \
			printf "$(RED)✗$(RESET) failed to rewrite Arduino source paths / 修正 Arduino 源码路径失败\n"; \
			exit 1; \
		fi; \
		cp "$(COMPDB_BUILD_DIR)/compile_commands.source.json" ./compile_commands.json; \
		printf "$(GREEN)✓$(RESET) compile_commands.json generated / 编译数据库已生成\n"; \
	else \
		printf "$(RED)✗ compile_commands.json not found in $(COMPDB_BUILD_DIR)$(RESET)\n"; \
		exit 1; \
	fi
	@printf "$(DIM)Tip / 提示: run :LspRestart clangd in Neovim / 在 Neovim 中运行 :LspRestart clangd$(RESET)\n"

build:
	$(call section,Building firmware / 编译固件)
	$(call cmd,arduino-cli compile --fqbn $(FQBN) $(BUILD_PROPERTIES) --build-path $(BUILD_DIR) $(SKETCH))
	@arduino-cli compile --fqbn $(FQBN) $(BUILD_PROPERTIES) --build-path $(BUILD_DIR) $(SKETCH)
	$(call ok,build finished / 编译完成)

debug-build: BUILD_OPT_FLAGS := -Og -g3
debug-build: build

debug-server:
	$(call section,Starting $(BOARD_NAME) OpenOCD server / 启动 $(BOARD_NAME) OpenOCD 调试服务器)
	@if [ ! -x "$(OPENOCD_BIN)" ]; then \
		printf "$(RED)✗ OpenOCD not found / 未找到 OpenOCD: $(OPENOCD_BIN)$(RESET)\n"; \
		printf "Run / 运行:\n  arduino-cli core install esp32:esp32\n"; \
		exit 1; \
	fi
	$(call cmd,$(OPENOCD_BIN) -s $(OPENOCD_SCRIPTS) -f $(OPENOCD_BOARD_CFG))
	@$(OPENOCD_BIN) -s $(OPENOCD_SCRIPTS) -f $(OPENOCD_BOARD_CFG)

debug-gdb:
	$(call section,Connecting GDB to OpenOCD / 使用 GDB 连接 OpenOCD)
	@if [ ! -x "$(GDB_BIN)" ]; then \
		printf "$(RED)✗ $(GDB_TARGET_ARCH)-gdb not found / 未找到 $(GDB_TARGET_ARCH)-gdb: $(GDB_BIN)$(RESET)\n"; \
		printf "Run / 运行:\n  arduino-cli core install esp32:esp32\n"; \
		exit 1; \
	fi
	@if [ ! -f "$(DEBUG_ELF)" ]; then \
		printf "$(RED)✗ Debug ELF not found / 未找到调试 ELF: $(DEBUG_ELF)$(RESET)\n"; \
		printf "Run / 运行:\n  make debug-build\n"; \
		exit 1; \
	fi
	$(call cmd,$(GDB_BIN) -q $(DEBUG_ELF) -ex "target remote localhost:$(DEBUG_GDB_PORT)" -ex "monitor reset halt")
	@$(GDB_BIN) -q $(DEBUG_ELF) -ex "target remote localhost:$(DEBUG_GDB_PORT)" -ex "monitor reset halt"

serial-tui:
	$(call section,Building serial TUI / 构建串口 TUI)
	$(call cmd,cargo build --release --manifest-path $(SERIAL_TUI_DIR)/Cargo.toml)
	@cargo build --release --manifest-path $(SERIAL_TUI_DIR)/Cargo.toml
	$(call ok,serial TUI ready / 串口 TUI 已就绪)

ble-client:
	$(call section,Building BLE Agent client / 构建 BLE Agent 客户端)
	$(call cmd,cargo build --release --manifest-path $(BLE_CLIENT_DIR)/Cargo.toml)
	@cargo build --release --manifest-path $(BLE_CLIENT_DIR)/Cargo.toml
	$(call ok,BLE Agent client ready / BLE Agent 客户端已就绪)

ble-scan: ble-client
	$(call section,Scanning BLE devices / 扫描 BLE 设备)
	@$(BLE_CLIENT_BIN) scan

ble-demo: ble-client
	$(call section,Running BLE Agent hardware demo / 运行 BLE Agent 硬件测试)
	@$(BLE_CLIENT_BIN) demo

tools: serial-tui ble-client

gif-to-rgb565:
	$(call section,Converting GIF to LCD RGB565 / 转换 GIF 为 LCD RGB565)
	@if [ -z "$(GIF)" ]; then \
		printf "$(RED)✗ missing GIF input$(RESET)\n"; \
		printf "Usage / 用法:\n  make gif-to-rgb565 GIF=input.gif GIF_OUT=src/app/assets/boot-anim.h GIF_NAME=LcdAnimation GIF_WIDTH=80 GIF_HEIGHT=160 GIF_FIT=contain\n"; \
		exit 1; \
	fi
	$(call cmd,python3 tools/gif_to_rgb565.py "$(GIF)" -o "$(GIF_OUT)" --name "$(GIF_NAME)" --width $(GIF_WIDTH) --height $(GIF_HEIGHT) --fps $(GIF_FPS) --fit "$(GIF_FIT)")
	@python3 tools/gif_to_rgb565.py "$(GIF)" -o "$(GIF_OUT)" --name "$(GIF_NAME)" --width $(GIF_WIDTH) --height $(GIF_HEIGHT) --fps $(GIF_FPS) --fit "$(GIF_FIT)"
	$(call ok,GIF converted / GIF 已转换)

upload:
	$(call section,Uploading firmware / 上传固件)
	@if [ ! -e "$(PORT)" ]; then \
		printf "$(YELLOW)!$(RESET) serial port not found / 未找到串口: $(PORT)\n"; \
		printf "Use / 使用: make list-ports\n"; \
	fi
	$(call cmd,arduino-cli upload -p $(PORT) --fqbn $(FQBN) --input-dir $(BUILD_DIR))
	@arduino-cli upload -p $(PORT) --fqbn $(FQBN) --input-dir $(BUILD_DIR)
	$(call ok,upload finished / 上传完成)

monitor: serial-tui
	$(call section,Opening serial monitor / 打开串口监视器)
	@if [ ! -e "$(PORT)" ]; then \
		printf "$(YELLOW)!$(RESET) serial port not found / 未找到串口: $(PORT)\n"; \
		printf "Use / 使用: make list-ports\n"; \
	fi
	$(call cmd,$(SERIAL_TUI_BIN) --port $(PORT) --baud $(BAUD))
	@$(SERIAL_TUI_BIN) --port $(PORT) --baud $(BAUD)

monitor-arduino:
	$(call section,Opening Arduino CLI serial monitor / 打开 Arduino CLI 串口监视器)
	@if [ ! -e "$(PORT)" ]; then \
		printf "$(YELLOW)!$(RESET) serial port not found / 未找到串口: $(PORT)\n"; \
		printf "Use / 使用: make list-ports\n"; \
	fi
	$(call cmd,arduino-cli monitor -p $(PORT) -c baudrate=$(BAUD))
	@arduino-cli monitor -p $(PORT) -c baudrate=$(BAUD)

list-ports:
	$(call section,Connected boards and ports / 已连接板卡和串口)
	@arduino-cli board list

clean:
	$(call section,Cleaning build output / 清理构建输出)
	$(call cmd,rm -rf $(BUILD_DIR) compile_commands.json)
	@rm -rf $(BUILD_DIR) compile_commands.json
	$(call ok,clean finished / 清理完成)

clean-log:
	$(call section,Cleaning Neovim LSP log / 清理 Neovim LSP 日志)
	$(call cmd,rm -f ~/.local/state/nvim/lsp.log)
	@rm -f ~/.local/state/nvim/lsp.log
	$(call ok,lsp.log removed / lsp.log 已删除)

format:
	$(call section,Formatting source files / 格式化源代码)
	$(call cmd,clang-format -i $(FORMAT_FILES))
	@clang-format -i $(FORMAT_FILES)
	$(call ok,formatting finished / 格式化完成)

format-check:
	$(call section,Checking source file formatting / 检查源代码格式)
	$(call cmd,clang-format --dry-run --Werror $(FORMAT_FILES))
	@clang-format --dry-run --Werror $(FORMAT_FILES)
	$(call ok,formatting correct / 格式正确)
