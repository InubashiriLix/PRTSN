#pragma once

#include "../../cfg/BuildConfig.h"
#include "../../dvc/inc/LED.h"
#include "../../dvc/inc/SerialConsole.h"
#include "../../dom/NodeInfo.h"
#include "../../svc/inc/wifi.h"

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class NodeController
{
private:
    NodeInfo& m_nodeInfo;

    // dvc:
    LED&           m_ledAux;
    SerialConsole& m_console;
    Wifi&          m_wifi;

    // freertos:
    TaskHandle_t m_taskHandle = nullptr;

    constexpr static TickType_t  m_period             = pdMS_TO_TICKS(PRTN_LOOP_INTERVAL_MS);
    constexpr static uint32_t    m_printLogIntervalMs = 1000;
    constexpr static uint32_t    m_stack              = 1024 * 4;
    constexpr static UBaseType_t m_priority           = 4;

public:
    NodeController(NodeInfo& nodeInfo, LED& ledAux, SerialConsole& console, Wifi& wifi);

    // standard setup and loop methods using freertos tasks:
    bool        setup();
    bool        createTask();
    static void taskEntry(void* arg);
    void        printConsoleLog(uint32_t& lastWifiLogMs, uint32_t intervalMs);
    void        ControlLoop();
};
