#pragma once

#include "src/dvc/inc/LED.h"
#include "src/dvc/inc/SerialConsole.h"
#include "src/svc/inc/EspNowNode.h"

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

class EspNowEchoController
{
public:
    struct Config
    {
        const char* localNodeId = nullptr;
        const char* localNodeName = nullptr;

        bool enableSender = true;
        bool enableReceiver = true;
        bool useAck = false;
        bool verboseLog = false;
        bool statsLog = true;

        size_t payloadLen = 30;
        uint32_t sendIntervalMs = 20;
        uint32_t statsLogIntervalMs = 1000;
        uint32_t activityLedWindowMs = 1200;
        uint32_t activityLedIntervalMs = 60;
    };

private:
    EspNowNode&    m_espNowNode;
    LED&           m_ledAux;
    SerialConsole& m_console;
    Config         m_config;

    uint32_t m_lastEspNowSendMs      = 0;
    uint32_t m_lastEspNowActivityMs  = 0;
    uint32_t m_lastEspNowStatsMs     = 0;
    uint16_t m_espNowTxCounter       = 0;
    uint32_t m_espNowTxCommandCount  = 0;
    uint32_t m_espNowTxFailCount     = 0;
    uint32_t m_espNowNoTargetCount   = 0;
    uint32_t m_espNowRxCommandCount  = 0;
    uint32_t m_espNowRxResultCount   = 0;
    uint32_t m_espNowTxEchoCount     = 0;
    uint32_t m_espNowEchoFailCount   = 0;
    uint32_t m_espNowTargetResetCount = 0;
    uint32_t m_lastEspNowStatsTxCount = 0;
    uint32_t m_lastEspNowStatsRxCommandCount = 0;
    uint32_t m_lastEspNowStatsRxResultCount = 0;
    uint32_t m_lastEspNowStatsEchoCount = 0;

    uint8_t m_espNowTargetMac[ESP_NOW_ETH_ALEN] {};
    bool    m_hasEspNowTarget = false;

public:
    EspNowEchoController(EspNowNode& espNowNode, LED& ledAux, SerialConsole& console, const Config& config);

    bool setup();
    void update();

private:
    void                    handleEspNowEvent(const EspNowNode::Event& event);
    EspNowNode::Node*       findSendableEspNowNode();
    EspNowNode::Node*       currentEspNowTarget();
    void                    fillEspNowTestPayload(uint8_t* payload, size_t len);
    void                    logEspNowPayload(const char* prefix, const uint8_t* payload, size_t len);
    void                    logEspNowStats();
    const char*             espNowNodeDisplayName(const EspNowNode::Node& node) const;
    const char*             espNowEventDisplayName(const EspNowNode::Event& event) const;
    char                    espNowNodeStateCode(const EspNowNode::Node& node) const;
    void                    markEspNowActivity();
    void                    updateAuxLed();
};
