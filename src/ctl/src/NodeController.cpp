#include "../inc/NodeController.h"
#include "../../svc/inc/wifi.h"

NodeController::NodeController(NodeInfo&      nodeInfo,
                               LED&           ledAux,
                               SerialConsole& console,
                               Wifi&          wifi)
    : m_nodeInfo(nodeInfo),
      m_ledAux(ledAux),
      m_console(console),
      m_wifi(wifi) {}

bool NodeController::setup() {
    m_ledAux.setup();

    if (!m_wifi.begin()) {
        m_nodeInfo.updateNodeState(ERROR);
        m_console.error("failed to start wifi");
        return false;
    }

    if (!createTask()) {
        m_nodeInfo.updateNodeState(ERROR);
        m_console.error("failed to create node controller task");
        return false;
    }

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

void NodeController::ControlLoop() {
    TickType_t taskLastWakeTime = xTaskGetTickCount();
    uint32_t   lastWifiLogMs    = 0;

    for (;;) {
        m_wifi.update();
        m_ledAux.updateByIntervalMs(1000);

        const uint32_t nowMs = millis();
        if (nowMs - lastWifiLogMs >= 1000) {
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

        vTaskDelayUntil(&taskLastWakeTime, m_period);
    }
}
