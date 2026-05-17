#include "src/cfg/BoardConfig.h"
#include "src/cfg/BuildConfig.h"
#include "src/ctl/inc/HeartbeatController.h"
#include "src/ctl/inc/NodeController.h"
#include "src/dom/NodeInfo.h"
#include "src/dvc/inc/LED.h"
#include "src/dvc/inc/SerialConsole.h"
#include "src/svc/inc/wifi.h"

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

    static Wifi::Config wifiConfig {
        .mode = PRTN_WIFI_MODE,
        .sta  = {},
        .ap   = {
            .ssid       = PRTN_WIFI_AP_SSID,
            .password   = PRTN_WIFI_AP_PASSWORD,
            .channel    = PRTN_WIFI_AP_CHANNEL,
            .hidden     = false,
            .maxClients = PRTN_WIFI_AP_MAX_CLIENTS_NUM},
    };

    static Wifi wifi(wifiConfig);

    static NodeController      nodeController(nodeInfo, ledAux, console, wifi);
    static HeartbeatController heartbeatController(nodeInfo, ledMain, console);
} // namespace

void setup() {
    console.setup();
    console.printBootBanner(nodeInfo);

    if (!nodeController.setup()) {
        console.error("failed to setup node controller");
    }

    console.printState(nodeInfo.getNodeState());

    if (!heartbeatController.setup()) {
        console.error("failed to setup heartbeat controller");
    }
}

void loop() {
    delay(PRTN_LOOP_INTERVAL_MS);
}
