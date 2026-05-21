#include "src/tsk/inc/PrtnTasks.h"

#include "src/cfg/BuildConfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace
{
    constexpr TickType_t  NODE_PERIOD_TICKS      = pdMS_TO_TICKS(PRTN_LOOP_INTERVAL_MS);
    constexpr uint32_t    NODE_STACK_WORDS       = 1024 * 4;
    constexpr UBaseType_t NODE_PRIORITY          = 4;
    constexpr TickType_t  HEARTBEAT_PERIOD_TICKS = pdMS_TO_TICKS(PRTN_HEARTBEAT_INTERVAL_MS);
    constexpr uint32_t    HEARTBEAT_STACK_WORDS  = 1024 * 3;
    constexpr UBaseType_t HEARTBEAT_PRIORITY     = 1;

    TaskHandle_t s_nodeTaskHandle = nullptr;
    TaskHandle_t s_heartbeatTaskHandle = nullptr;

    void nodeTaskEntry(void* arg) {
        auto* controller = static_cast<NodeController*>(arg);
        TickType_t taskLastWakeTime = xTaskGetTickCount();

        for (;;) {
            controller->update();
            vTaskDelayUntil(&taskLastWakeTime, NODE_PERIOD_TICKS);
        }
    }

    void heartbeatTaskEntry(void* arg) {
        auto* controller = static_cast<HeartbeatController*>(arg);
        TickType_t taskLastWakeTime = xTaskGetTickCount();

        for (;;) {
            controller->update();
            vTaskDelayUntil(&taskLastWakeTime, HEARTBEAT_PERIOD_TICKS);
        }
    }
} // namespace

bool PrtnTasks::startNode(NodeController& controller, NodeInfo& nodeInfo, SerialConsole& console) {
    if (s_nodeTaskHandle != nullptr) {
        return true;
    }

    const BaseType_t ok = xTaskCreate(
        nodeTaskEntry,
        "node_ctrl",
        NODE_STACK_WORDS,
        &controller,
        NODE_PRIORITY,
        &s_nodeTaskHandle);

    if (ok != pdPASS) {
        s_nodeTaskHandle = nullptr;
        nodeInfo.updateNodeState(ERROR);
        console.error("failed to create node controller task");
        return false;
    }

    return true;
}

bool PrtnTasks::startHeartbeat(HeartbeatController& controller, NodeInfo& nodeInfo, SerialConsole& console) {
    if (s_heartbeatTaskHandle != nullptr) {
        return true;
    }

    const BaseType_t ok = xTaskCreate(
        heartbeatTaskEntry,
        "node_heartbeat",
        HEARTBEAT_STACK_WORDS,
        &controller,
        HEARTBEAT_PRIORITY,
        &s_heartbeatTaskHandle);

    if (ok != pdPASS) {
        s_heartbeatTaskHandle = nullptr;
        nodeInfo.updateNodeState(ERROR);
        console.error("failed to create heartbeat task");
        return false;
    }

    return true;
}
