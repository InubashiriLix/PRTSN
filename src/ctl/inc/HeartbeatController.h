#pragma once

#include "src/cfg/BuildConfig.h"
#include "src/dom/NodeInfo.h"
#include "src/dvc/inc/LED.h"
#include "src/dvc/inc/SerialConsole.h"

#include <Arduino.h>

class HeartbeatController
{
private:
    NodeInfo&      m_nodeInfo;
    SerialConsole& m_console;
    LED&           m_ledMain;

public:
    HeartbeatController(NodeInfo& nodeInfo, LED& ledMain, SerialConsole& console);

    bool setup();
    void update();
};
