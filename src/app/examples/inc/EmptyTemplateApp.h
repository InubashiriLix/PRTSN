#pragma once

#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "src/cfg/AppConfig.h"
#include "src/dom/NodeInfo.h"
#include "src/dvc/inc/Button.h"
#include "src/dvc/inc/Serial.h"
#include "src/svc/inc/SerialConsoleService.h"

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef Serial
#undef Serial
#endif

namespace EmptyTemplateApp
{
    namespace Detail
    {
        enum class PinId : uint8_t
        {
            Button1,
        };

        inline constexpr auto Pins = ::prtn::pin::layout(
            ::prtn::pin::bind(PinId::Button1, GPIO_NUM_9, ::prtn::pin::Role::InputPullup));

        constexpr TickType_t  TaskPeriodTicks = pdMS_TO_TICKS(AppConfig::Runtime::AppLoopIntervalMs);
        constexpr uint32_t    TaskStackWords  = AppConfig::Runtime::EspNowTaskStackWords;
        constexpr UBaseType_t TaskPriority    = AppConfig::Runtime::EspNowTaskPriority;

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

            Button button1 {Pins[PinId::Button1], LOW, INPUT_PULLUP, 50};

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
                app.button1.update();
                vTaskDelayUntil(&taskLastWakeTime, TaskPeriodTicks);
            }
        }

        inline bool startTask() {
            Context& app = context();
            if (app.taskHandle != nullptr) {
                return true;
            }

            const BaseType_t ok = xTaskCreate(
                taskEntry,
                "button test",
                TaskStackWords,
                nullptr,
                TaskPriority,
                &app.taskHandle);

            if (ok != pdPASS) {
                app.taskHandle = nullptr;
                app.nodeInfo.updateNodeState(ERROR);
                app.console.error("failed to createa button test task");
                return false;
            }

            return false;
        }
    }

    inline void setup() {
        Detail::Context& app = Detail::context();

        app.console.setup();
        app.console.printBootBanner(app.nodeInfo);
        if (!app.button1.setup())
            app.console.error("Failed to setup Button1");
        app.button1.setCallback([](Button::Event event, Button::State, void* context) -> void {
            if (event != Button::Event::PRESSED && context != nullptr) {
                return;
            }
            auto* app = static_cast<Detail::Context*>(context);
            app->console.log("Button1 pressed");
        },
                                &app);

        // Put complete app-level experiment setup here.

        app.nodeInfo.updateNodeState(RUNNING);
        app.console.printState(app.nodeInfo.getNodeState());

        Detail::startTask();
    }

    inline void idle() {
        Detail::context().console.updateCommandResponse();

        // Put complete app-level experiment loop code here.

        delay(AppConfig::Runtime::AppLoopIntervalMs);
    }
}
