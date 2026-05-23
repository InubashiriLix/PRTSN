#pragma once

#include "src/cfg/AppConfig.h"
#include "src/ctl/inc/EspNowEchoController.h"
#include "src/dom/NodeInfo.h"
#include "src/dvc/inc/Serial.h"
#include "src/svc/inc/SerialConsoleService.h"

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef Serial
#undef Serial
#endif

namespace EspNowEchoApp
{
    namespace Detail
    {
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
            LED                  ledMain {AppConfig::Hardware::LedMainPin, LED::State::DIGITAL_HIGH};
            LED                  ledAux {AppConfig::Hardware::LedAuxPin, LED::State::DIGITAL_LOW};

            Wifi::Config wifiConfig {
                .mode = AppConfig::Network::WifiMode,
                .sta  = {
                    .ssid        = AppConfig::Network::StaSsid,
                    .password    = AppConfig::Network::StaPassword,
                    .hostname    = AppConfig::Identity::NodeId,
                    .reconnectMs = AppConfig::Network::StaReconnectMs,
                    .connect     = AppConfig::Network::StaConnect,
                    .channel     = AppConfig::Network::StaChannel},
                .ap = {}};

            Wifi wifi {wifiConfig};

            EspNowNode::Config espNowConfig {
                .localNodeId         = AppConfig::Identity::NodeId,
                .localNodeName       = AppConfig::Identity::NodeName,
                .channel             = AppConfig::Network::StaChannel,
                .discoveryIntervalMs = AppConfig::EspNow::DiscoveryIntervalMs,
                .heartbeatIntervalMs = AppConfig::EspNow::HeartbeatIntervalMs,
                .staleTimeoutMs      = AppConfig::EspNow::StaleTimeoutMs,
                .offlineTimeoutMs    = AppConfig::EspNow::OfflineTimeoutMs,
                .ackTimeoutMs        = AppConfig::EspNow::AckTimeoutMs,
                .maxRetries          = AppConfig::EspNow::MaxRetries,
            };

            EspNowEchoController::Config echoConfig {
                .localNodeId           = AppConfig::Identity::NodeId,
                .localNodeName         = AppConfig::Identity::NodeName,
                .enableSender          = AppConfig::EspNowEcho::Sender,
                .enableReceiver        = AppConfig::EspNowEcho::Receiver,
                .useAck                = AppConfig::EspNowEcho::UseAck,
                .verboseLog            = AppConfig::EspNowEcho::VerboseLog,
                .statsLog              = AppConfig::EspNowEcho::StatsLog,
                .payloadLen            = AppConfig::EspNowEcho::PayloadLen,
                .sendIntervalMs        = AppConfig::EspNowEcho::SendIntervalMs,
                .statsLogIntervalMs    = AppConfig::EspNowEcho::StatsLogIntervalMs,
                .activityLedWindowMs   = AppConfig::EspNowEcho::ActivityLedWindowMs,
                .activityLedIntervalMs = AppConfig::EspNowEcho::ActivityLedIntervalMs,
            };

            EspNowEchoController controller {
                EspNowNode::instance(),
                ledMain,
                ledAux,
                console,
                echoConfig,
            };

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
                app.wifi.update();
                app.controller.update();
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
                "espnow_echo",
                TaskStackWords,
                nullptr,
                TaskPriority,
                &app.taskHandle);

            if (ok != pdPASS) {
                app.taskHandle = nullptr;
                app.nodeInfo.updateNodeState(ERROR);
                app.console.error("failed to create esp-now task");
                return false;
            }

            return true;
        }
    }

    inline void setup() {
        Detail::Context& app = Detail::context();

        app.console.setup();
        app.console.printBootBanner(app.nodeInfo);

        if (!app.wifi.begin()) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.error("failed to start wifi");
            return;
        }

        if (!EspNowNode::instance().setup(&app.wifi, app.espNowConfig)) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.error("failed to start esp-now node");
            return;
        }

        if (!app.controller.setup()) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.error("failed to setup esp-now echo controller");
            return;
        }

        app.nodeInfo.updateNodeState(RUNNING);
        app.console.printState(app.nodeInfo.getNodeState());

        Detail::startTask();
    }

    inline void idle() {
        delay(AppConfig::Runtime::AppLoopIntervalMs);
    }
}
