#include "../inc/NodeController.h"
#include "../../svc/inc/wifi.h"

NodeController::NodeController(NodeInfo&      nodeInfo,
                               LED&           ledAux,
                               SerialConsole& console,
                               Wifi&          wifi,
                               MqttServer&    mqttServer)
    : m_nodeInfo(nodeInfo),
      m_ledAux(ledAux),
      m_console(console),
      m_wifi(wifi),
      m_mqttServer(mqttServer) {}

bool NodeController::setup() {
    // ================== SETUP DEVICES / SERVICES ===================
    m_ledAux.setup();

    if (!m_wifi.begin()) {
        m_nodeInfo.updateNodeState(ERROR);
        m_console.error("failed to start wifi");
        return false;
    }

    m_mqttServer.setMessageHandler(
        [this](const char* topic, const uint8_t* payload, size_t payloadLen) {
            mqttServerMsgHandler(topic, payload, payloadLen);
        });

    m_mqttServer.setEventHandler(
        [this](const MqttServer::Event& event) {
            mqttServerEventHandler(event);
        });

    if (!m_mqttServer.begin()) {
        m_nodeInfo.updateNodeState(ERROR);
        m_console.error("failed to start mqttServer");
        return false;
    }
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
        m_mqttServer.update();
        m_ledAux.updateByIntervalMs(1000);
        printConsoleLog(lastWifiLogMs, m_printLogIntervalMs);

        vTaskDelayUntil(&taskLastWakeTime, m_period);
    }
}

void NodeController::mqttServerMsgHandler(const char* topic, const uint8_t* payload, size_t payloadLen) {
    m_console.log(
        "received mqtt message topic=%s payload=%.*s",
        topic,
        static_cast<int>(payloadLen),
        reinterpret_cast<const char*>(payload));
}

void NodeController::mqttServerEventHandler(const MqttServer::Event& event) {
    switch (event.type) {
        case MqttServer::EventType::Started:
            m_console.log(
                "mqtt server started port=%u maxClients=%u",
                event.port,
                event.maxClients);
            break;

        case MqttServer::EventType::Stopped:
            m_console.log("mqtt server stopped");
            break;

        case MqttServer::EventType::TcpClientAccepted:
#if PRTN_MQTT_SERVER_VERBOSE_LOG
            m_console.log("mqtt tcp client accepted slot=%u", event.clientIndex);
#endif
            break;

        case MqttServer::EventType::TcpClientRejected:
            m_console.error("mqtt tcp client rejected: no free client slot");
            break;

        case MqttServer::EventType::ClientConnected:
            m_console.log(
                "mqtt client connected slot=%u clientId=%s clients=%u",
                event.clientIndex,
                event.clientId,
                event.mqttClients);
            break;

        case MqttServer::EventType::ClientDisconnected:
            m_console.log(
                "mqtt client disconnected slot=%u clientId=%s reason=%s clients=%u",
                event.clientIndex,
                event.clientId,
                mqttDisconnectReasonName(event.reason),
                event.mqttClients);
            break;

        case MqttServer::EventType::Subscribe:
#if PRTN_MQTT_SERVER_VERBOSE_LOG
            m_console.log(
                "mqtt client subscribed slot=%u clientId=%s topic=%s subs=%u",
                event.clientIndex,
                event.clientId,
                event.topic,
                event.activeSubscriptions);
#endif
            break;

        case MqttServer::EventType::Publish:
#if PRTN_MQTT_SERVER_VERBOSE_LOG
            m_console.log(
                "mqtt publish received slot=%u clientId=%s topic=%s payloadLen=%u",
                event.clientIndex,
                event.clientId,
                event.topic,
                static_cast<unsigned>(event.payloadLen));
#endif
            break;
    }
}

const char* NodeController::mqttDisconnectReasonName(MqttServer::DisconnectReason reason) const {
    switch (reason) {
        case MqttServer::DisconnectReason::None:
            return "none";
        case MqttServer::DisconnectReason::TcpClosed:
            return "tcp_closed";
        case MqttServer::DisconnectReason::ClientRequested:
            return "client_requested";
        case MqttServer::DisconnectReason::ProtocolError:
            return "protocol_error";
        case MqttServer::DisconnectReason::KeepAliveTimeout:
            return "keep_alive_timeout";
        case MqttServer::DisconnectReason::ServerStopped:
            return "server_stopped";
        case MqttServer::DisconnectReason::Rejected:
            return "rejected";
    }

    return "unknown";
}
