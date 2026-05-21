#pragma once

#include "src/ctl/inc/EspNowEchoController.h"
#include "src/dom/NodeInfo.h"
#include "src/dvc/inc/SerialConsole.h"
#include "src/svc/inc/wifi.h"

#include <Arduino.h>
#include <cstdint>

class NodeController
{
public:
    struct Config
    {
        bool     updateWifi = false;
        bool     logWifiStatus = false;
        uint32_t wifiUpdateIntervalMs = 400;
        uint32_t wifiStatusLogIntervalMs = 1000;
    };

private:
    NodeInfo& m_nodeInfo;

    SerialConsole& m_console;
    Wifi&          m_wifi;

    EspNowEchoController& m_espNowEchoController;
    EspNowNode::Config& m_espNowNodeConfig;
    Config m_config;

    uint32_t m_lastWifiLogMs = 0;

public:
    NodeController(NodeInfo&              nodeInfo,
                   SerialConsole&         console,
                   Wifi&                  wifi,
                   EspNowNode::Config&    espNowNodeConfig,
                   EspNowEchoController&  espNowEchoController,
                   const Config&          config);

    bool setup();
    void update();

private:
    void printConsoleLog(uint32_t intervalMs);
};
