#pragma once

#include "src/dvc/inc/LED.h"
#include "src/svc/inc/EspNowNode.h"
#include "src/svc/inc/SerialConsoleService.h"

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

class EspNowEchoController
{
public:
    static constexpr size_t   DefaultPayloadLen            = 30;
    static constexpr uint32_t DefaultSendIntervalMs        = 20;
    static constexpr uint32_t DefaultStatsLogIntervalMs    = 1000;
    static constexpr uint32_t DefaultActivityLedWindowMs   = 1200;
    static constexpr uint32_t DefaultActivityLedIntervalMs = 60;
    static constexpr bool     DefaultUseAck                = false;
    static constexpr bool     DefaultVerboseLog            = false;
    static constexpr bool     DefaultStatsLog              = false;

    struct Config
    {
        const char* localNodeId   = nullptr;
        const char* localNodeName = nullptr;

        bool enableSender   = true;
        bool enableReceiver = true;
        bool useAck         = DefaultUseAck;
        bool verboseLog     = DefaultVerboseLog;
        bool statsLog       = DefaultStatsLog;

        size_t   payloadLen            = DefaultPayloadLen;
        uint32_t sendIntervalMs        = DefaultSendIntervalMs;
        uint32_t statsLogIntervalMs    = DefaultStatsLogIntervalMs;
        uint32_t activityLedWindowMs   = DefaultActivityLedWindowMs;
        uint32_t activityLedIntervalMs = DefaultActivityLedIntervalMs;
    };

private:
    EspNowNode&           m_espNowNode;
    LED&                  m_ledMain;
    LED&                  m_ledAux;
    SerialConsoleService& m_console;
    Config                m_config;

    uint32_t m_lastEspNowSendMs              = 0;
    uint32_t m_lastEspNowActivityMs          = 0;
    uint32_t m_lastEspNowStatsMs             = 0;
    uint16_t m_espNowTxCounter               = 0;
    uint32_t m_espNowTxCommandCount          = 0;
    uint32_t m_espNowTxFailCount             = 0;
    uint32_t m_espNowNoTargetCount           = 0;
    uint32_t m_espNowRxCommandCount          = 0;
    uint32_t m_espNowRxResultCount           = 0;
    uint32_t m_espNowTxEchoCount             = 0;
    uint32_t m_espNowEchoFailCount           = 0;
    uint32_t m_espNowTargetResetCount        = 0;
    uint32_t m_lastEspNowStatsTxCount        = 0;
    uint32_t m_lastEspNowStatsRxCommandCount = 0;
    uint32_t m_lastEspNowStatsRxResultCount  = 0;
    uint32_t m_lastEspNowStatsEchoCount      = 0;

    uint8_t m_espNowTargetMac[ESP_NOW_ETH_ALEN] {};
    bool    m_hasEspNowTarget = false;

public:
    EspNowEchoController(EspNowNode&           espNowNode,
                         LED&                  ledMain,
                         LED&                  ledAux,
                         SerialConsoleService& console,
                         const Config&         config);

    bool setup();
    void update();

private:
    void              handleEspNowEvent(const EspNowNode::Event& event);
    EspNowNode::Node* findSendableEspNowNode();
    EspNowNode::Node* currentEspNowTarget();
    void              fillEspNowTestPayload(uint8_t* payload, size_t len);
    void              logEspNowPayload(const char* prefix, const uint8_t* payload, size_t len);
    void              logEspNowStats(bool verbose = DefaultStatsLog, bool force = false);
    const char*       espNowNodeDisplayName(const EspNowNode::Node& node) const;
    const char*       espNowEventDisplayName(const EspNowNode::Event& event) const;
    char              espNowNodeStateCode(const EspNowNode::Node& node) const;
    void              markEspNowActivity();
    void              updateLed();

    static void cmdEspNowHandler(SerialConsoleService& service, const char* args, void* context);
};
