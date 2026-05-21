#include "src/app/inc/PrtnApp.h"

#include "src/cfg/AppConfig.h"
#include "src/ctl/inc/EspNowEchoController.h"
#include "src/dom/NodeInfo.h"

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace
{
    constexpr TickType_t  EspNowTaskPeriodTicks = pdMS_TO_TICKS(AppConfig::Runtime::AppLoopIntervalMs);
    constexpr uint32_t    EspNowTaskStackWords  = AppConfig::Runtime::EspNowTaskStackWords;
    constexpr UBaseType_t EspNowTaskPriority    = AppConfig::Runtime::EspNowTaskPriority;

    constexpr TickType_t  HeartbeatTaskPeriodTicks = pdMS_TO_TICKS(AppConfig::Runtime::HeartbeatIntervalMs);
    constexpr uint32_t    HeartbeatTaskStackWords  = AppConfig::Runtime::HeartbeatTaskStackWords;
    constexpr UBaseType_t HeartbeatTaskPriority    = AppConfig::Runtime::HeartbeatTaskPriority;

    TaskHandle_t espNowTaskHandle    = nullptr;
    TaskHandle_t heartbeatTaskHandle = nullptr;

    static NodeInfo nodeInfo {
        AppConfig::Identity::ProjectName,
        AppConfig::Identity::ProjectFullName,
        AppConfig::Identity::BoardName,
        AppConfig::Identity::VersionString,
        AppConfig::Identity::NodeName,
        AppConfig::Identity::NodeId,
        BOOTING,
    };

    static SerialConsole console(AppConfig::Hardware::SerialBaudrate);
    static LED           ledMain(AppConfig::Hardware::LedMainPin, LED::State::DIGITAL_HIGH);
    static LED           ledAux(AppConfig::Hardware::LedAuxPin, LED::State::DIGITAL_LOW);

    static Wifi::Config wifiConfig {
        .mode = AppConfig::Network::WifiMode,
        .sta  = {
            .ssid        = AppConfig::Network::StaSsid,
            .password    = AppConfig::Network::StaPassword,
            .hostname    = AppConfig::Identity::NodeId,
            .reconnectMs = AppConfig::Network::StaReconnectMs,
            .connect     = AppConfig::Network::StaConnect,
            .channel     = AppConfig::Network::StaChannel},
        .ap = {}};

    static Wifi wifi(wifiConfig);

    static EspNowNode::Config espNowConfig {
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

    static EspNowEchoController::Config espNowEchoConfig {
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

    static EspNowEchoController espNowEchoController(EspNowNode::instance(), ledMain, ledAux, console, espNowEchoConfig);

    void espNowTaskEntry(void*) {
        TickType_t taskLastWakeTime = xTaskGetTickCount();

        for (;;) {
            wifi.update();
            espNowEchoController.update();
            vTaskDelayUntil(&taskLastWakeTime, EspNowTaskPeriodTicks);
        }
    }

    bool startEspNowTask() {
        if (espNowTaskHandle != nullptr) {
            return true;
        }

        const BaseType_t ok = xTaskCreate(
            espNowTaskEntry,
            "espnow_echo",
            EspNowTaskStackWords,
            nullptr,
            EspNowTaskPriority,
            &espNowTaskHandle);

        if (ok != pdPASS) {
            espNowTaskHandle = nullptr;
            nodeInfo.updateNodeState(ERROR);
            console.error("failed to create esp-now task");
            return false;
        }

        return true;
    }

} // namespace

void PrtnApp::setup() {
    console.setup();
    console.printBootBanner(nodeInfo);

    if (!wifi.begin()) {
        nodeInfo.updateNodeState(ERROR);
        console.error("failed to start wifi");
        return;
    }

    if (!EspNowNode::instance().setup(&wifi, espNowConfig)) {
        nodeInfo.updateNodeState(ERROR);
        console.error("failed to start esp-now node");
        return;
    }

    if (!espNowEchoController.setup()) {
        nodeInfo.updateNodeState(ERROR);
        console.error("failed to setup esp-now echo controller");
        return;
    }

    nodeInfo.updateNodeState(RUNNING);
    console.printState(nodeInfo.getNodeState());

    startEspNowTask();
}

void PrtnApp::idle() {
    delay(AppConfig::Runtime::AppLoopIntervalMs);
}
