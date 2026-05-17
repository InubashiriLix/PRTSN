#pragma once

#include "../../cfg/BuildConfig.h"
#include "../../dvc/inc/LED.h"
#include "../../dvc/inc/SerialConsole.h"
#include "../../dom/NodeInfo.h"

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class HeartbeatController
{
private:
    NodeInfo&      m_nodeInfo;
    SerialConsole& m_console;
    LED&           m_ledMain;

    TaskHandle_t m_taskHandle = nullptr;

    constexpr static TickType_t  m_period   = pdMS_TO_TICKS(PRTN_HEARTBEAT_INTERVAL_MS);
    constexpr static uint32_t    m_stack    = 1024 * 3;
    constexpr static UBaseType_t m_priority = 1;

public:
    HeartbeatController(NodeInfo& nodeInfo, LED& ledMain, SerialConsole& console);

    bool        setup();
    bool        createTask();
    static void taskEntry(void* arg);
    void        HeartbeatLoop();
};
