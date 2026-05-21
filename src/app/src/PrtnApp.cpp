#include "src/app/inc/PrtnApp.h"

#include "src/cfg/BoardConfig.h"
#include "src/cfg/BuildConfig.h"
#include "src/cfg/ServiceConfig.h"
#include "src/ctl/inc/EspNowEchoController.h"
#include "src/ctl/inc/HeartbeatController.h"
#include "src/ctl/inc/NodeController.h"
#include "src/dom/NodeInfo.h"
#include "src/tsk/inc/PrtnTasks.h"

#include <Arduino.h>

namespace
{
    static NodeInfo nodeInfo {
        PRTN_PROJECT_NAME,
        PRTN_PROJECT_FULL_NAME,
        PRTN_BOARD_NAME,
        PRTN_VERSION_STRING,
        PRTN_NODE_NAME,
        PRTN_NODE_ID,
        BOOTING,
    };

    static SerialConsole console;
    static LED           ledMain(PRTN_LED_PIN, LED::State::DIGITAL_HIGH);
    static LED           ledAux(PRTN_LED_PIN_AUX, LED::State::DIGITAL_LOW);

    static NodeController::Config nodeControllerConfig {
        .updateWifi = PRTN_NODE_CONTROLLER_UPDATE_WIFI != 0,
        .logWifiStatus = PRTN_NODE_CONTROLLER_WIFI_STATUS_LOG != 0,
        .wifiUpdateIntervalMs = 400,
        .wifiStatusLogIntervalMs = 1000,
    };

    static Wifi::Config wifiConfig {
        .mode = PRTN_WIFI_MODE,
        .sta  = {
            .ssid        = PRTN_WIFI_STA_SSID,
            .password    = PRTN_WIFI_STA_PASSWORD,
            .hostname    = PRTN_WIFI_STA_HOSTNAME,
            .reconnectMs = PRTN_WIFI_STA_RECONNECT_MS,
            .connect     = PRTN_WIFI_STA_CONNECT != 0,
            .channel     = PRTN_WIFI_STA_CHANNEL},
        .ap = {}};

    static Wifi wifi(wifiConfig);

    EspNowNode::Config espNowConfig {
        .localNodeId         = PRTN_NODE_ID,
        .localNodeName       = PRTN_NODE_NAME,
        .channel             = PRTN_WIFI_STA_CHANNEL,
        .discoveryIntervalMs = PRTN_ESPNOW_DISCOVERY_INTERVAL_MS,
        .heartbeatIntervalMs = PRTN_ESPNOW_HEARTBEAT_INTERVAL_MS,
        .staleTimeoutMs      = 5000,
        .offlineTimeoutMs    = 15000,
        .ackTimeoutMs        = 300,
        .maxRetries          = 3,
    };

    static EspNowEchoController::Config espNowEchoConfig {
        .localNodeId = PRTN_NODE_ID,
        .localNodeName = PRTN_NODE_NAME,
        .enableSender = PRTN_ESPNOW_ECHO_ENABLE_SENDER != 0,
        .enableReceiver = PRTN_ESPNOW_ECHO_ENABLE_RECEIVER != 0,
        .useAck = PRTN_ESPNOW_ECHO_USE_ACK != 0,
        .verboseLog = PRTN_ESPNOW_ECHO_VERBOSE_LOG != 0,
        .statsLog = PRTN_ESPNOW_ECHO_STATS_LOG != 0,
        .payloadLen = PRTN_ESPNOW_ECHO_PAYLOAD_LEN,
        .sendIntervalMs = PRTN_ESPNOW_ECHO_SEND_INTERVAL_MS,
        .statsLogIntervalMs = PRTN_ESPNOW_ECHO_STATS_LOG_INTERVAL_MS,
        .activityLedWindowMs = PRTN_ESPNOW_ECHO_ACTIVITY_LED_WINDOW_MS,
        .activityLedIntervalMs = PRTN_ESPNOW_ECHO_ACTIVITY_LED_INTERVAL_MS,
    };

    static EspNowEchoController espNowEchoController(EspNowNode::instance(), ledAux, console, espNowEchoConfig);
    static NodeController       nodeController(nodeInfo, console, wifi, espNowConfig, espNowEchoController, nodeControllerConfig);
    static HeartbeatController  heartbeatController(nodeInfo, ledMain, console);
} // namespace

void PrtnApp::setup() {
    console.setup();
    console.printBootBanner(nodeInfo);

    const bool nodeControllerReady = nodeController.setup();
    if (!nodeControllerReady) {
        console.error("failed to setup node controller");
    }

    console.printState(nodeInfo.getNodeState());

    const bool heartbeatReady = heartbeatController.setup();
    if (!heartbeatReady) {
        console.error("failed to setup heartbeat controller");
    }

    if (nodeControllerReady && !PrtnTasks::startNode(nodeController, nodeInfo, console)) {
        console.error("failed to start node task");
    }

    if (heartbeatReady && !PrtnTasks::startHeartbeat(heartbeatController, nodeInfo, console)) {
        console.error("failed to start heartbeat task");
    }
}

void PrtnApp::idle() {
    delay(PRTN_LOOP_INTERVAL_MS);
}
