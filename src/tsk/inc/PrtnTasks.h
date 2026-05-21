#pragma once

#include "src/ctl/inc/HeartbeatController.h"
#include "src/ctl/inc/NodeController.h"
#include "src/dom/NodeInfo.h"
#include "src/dvc/inc/SerialConsole.h"

namespace PrtnTasks
{
    bool startNode(NodeController& controller, NodeInfo& nodeInfo, SerialConsole& console);
    bool startHeartbeat(HeartbeatController& controller, NodeInfo& nodeInfo, SerialConsole& console);
}
