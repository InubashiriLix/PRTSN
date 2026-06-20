#pragma once

#include "src/cfg/AppConfig.h"
#include "src/dvc/inc/KeyBoard4x5.hpp"
#include "src/dom/NodeInfo.h"
#include "src/dvc/inc/Serial.h"
#include "src/svc/inc/SerialConsoleService.h"

#include <Arduino.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef Serial
#undef Serial
#endif

namespace KeyBoard4x5Example
{
    namespace Detail
    {
        constexpr TickType_t  TaskPeriod     = pdMS_TO_TICKS(AppConfig::Runtime::AppLoopIntervalMs);
        constexpr uint32_t    TaskStackBytes = 4096;
        constexpr UBaseType_t TaskPriority   = 4;

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

            KeyBoard4x5::PinConfig keyboardConfig {
                .scanIntervalMs = 1,
                .debounceMs     = 20,
                .longPressMs    = 600,
                .activeLevel    = StdPinLevel::High,
                .colPins        = {12, 18, 19},
                .colPinMode     = StdPinFunc::Output,
                .colNum         = 3,
                .rowPins        = {2, 3, 10},
                .rowPinMode     = StdPinFunc::InputPulldown,
                .rowNum         = 3,
            };

            KeyBoard4x5  keyboard {keyboardConfig, &console};
            TaskHandle_t taskHandle = nullptr;
        };

        inline Context& context() {
            static Context app;
            return app;
        }

        inline void taskEntry(void*) {
            Context&   app              = context();
            TickType_t taskLastWakeTime = xTaskGetTickCount();

            for (;;) {
                app.console.updateCommandResponse();

                if (!app.keyboard.update()) {
                    app.nodeInfo.updateNodeState(ERROR);
                    app.console.error("keyboard update failed");
                    app.console.printState(app.nodeInfo.getNodeState());
                    vTaskDelay(pdMS_TO_TICKS(250));
                    continue;
                }

                vTaskDelayUntil(&taskLastWakeTime, TaskPeriod);
            }
        }
    }

    inline void setup() {
        Detail::Context& app = Detail::context();

        app.console.setup();
        app.console.printBootBanner(app.nodeInfo);
        app.console.info("keyboard 4x5 example: cols=%u rows=%u debounce_ms=%lu long_press_ms=%lu",
                         static_cast<unsigned>(app.keyboardConfig.colNum),
                         static_cast<unsigned>(app.keyboardConfig.rowNum),
                         static_cast<unsigned long>(app.keyboardConfig.debounceMs),
                         static_cast<unsigned long>(app.keyboardConfig.longPressMs));

        if (!app.keyboard.setup()) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.error("failed to setup keyboard");
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }

        const BaseType_t ok = xTaskCreate(
            Detail::taskEntry,
            "keyboard 4x5",
            Detail::TaskStackBytes,
            nullptr,
            Detail::TaskPriority,
            &app.taskHandle);

        if (ok != pdPASS) {
            app.taskHandle = nullptr;
            app.nodeInfo.updateNodeState(ERROR);
            app.console.error("failed to create keyboard task");
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }

        app.nodeInfo.updateNodeState(RUNNING);
        app.console.printState(app.nodeInfo.getNodeState());
    }

    inline void idle() {
        Detail::context().console.updateCommandResponse();
        delay(AppConfig::Runtime::AppLoopIntervalMs);
    }
}
