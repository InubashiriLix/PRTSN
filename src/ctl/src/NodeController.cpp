#include "src/ctl/inc/NodeController.h"

NodeController::NodeController(NodeInfo&             nodeInfo,
                               SerialConsole&        console,
                               Wifi&                 wifi,
                               EspNowNode::Config&   espNowNodeConfig,
                               EspNowEchoController& espNowEchoController,
                               const Config&         config)
    : m_nodeInfo(nodeInfo),
      m_console(console),
      m_wifi(wifi),
      m_espNowEchoController(espNowEchoController),
      m_espNowNodeConfig(espNowNodeConfig),
      m_config(config) {}

bool NodeController::setup() {
    if (!m_wifi.begin()) {
        m_nodeInfo.updateNodeState(ERROR);
        m_console.error("failed to start wifi");
        return false;
    }

    if (!EspNowNode::instance().setup(&m_wifi, m_espNowNodeConfig)) {
        m_nodeInfo.updateNodeState(ERROR);
        m_console.error("failed to start esp now node");
        return false;
    }

    if (!m_espNowEchoController.setup()) {
        m_nodeInfo.updateNodeState(ERROR);
        m_console.error("failed to setup esp now echo controller");
        return false;
    }

    m_nodeInfo.updateNodeState(RUNNING);
    return true;
}

void NodeController::update() {
    if (m_config.updateWifi) {
        m_wifi.updateByIntervalMs(m_config.wifiUpdateIntervalMs);
    }

    if (m_config.logWifiStatus) {
        printConsoleLog(m_config.wifiStatusLogIntervalMs);
    }

    m_espNowEchoController.update();
}

void NodeController::printConsoleLog(uint32_t intervalMs) {
    const uint32_t nowMs = millis();
    if (nowMs - m_lastWifiLogMs < intervalMs) {
        return;
    }

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

    m_lastWifiLogMs = nowMs;
}
