#include "../inc/NodeController.h"
#include "../../svc/inc/wifi.h"

NodeController::NodeController(NodeInfo&      nodeInfo,
                               LED&           ledAux,
                               SerialConsole& console,
                               Wifi&          wifi,
                               MqttClient&    mqttClient)
    : m_nodeInfo(nodeInfo),
      m_ledAux(ledAux),
      m_console(console),
      m_wifi(wifi),
      m_mqttClient(mqttClient) {}

bool NodeController::setup() {
    // ================== SETUP DEVICES / SERVICES ===================
    m_ledAux.setup();

    if (!m_wifi.begin()) {
        m_nodeInfo.updateNodeState(ERROR);
        m_console.error("failed to start wifi");
        return false;
    }

    m_mqttClient.setMessageHandler(
        [this](const char* topic, const uint8_t* payload, size_t payloadLen) {
            mqttClientMsgHandler(topic, payload, payloadLen);
        });

    m_mqttClient.setEventHandler(
        [this](const MqttClient::Event& event) {
            mqttClientEventHandler(event);
        });
    // ================ SETUP DEVICES / SERVICES END =================

    // ================ CREATE FREERTOS TASK ==================
    // note: if you;re not sure what you are doing with freertos task,
    // DO NOT EDIT THIS PART!!!
    if (!createTask()) {
        m_nodeInfo.updateNodeState(ERROR);
        m_console.error("failed to create node controller task");
        return false;
    }
    // ============= CREATE FREERTOS TASK END =================

    m_nodeInfo.updateNodeState(RUNNING);
    return true;
}

void NodeController::taskEntry(void* arg) {
    auto* self = static_cast<NodeController*>(arg);
    self->ControlLoop();
}

bool NodeController::createTask() {
    if (m_taskHandle != nullptr) {
        return true;
    }

    const BaseType_t ok = xTaskCreate(
        taskEntry,
        "node_ctrl",
        m_stack,
        this,
        m_priority,
        &m_taskHandle);

    if (ok != pdPASS) {
        m_taskHandle = nullptr;
        return false;
    }

    return true;
}

void NodeController::printConsoleLog(uint32_t& lastWifiLogMs, uint32_t intervalMs) {
    const uint32_t nowMs = millis();
    if (nowMs - lastWifiLogMs >= intervalMs) {
        const Wifi::Status wifiStatus = m_wifi.status();

        if (wifiStatus.ap.enabled) {
            const String apIp = wifiStatus.ap.ip.toString();
            m_console.log(
                "AP %s ip=%s ch=%u clients=%u/%u",
                wifiStatus.ap.running ? "ON" : "OFF",
                apIp.c_str(),
                wifiStatus.ap.channel,
                wifiStatus.ap.clientCount,
                wifiStatus.ap.maxClients);
        }

        if (wifiStatus.sta.enabled) {
            const String staIp = wifiStatus.sta.ip.toString();
            m_console.log(
                "STA %s ip=%s rssi=%ld dBm quality=%u%% status=%d",
                wifiStatus.sta.connected ? "ON" : "OFF",
                staIp.c_str(),
                wifiStatus.sta.rssi,
                wifiStatus.sta.quality,
                wifiStatus.sta.statusCode);
        }

        lastWifiLogMs = nowMs;
    }
}

void NodeController::ControlLoop() {
    TickType_t taskLastWakeTime = xTaskGetTickCount();
    uint32_t   lastWifiLogMs    = 0;

    for (;;) {
        m_wifi.updateByIntervalMs(400);
        updateMqttClientTest();
        m_ledAux.updateByIntervalMs(1000);
        printConsoleLog(lastWifiLogMs, m_printLogIntervalMs);

        vTaskDelayUntil(&taskLastWakeTime, m_period);
    }
}

void NodeController::mqttClientMsgHandler(const char* topic, const uint8_t* payload, size_t payloadLen) {
    m_console.log(
        "mqtt client message topic=%s payload=%.*s",
        topic,
        static_cast<int>(payloadLen),
        reinterpret_cast<const char*>(payload));
}

void NodeController::mqttClientEventHandler(const MqttClient::Event& event) {
    switch (event.type) {
        case MqttClient::EventType::ConnectAttempt:
#if PRTN_MQTT_CLIENT_VERBOSE_LOG
            m_console.log(
                "mqtt client connecting host=%s port=%u clientId=%s",
                event.host,
                event.port,
                event.clientId);
#endif
            break;

        case MqttClient::EventType::Connected:
            m_console.log(
                "mqtt client connected host=%s port=%u clientId=%s",
                event.host,
                event.port,
                event.clientId);
            break;

        case MqttClient::EventType::ConnectFailed:
            m_console.error(
                "mqtt client connect failed host=%s port=%u reason=%s",
                event.host,
                event.port,
                mqttClientDisconnectReasonName(event.reason));
            break;

        case MqttClient::EventType::Disconnected:
            m_mqttClientSubscribed = false;
            m_console.log(
                "mqtt client disconnected reason=%s",
                mqttClientDisconnectReasonName(event.reason));
            break;

        case MqttClient::EventType::SubscribeSent:
            m_console.log(
                "mqtt client subscribe sent topic=%s packetId=%u",
                event.topic,
                event.packetId);
            break;

        case MqttClient::EventType::PublishSent:
#if PRTN_MQTT_CLIENT_VERBOSE_LOG
            m_console.log(
                "mqtt client publish sent topic=%s payloadLen=%u",
                event.topic,
                static_cast<unsigned>(event.payloadLen));
#endif
            break;

        case MqttClient::EventType::PublishReceived:
            m_console.log(
                "mqtt client publish received topic=%s payloadLen=%u",
                event.topic,
                static_cast<unsigned>(event.payloadLen));
            break;

        case MqttClient::EventType::PingReqSent:
#if PRTN_MQTT_CLIENT_VERBOSE_LOG
            m_console.log("mqtt client pingreq sent");
#endif
            break;

        case MqttClient::EventType::PingRespReceived:
#if PRTN_MQTT_CLIENT_VERBOSE_LOG
            m_console.log("mqtt client pingresp received");
#endif
            break;

        case MqttClient::EventType::SubAckReceived:
            m_console.log(
                "mqtt client suback received packetId=%u",
                event.packetId);
            break;

        case MqttClient::EventType::PubAckReceived:
#if PRTN_MQTT_CLIENT_VERBOSE_LOG
            m_console.log(
                "mqtt client puback received packetId=%u",
                event.packetId);
#endif
            break;
    }
}

const char* NodeController::mqttClientDisconnectReasonName(MqttClient::DisconnectReason reason) const {
    switch (reason) {
        case MqttClient::DisconnectReason::None:
            return "none";
        case MqttClient::DisconnectReason::TcpConnectFailed:
            return "tcp_connect_failed";
        case MqttClient::DisconnectReason::ConnectPacketFailed:
            return "connect_packet_failed";
        case MqttClient::DisconnectReason::ConnAckRejected:
            return "connack_rejected";
        case MqttClient::DisconnectReason::PacketReadFailed:
            return "packet_read_failed";
        case MqttClient::DisconnectReason::KeepAliveTimeout:
            return "keep_alive_timeout";
        case MqttClient::DisconnectReason::BrokerRequested:
            return "broker_requested";
        case MqttClient::DisconnectReason::ClientRequested:
            return "client_requested";
        case MqttClient::DisconnectReason::LocalStop:
            return "local_stop";
    }

    return "unknown";
}

void NodeController::updateMqttClientTest() {
    const Wifi::Status wifiStatus = m_wifi.status();
    if (!wifiStatus.sta.connected) {
        m_mqttClientSubscribed = false;
        return;
    }

    m_mqttClient.update();

    if (!m_mqttClient.connected()) {
        m_mqttClientSubscribed = false;
        return;
    }

    if (!m_mqttClientSubscribed) {
        m_mqttClientSubscribed = m_mqttClient.subscribe(PRTN_MQTT_CLIENT_TEST_TOPIC);
        return;
    }

    const uint32_t nowMs = millis();
    if (nowMs - m_lastMqttClientPublishMs >= PRTN_MQTT_CLIENT_TEST_PUBLISH_MS) {
        if (m_mqttClient.publish(PRTN_MQTT_CLIENT_TEST_TOPIC, PRTN_MQTT_CLIENT_TEST_PAYLOAD)) {
            m_lastMqttClientPublishMs = nowMs;
        }
    }
}
