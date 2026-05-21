#include "../inc/HeartbeatController.h"

HeartbeatController::HeartbeatController(NodeInfo& nodeInfo, LED& ledMain, SerialConsole& console)
    : m_nodeInfo(nodeInfo),
      m_ledMain(ledMain),
      m_console(console) {}

bool HeartbeatController::setup() {
    m_ledMain.setup();
    return true;
}

void HeartbeatController::update() {
    m_ledMain.update();
    m_console.printHeartbeat(m_nodeInfo);
}
