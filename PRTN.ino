#include "src/cfg/BoardConfig.h"
#include "src/cfg/BuildConfig.h"
#include "src/ctl/inc/HeartbeatController.h"
#include "src/ctl/inc/NodeController.h"
#include "src/dom/NodeInfo.h"
#include "src/dvc/inc/LED.h"
#include "src/dvc/inc/SerialConsole.h"
#include "src/svc/inc/Mqtt.h"
#include "src/svc/inc/MqttClient.h"
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
        .sta  = {
            .ssid        = PRTN_WIFI_STA_SSID,
            .password    = PRTN_WIFI_STA_PASSWORD,
            .hostname    = PRTN_WIFI_STA_HOSTNAME,
            .reconnectMs = PRTN_WIFI_STA_RECONNECT_MS},
        .ap = {}};

    static Wifi wifi(wifiConfig);

    static MqttClientConfig mqttClientConfig {
        .host                = PRTN_MQTT_CLIENT_HOST,
        .port                = PRTN_MQTT_CLIENT_PORT,
        .clientId            = PRTN_MQTT_CLIENT_ID,
        .username            = PRTN_MQTT_CLIENT_USERNAME,
        .password            = PRTN_MQTT_CLIENT_PASSWORD,
        .keepAliveSec        = PRTN_MQTT_CLIENT_KEEP_ALIVE_SEC,
        .cleanSession        = true,
        .maxPacketSize       = PRTN_MQTT_CLIENT_MAX_PACKET_SIZE,
        .reconnectIntervalMs = PRTN_MQTT_CLIENT_RECONNECT_MS,
    };

    static MqttClient mqttClient(mqttClientConfig);

    static NodeController      nodeController(nodeInfo, ledAux, console, wifi, mqttClient);
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
