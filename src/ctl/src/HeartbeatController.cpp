#include "../inc/HeartbeatController.h"

HeartbeatController::HeartbeatController(NodeInfo& nodeInfo, LED& ledMain, SerialConsole& console)
    : m_nodeInfo(nodeInfo),
      m_ledMain(ledMain),
      m_console(console) {}

bool HeartbeatController::setup() {
    m_ledMain.setup();
    return createTask();
}

bool HeartbeatController::createTask() {
    if (m_taskHandle != nullptr) {
        return true;
    }

    const BaseType_t ok = xTaskCreate(
        taskEntry,
        "node_heartbeat",
        m_stack,
        this,
        m_priority,
        &m_taskHandle);

    if (ok != pdPASS) {
        m_taskHandle = nullptr;
        m_nodeInfo.updateNodeState(ERROR);
        m_console.error("failed to create heartbeat task");
        return false;
    }

    return true;
}

void HeartbeatController::taskEntry(void* arg) {
    auto* self = static_cast<HeartbeatController*>(arg);
    self->HeartbeatLoop();
}

void HeartbeatController::HeartbeatLoop() {
    TickType_t taskLastWakeTime = xTaskGetTickCount();

    for (;;) {
        m_ledMain.update();
        m_console.printHeartbeat(m_nodeInfo);
        vTaskDelayUntil(&taskLastWakeTime, m_period);
    }
}
