#pragma once

#include "src/cfg/AppConfig.h"
#include "src/dom/NodeInfo.h"
#include "src/dvc/inc/Serial.h"
#include "src/dvc/inc/WS2812.h"
#include "src/svc/inc/SerialConsoleService.h"

#include <Arduino.h>

#include "freertos/task.h"

#ifdef Serial
#undef Serial
#endif

namespace WS2812Example
{
    namespace Detail
    {
        enum class PinId : uint8_t
        {
            LedData,
        };

        using Color = Color;

        inline constexpr auto Pins = ::prtn::pin::layout(
            ::prtn::pin::bind(PinId::LedData, GPIO_NUM_48, ::prtn::pin::Role::RmtTx));

        constexpr size_t      LedCount       = 8;
        constexpr uint8_t     Brightness     = 32;
        constexpr TickType_t  TaskPeriod     = pdMS_TO_TICKS(10);
        constexpr uint32_t    TaskStackBytes = 4096;
        constexpr UBaseType_t TaskPriority   = 4;

        inline WS2812::Config makeConfig() {
            WS2812::Config config {};
            config.pixelCount = LedCount;
            config.brightness = Brightness;
            config.colorOrder = WS2812::ColorOrder::GRB;
            return config;
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
            WS2812               led {Pins[PinId::LedData], makeConfig()};

            TaskHandle_t taskHandle = nullptr;
            uint32_t     frame      = 0;
        };

        inline Context& context() {
            static Context app;
            return app;
        }

        inline WS2812::Color wheel(uint8_t pos) {
            if (pos < 85) {
                return WS2812::Color {
                    .r = static_cast<uint8_t>(255 - pos * 3),
                    .g = static_cast<uint8_t>(pos * 3),
                    .b = 0,
                    .a = 255,
                };
            }
            if (pos < 170) {
                pos = static_cast<uint8_t>(pos - 85);
                return WS2812::Color {
                    .r = 0,
                    .g = static_cast<uint8_t>(255 - pos * 3),
                    .b = static_cast<uint8_t>(pos * 3),
                    .a = 255,
                };
            }

            pos = static_cast<uint8_t>(pos - 170);
            return WS2812::Color {
                .r = static_cast<uint8_t>(pos * 3),
                .g = 0,
                .b = static_cast<uint8_t>(255 - pos * 3),
                .a = 255,
            };
        }

        template <size_t Depth, auto... Errors>
        inline void logError(Context& app, const char* op, const TracedErrorSet<Depth, Errors...>& error) {
            app.console.error("WS2812 %s failed: %s::%s (%ld)%s%s",
                              op,
                              error.domain(),
                              error.name(),
                              static_cast<long>(error.numeric_code()),
                              error.has_message() ? ": " : "",
                              error.message());
        }

        inline bool updateColorFlow(Context& app) {
            for (size_t i = 0; i < app.led.pixelCount(); ++i) {
                const WS2812::Color color  = wheel(static_cast<uint8_t>(app.frame + i * 32));
                const auto          result = app.led.setPixel(i, color);
                if (result.is_err()) {
                    logError(app, "setPixel", result.error());
                    return false;
                }
            }

            const auto showResult = app.led.show();
            if (showResult.is_err()) {
                logError(app, "show", showResult.error());
                return false;
            }

            ++app.frame;
            return true;
        }

        inline void taskEntry(void*) {
            Context&   app              = context();
            TickType_t taskLastWakeTime = xTaskGetTickCount();

            for (;;) {
                app.console.updateCommandResponse();

                if (!updateColorFlow(app)) {
                    app.nodeInfo.updateNodeState(ERROR);
                    app.console.printState(app.nodeInfo.getNodeState());
                    vTaskDelay(pdMS_TO_TICKS(250));
                    continue;
                }

                vTaskDelayUntil(&taskLastWakeTime, TaskPeriod);
            }
        }

        inline bool startTask() {
            Context& app = context();
            if (app.taskHandle != nullptr) {
                return true;
            }

            const BaseType_t ok = xTaskCreate(
                taskEntry,
                "ws2812 example",
                TaskStackBytes,
                nullptr,
                TaskPriority,
                &app.taskHandle);

            if (ok != pdPASS) {
                app.taskHandle = nullptr;
                app.nodeInfo.updateNodeState(ERROR);
                app.console.error("failed to create WS2812 example task");
                return false;
            }

            return true;
        }
    }

    inline void setup() {
        Detail::Context& app = Detail::context();

        app.console.setup();
        app.console.printBootBanner(app.nodeInfo);
        app.console.info("WS2812 color flow: pin=%d count=%u brightness=%u order=%s",
                         Detail::Pins[Detail::PinId::LedData],
                         static_cast<unsigned>(Detail::LedCount),
                         static_cast<unsigned>(Detail::Brightness),
                         WS2812::colorOrderName(WS2812::ColorOrder::GRB));

        const auto setupResult = app.led.setup();
        if (setupResult.is_err()) {
            app.nodeInfo.updateNodeState(ERROR);
            Detail::logError(app, "setup", setupResult.error());
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }

        app.nodeInfo.updateNodeState(RUNNING);
        app.console.printState(app.nodeInfo.getNodeState());

        if (!Detail::startTask()) {
            app.console.printState(app.nodeInfo.getNodeState());
        }
    }

    inline void idle() {
        Detail::context().console.updateCommandResponse();
        delay(AppConfig::Runtime::AppLoopIntervalMs);
    }
}
