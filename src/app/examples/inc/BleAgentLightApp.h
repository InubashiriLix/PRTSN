#pragma once

#include "src/cfg/AppConfig.h"
#include "src/dom/NodeInfo.h"
#include "src/dvc/inc/Button.h"
#include "src/dvc/inc/ST7735.h"
#include "src/dvc/inc/Serial.h"
#include "src/fw/inc/spi.h"
#include "src/svc/inc/BleAgentLight/AgentRegistry.h"
#include "src/svc/inc/BleAgentLight/BleAgentLight.h"
#include "src/svc/inc/BleAgentLight/DisplayService.h"
#include "src/svc/inc/BleAgentLight/Ws2812Service.h"
#include "src/svc/inc/SerialConsoleService.h"

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <optional>

#ifdef Serial
#undef Serial
#endif

namespace BleAgentLightApp
{
    namespace Detail
    {
        enum class PinId : uint8_t
        {
            Button1,
            LcdReset,
            LcdSck,
            LcdMosi,
            LcdCs,
            LcdDc,
            LcdBacklight,
            AgentLeds,
        };

        inline constexpr auto Pins = ::prtn::pin::layout(
            ::prtn::pin::bind(PinId::Button1, GPIO_NUM_9, ::prtn::pin::Role::InputPullup),
            ::prtn::pin::bind(PinId::LcdReset, GPIO_NUM_1, ::prtn::pin::Role::Output),
            ::prtn::pin::bind(PinId::LcdSck, ::prtn::b::spi::sck, ::prtn::pin::Role::SpiSck),
            ::prtn::pin::bind(PinId::LcdMosi, ::prtn::b::spi::mosi, ::prtn::pin::Role::SpiMosi),
            ::prtn::pin::bind(PinId::LcdCs, ::prtn::b::spi::cs, ::prtn::pin::Role::SpiCs),
            ::prtn::pin::bind(PinId::LcdDc, GPIO_NUM_0, ::prtn::pin::Role::Output),
            ::prtn::pin::bind(PinId::LcdBacklight, GPIO_NUM_10, ::prtn::pin::Role::Output),
            ::prtn::pin::bind(PinId::AgentLeds, GPIO_NUM_48, ::prtn::pin::Role::RmtTx));

        constexpr uint32_t SpiClockHz     = 20 * 1000 * 1000;
        constexpr size_t   DeviceIndex    = 0;
        constexpr size_t   DmaBufferBytes = 3200;

        constexpr TickType_t  TaskPeriodTicks = pdMS_TO_TICKS(AppConfig::Runtime::AppLoopIntervalMs);
        constexpr uint32_t    TaskStackWords  = AppConfig::Runtime::EspNowTaskStackWords;
        constexpr UBaseType_t TaskPriority    = AppConfig::Runtime::EspNowTaskPriority;

        inline SPI::BusConfig makeBusConfig() {
            SPI::BusConfig config {};
            config.host                = SPI2_HOST;
            config.bus.mosi_io_num     = Pins[PinId::LcdMosi];
            config.bus.miso_io_num     = -1;
            config.bus.sclk_io_num     = Pins[PinId::LcdSck];
            config.bus.quadwp_io_num   = -1;
            config.bus.quadhd_io_num   = -1;
            config.bus.max_transfer_sz = DmaBufferBytes;
            config.bus.flags           = SPICOMMON_BUSFLAG_MASTER;
            config.bus.intr_flags      = 0;
            return config;
        }

        inline SPI::DeviceConfig makeDeviceConfig() {
            SPI::DeviceConfig config {};
            config.index                 = DeviceIndex;
            config.device.clock_speed_hz = SpiClockHz;
            config.device.mode           = 0;
            config.device.spics_io_num   = Pins[PinId::LcdCs];
            config.device.queue_size     = 4;
            return config;
        }

        inline ST7735::Config makeLcdConfig() {
            ST7735::Config config       = ST7735::makeConfig(ST7735::Orientation::Portrait);
            config.dmaBufferBytes       = DmaBufferBytes;
            config.resetPin             = Pins[PinId::LcdReset];
            config.dcPin                = Pins[PinId::LcdDc];
            config.backlightPin         = Pins[PinId::LcdBacklight];
            config.useResetPin          = true;
            config.useBacklightPin      = true;
            config.autoDmaBuffer        = true;
            config.useOrientationPreset = true;
            config.invertColors         = true;
            return config;
        }

        inline ble_agent_light::Ws2812Service::Config makeLedConfig() {
            return {
                .ledCount = ble_agent_light::protocol::AgentCount,
                .ledPin   = Pins[PinId::AgentLeds],
            };
        }

        struct Context
        {
            NodeInfo nodeInfo {
                AppConfig::Identity::ProjectName,
                AppConfig::Identity::ProjectFullName,
                AppConfig::Identity::BoardName,
                AppConfig::Identity::VersionString,
                AppConfig::Identity::NodeName,
                AppConfig::Identity::NodeId,
                BOOTING,
            };

            dvc::Serial          serial {AppConfig::Hardware::SerialBaudrate};
            SerialConsoleService console {serial};

            ble_agent_light::AgentRegistry  registry;
            SPI                             spi {makeBusConfig()};
            ST7735                          lcd {spi, DeviceIndex, makeLcdConfig()};
            ble_agent_light::DisplayService display {lcd, registry};
            ble_agent_light::Ws2812Service  leds {makeLedConfig()};
            BleAgentLightService::Config    bleConfig {.console = &console};
            BleAgentLightService            bleService {registry, bleConfig};

            TaskHandle_t        taskHandle = nullptr;
            std::optional<bool> displayedConnectionState;
        };

        inline Context& context() {
            static Context app;
            return app;
        }

        inline void taskEntry(void*) {
            Context& app = context();

            for (;;) {
                app.console.updateCommandResponse();
                app.bleService.poll(millis());

                const bool connected = app.bleService.connected();
                if (!app.displayedConnectionState.has_value() ||
                    *app.displayedConnectionState != connected) {
                    app.leds.setConnected(connected);
                    app.displayedConnectionState = connected;
                }

                const auto displayError = app.display.takeLastError();
                if (displayError.has_value()) {
                    app.console.errorResult("Agent display update", *displayError);
                    app.nodeInfo.updateNodeState(ERROR);
                }

                const auto ledError = app.leds.takeLastError();
                if (ledError.has_value()) {
                    app.console.errorResult("Agent LED update", *ledError);
                    app.nodeInfo.updateNodeState(ERROR);
                }

                vTaskDelay(TaskPeriodTicks);
            }
        }

        inline bool startTask() {
            Context& app = context();
            if (app.taskHandle != nullptr)
                return true;

            const BaseType_t result = xTaskCreatePinnedToCore(
                taskEntry,
                "agent panel",
                TaskStackWords,
                nullptr,
                TaskPriority,
                &app.taskHandle,
                1);

            if (result != pdPASS) {
                app.taskHandle = nullptr;
                app.nodeInfo.updateNodeState(ERROR);
                app.console.error("failed to create Agent Panel task");
                return false;
            }

            return true;
        }
    }

    inline void setup() {
        Detail::Context& app = Detail::context();

        app.console.setup();
        app.console.printBootBanner(app.nodeInfo);

        const auto spiSetupResult = app.spi.setupBus();
        if (spiSetupResult.is_err()) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.errorResult("SPI setup", spiSetupResult.error());
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }

        const auto deviceResult = app.spi.addDevice(Detail::makeDeviceConfig());
        if (deviceResult.is_err()) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.errorResult("ST7735 SPI device setup", deviceResult.error());
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }

        const auto lcdResult = app.lcd.setup();
        if (lcdResult.is_err()) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.errorResult("ST7735 setup", lcdResult.error());
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }

        const auto displayResult = app.display.setup();
        if (displayResult.is_err()) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.errorResult("Agent display setup", displayResult.error());
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }

        const auto ledSetupResult = app.leds.setup();
        if (ledSetupResult.is_err()) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.errorResult("Agent LED setup", ledSetupResult.error());
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }

        const auto subscriptionResult = app.registry.subscribe(app.display.eventHandlers());
        if (subscriptionResult.is_err()) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.errorResult("Agent display subscription", subscriptionResult.error());
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }

        const auto ledSubscriptionResult = app.registry.subscribe(app.leds.eventHandlers());
        if (ledSubscriptionResult.is_err()) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.errorResult("Agent LED subscription", ledSubscriptionResult.error());
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }

        const auto bleSetupResult = app.bleService.setup();
        if (bleSetupResult.is_err()) {
            app.nodeInfo.updateNodeState(ERROR);
            // BleAgentLightService already logs the typed setup error when a
            // diagnostic Console is configured.
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }

        if (!Detail::startTask()) {
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }

        app.nodeInfo.updateNodeState(RUNNING);
        app.console.printState(app.nodeInfo.getNodeState());
    }

    inline void idle() {
        delay(AppConfig::Runtime::AppLoopIntervalMs);
    }
}
