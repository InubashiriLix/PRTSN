include config/project.mk
-include config/local.mk

BASE_FQBN := $(BOARD_PACKAGE):$(BOARD_ARCH):$(BOARD_ID)
FQBN_OPTIONS := UploadSpeed=$(UPLOAD_SPEED),CDCOnBoot=$(CDC_ON_BOOT),CPUFreq=$(CPU_FREQ),FlashFreq=$(FLASH_FREQ),PartitionScheme=$(PARTITION_SCHEME),DebugLevel=$(DEBUG_LEVEL),EraseFlash=$(ERASE_FLASH)
FQBN := $(BASE_FQBN):$(FQBN_OPTIONS)

BUILD_FLAGS := $(strip $(BUILD_DEFINES) $(BUILD_OPT_FLAGS))
ifneq ($(BUILD_FLAGS),)
BUILD_PROPERTIES := --build-property compiler.c.extra_flags="$(BUILD_FLAGS)" --build-property compiler.cpp.extra_flags="$(BUILD_FLAGS)"
endif

SERIAL_TUI_DIR := tools/serial-tui
SERIAL_TUI_BIN := $(SERIAL_TUI_DIR)/target/release/serial-tui

RESET  := \033[0m
BOLD   := \033[1m
DIM    := \033[2m
RED    := \033[31m
GREEN  := \033[32m
YELLOW := \033[33m
CYAN   := \033[36m

.PHONY: help info board-options check compdb build upload monitor monitor-arduino serial-tui tools list-ports clean clean-log

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
	@printf "$(DIM)$ %s$(RESET)\n" "$(1)"
endef

help:
	@printf "\n$(BOLD)PRTN$(RESET) - PRTS Node / PRTS 节点\n"
	@printf "$(DIM)Minimal layered Arduino CLI project for AirM2M CORE ESP32-C3.$(RESET)\n"
	@printf "$(DIM)适用于 AirM2M CORE ESP32-C3 的极简分层 Arduino CLI 项目。$(RESET)\n\n"

	@printf "$(BOLD)Targets / 可用命令$(RESET)\n"
	@printf "  $(CYAN)make check$(RESET)        Check toolchain, ESP32 core, board and port\n"
	@printf "                    检查 arduino-cli、ESP32 core、板卡和串口\n"
	@printf "  $(CYAN)make compdb$(RESET)       Generate compile_commands.json for clangd\n"
	@printf "                    为 clangd 生成 compile_commands.json\n"
	@printf "  $(CYAN)make build$(RESET)        Compile firmware\n"
	@printf "                    编译固件\n"
	@printf "  $(CYAN)make upload$(RESET)       Upload firmware to $(BOLD)$(PORT)$(RESET)\n"
	@printf "                    上传固件到 $(BOLD)$(PORT)$(RESET)\n"
	@printf "  $(CYAN)make monitor$(RESET)      Open serial monitor at $(BOLD)$(BAUD)$(RESET)\n"
	@printf "                    使用 Rust TUI 打开串口监视器，波特率 $(BOLD)$(BAUD)$(RESET)\n"
	@printf "  $(CYAN)make monitor-arduino$(RESET) Open Arduino CLI serial monitor\n"
	@printf "                    使用 arduino-cli 原生串口监视器\n"
	@printf "  $(CYAN)make serial-tui$(RESET)   Build Rust serial TUI tool\n"
	@printf "                    构建 Rust 串口 TUI 工具\n"
	@printf "  $(CYAN)make tools$(RESET)        Build local development tools\n"
	@printf "                    构建本地开发工具\n"
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

	@printf "$(BOLD)Config / 当前配置$(RESET)\n"
	@printf "  FQBN      = $(FQBN)\n"
	@printf "  Config    = config/project.mk + optional config/local.mk\n"
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
	@printf "Board   / 板卡      : $(BOLD)AirM2M CORE ESP32-C3$(RESET)\n"
	@printf "Base FQBN          : $(BASE_FQBN)\n"
	@printf "Full FQBN          : $(FQBN)\n"
	@printf "Partition / 分区   : $(PARTITION_SCHEME)\n"
	@printf "CPU freq / 主频    : $(CPU_FREQ) MHz\n"
	@printf "Flash freq         : $(FLASH_FREQ) MHz\n"
	@printf "USB CDC on boot    : $(CDC_ON_BOOT)\n"
	@printf "Upload speed       : $(UPLOAD_SPEED)\n"
	@printf "Debug level        : $(DEBUG_LEVEL)\n"
	@printf "Erase flash        : $(ERASE_FLASH)\n"
	@printf "Build flags        : $(if $(BUILD_FLAGS),$(BUILD_FLAGS),<arduino default>)\n"
	@printf "Port   / 串口      : $(PORT)\n"
	@printf "Baud   / 波特率    : $(BAUD)\n"
	@printf "Sketch             : $(SKETCH)\n"
	@printf "Build dir / 构建目录: $(BUILD_DIR)\n"

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

	@command -v arduino-language-server >/dev/null || { \
		printf "$(YELLOW)! arduino-language-server not found / 未找到 arduino-language-server$(RESET)\n"; \
		printf "Install / 安装:\n  sudo pacman -S arduino-language-server\n"; \
		exit 0; \
	}
	$(call ok,arduino-language-server found / 已找到 arduino-language-server)

	$(call section,Checking ESP32 Arduino core / 检查 ESP32 Arduino core)
	@arduino-cli core list | grep -q "esp32:esp32" || { \
		printf "$(RED)✗ Missing core: esp32:esp32 / 缺少 ESP32 core$(RESET)\n"; \
		printf "Run / 运行:\n  arduino-cli core install esp32:esp32\n"; \
		exit 1; \
	}
	$(call ok,esp32:esp32 core installed / ESP32 core 已安装)

	$(call section,Checking board FQBN / 检查板卡 FQBN)
	@arduino-cli board listall | grep -q "$(BASE_FQBN)" || { \
		printf "$(RED)✗ Board not found / 未找到板卡: $(BASE_FQBN)$(RESET)\n"; \
		printf "Try / 尝试:\n  arduino-cli board listall | grep -i AirM2M\n"; \
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
	$(call cmd,arduino-cli compile --fqbn $(FQBN) $(BUILD_PROPERTIES) --only-compilation-database --build-path $(BUILD_DIR) $(SKETCH))
	@arduino-cli compile --fqbn $(FQBN) $(BUILD_PROPERTIES) --only-compilation-database --build-path $(BUILD_DIR) $(SKETCH)
	@if [ -f "$(BUILD_DIR)/compile_commands.json" ]; then \
		cp "$(BUILD_DIR)/compile_commands.json" ./compile_commands.json; \
		printf "$(GREEN)✓$(RESET) compile_commands.json generated / 编译数据库已生成\n"; \
	else \
		printf "$(RED)✗ compile_commands.json not found in $(BUILD_DIR)$(RESET)\n"; \
		exit 1; \
	fi
	@printf "$(DIM)Tip / 提示: run :LspRestart clangd in Neovim / 在 Neovim 中运行 :LspRestart clangd$(RESET)\n"

build:
	$(call section,Building firmware / 编译固件)
	$(call cmd,arduino-cli compile --fqbn $(FQBN) $(BUILD_PROPERTIES) --build-path $(BUILD_DIR) $(SKETCH))
	@arduino-cli compile --fqbn $(FQBN) $(BUILD_PROPERTIES) --build-path $(BUILD_DIR) $(SKETCH)
	$(call ok,build finished / 编译完成)

serial-tui:
	$(call section,Building serial TUI / 构建串口 TUI)
	$(call cmd,cargo build --release --manifest-path $(SERIAL_TUI_DIR)/Cargo.toml)
	@cargo build --release --manifest-path $(SERIAL_TUI_DIR)/Cargo.toml
	$(call ok,serial TUI ready / 串口 TUI 已就绪)

tools: serial-tui

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
