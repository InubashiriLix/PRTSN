#include "src/ctl/inc/EspNowEchoController.h"

#include <cstring>

EspNowEchoController::EspNowEchoController(EspNowNode&   espNowNode,
                                           LED&          ledAux,
                                           SerialConsole& console,
                                           const Config& config)
    : m_espNowNode(espNowNode),
      m_ledAux(ledAux),
      m_console(console),
      m_config(config) {}

bool EspNowEchoController::setup() {
    m_ledAux.setup();
    return true;
}

void EspNowEchoController::update() {
    m_espNowNode.update();

    EspNowNode::Event event;
    while (m_espNowNode.popEvent(event)) {
        handleEspNowEvent(event);
    }

    const uint32_t nowMs = millis();

    updateAuxLed();
    logEspNowStats();

    if (!m_config.enableSender) {
        return;
    }

    if (nowMs - m_lastEspNowSendMs < m_config.sendIntervalMs) {
        return;
    }

    EspNowNode::Node* target = currentEspNowTarget();
    if (target == nullptr || target->nodeId[0] == '\0') {
        ++m_espNowNoTargetCount;
        return;
    }

    uint8_t payload[EspNowProtocol::MaxPayloadLen] {};
    const size_t payloadLen = m_config.payloadLen <= sizeof(payload) ? m_config.payloadLen : sizeof(payload);
    fillEspNowTestPayload(payload, payloadLen);

    if (m_espNowNode.sendCommand(*target, payload, payloadLen, m_config.useAck)) {
        m_lastEspNowSendMs = nowMs;
        ++m_espNowTxCommandCount;
        markEspNowActivity();
        if (m_config.verboseLog) {
            m_console.log(
                "esp-now command sent target=%s id=%s payloadLen=%u tx=%u",
                espNowNodeDisplayName(*target),
                target->nodeId,
                static_cast<unsigned>(payloadLen),
                static_cast<unsigned>(m_espNowTxCounter));
            logEspNowPayload("esp-now tx", payload, payloadLen);
        }
    } else {
        ++m_espNowTxFailCount;
    }
}

void EspNowEchoController::handleEspNowEvent(const EspNowNode::Event& event) {
    markEspNowActivity();

    switch (event.type) {
        case EspNowNode::EventType::COMMAND_RECEIVED:
            if (!m_config.enableReceiver) {
                break;
            }

            ++m_espNowRxCommandCount;
            if (m_config.verboseLog) {
                m_console.log(
                    "esp-now command received from=%s id=%s payloadLen=%u seq=%u",
                    espNowEventDisplayName(event),
                    event.nodeId,
                    static_cast<unsigned>(event.payloadLen),
                    static_cast<unsigned>(event.seq));
                logEspNowPayload("esp-now rx command", event.payload, event.payloadLen);
            }

            if (event.nodeId[0] != '\0' &&
                m_espNowNode.sendCommandResult(event.nodeId, event.payload, event.payloadLen, m_config.useAck)) {
                ++m_espNowTxEchoCount;
                markEspNowActivity();
                if (m_config.verboseLog) {
                    m_console.log("esp-now command result echoed to=%s id=%s", espNowEventDisplayName(event), event.nodeId);
                }
            } else {
                ++m_espNowEchoFailCount;
            }
            break;

        case EspNowNode::EventType::COMMAND_RESULT_RECEIVED:
            ++m_espNowRxResultCount;
            if (m_config.verboseLog) {
                m_console.log(
                    "esp-now command result received from=%s id=%s payloadLen=%u seq=%u",
                    espNowEventDisplayName(event),
                    event.nodeId,
                    static_cast<unsigned>(event.payloadLen),
                    static_cast<unsigned>(event.seq));
                logEspNowPayload("esp-now rx result", event.payload, event.payloadLen);
            }
            break;

        case EspNowNode::EventType::DATA_RECEIVED:
            if (m_config.verboseLog) {
                m_console.log(
                    "esp-now data received from=%s id=%s payloadLen=%u seq=%u",
                    espNowEventDisplayName(event),
                    event.nodeId,
                    static_cast<unsigned>(event.payloadLen),
                    static_cast<unsigned>(event.seq));
                logEspNowPayload("esp-now rx data", event.payload, event.payloadLen);
            }
            break;
    }
}

EspNowNode::Node* EspNowEchoController::findSendableEspNowNode() {
    const char* localNodeId = m_config.localNodeId != nullptr ? m_config.localNodeId : "";

    for (size_t i = 0; i < m_espNowNode.nodeCount(); ++i) {
        const EspNowNode::Node* node = m_espNowNode.nodeAt(i);
        if (node != nullptr &&
            node->state != EspNowNode::NodeState::OFFLINE &&
            node->state != EspNowNode::NodeState::ERROR &&
            node->peerRegistered &&
            strncmp(node->nodeId, localNodeId, EspNowNode::NodeIdLen) != 0) {
            return m_espNowNode.findNodeByMac(node->peer.mac);
        }
    }

    return nullptr;
}

EspNowNode::Node* EspNowEchoController::currentEspNowTarget() {
    if (m_hasEspNowTarget) {
        EspNowNode::Node* target = m_espNowNode.findNodeByMac(m_espNowTargetMac);
        if (target != nullptr &&
            target->peerRegistered &&
            target->state != EspNowNode::NodeState::OFFLINE &&
            target->state != EspNowNode::NodeState::ERROR) {
            return target;
        }

        m_hasEspNowTarget = false;
        ++m_espNowTargetResetCount;
    }

    EspNowNode::Node* target = findSendableEspNowNode();
    if (target == nullptr) {
        return nullptr;
    }

    std::memcpy(m_espNowTargetMac, target->peer.mac, sizeof(m_espNowTargetMac));
    m_hasEspNowTarget = true;
    return target;
}

void EspNowEchoController::fillEspNowTestPayload(uint8_t* payload, size_t len) {
    if (payload == nullptr) {
        return;
    }

    ++m_espNowTxCounter;
    for (size_t i = 0; i < len; ++i) {
        payload[i] = static_cast<uint8_t>((m_espNowTxCounter + i) & 0xFF);
    }
}

void EspNowEchoController::logEspNowPayload(const char* prefix, const uint8_t* payload, size_t len) {
    if (payload == nullptr) {
        return;
    }

    m_console.printf("[log] %s:", prefix);
    for (size_t i = 0; i < len; ++i) {
        m_console.printf(" %02X", payload[i]);
    }
    m_console.println();
}

void EspNowEchoController::logEspNowStats() {
    if (!m_config.statsLog) {
        return;
    }

    const uint32_t nowMs = millis();
    if (nowMs - m_lastEspNowStatsMs < m_config.statsLogIntervalMs) {
        return;
    }

    const uint32_t elapsedMs = m_lastEspNowStatsMs == 0
                                   ? m_config.statsLogIntervalMs
                                   : nowMs - m_lastEspNowStatsMs;
    const uint32_t txDelta = m_espNowTxCommandCount - m_lastEspNowStatsTxCount;
    const uint32_t rxCmdDelta = m_espNowRxCommandCount - m_lastEspNowStatsRxCommandCount;
    const uint32_t rxResultDelta = m_espNowRxResultCount - m_lastEspNowStatsRxResultCount;
    const uint32_t echoDelta = m_espNowTxEchoCount - m_lastEspNowStatsEchoCount;
    const uint32_t txHz = elapsedMs > 0 ? (txDelta * 1000U) / elapsedMs : 0;
    const uint32_t rxCmdHz = elapsedMs > 0 ? (rxCmdDelta * 1000U) / elapsedMs : 0;
    const uint32_t rxResultHz = elapsedMs > 0 ? (rxResultDelta * 1000U) / elapsedMs : 0;
    const uint32_t echoHz = elapsedMs > 0 ? (echoDelta * 1000U) / elapsedMs : 0;

    EspNowNode::Node* target = currentEspNowTarget();

    m_console.log(
        "espnow %s>%s st=%c n=%u tx=%lu/s rr=%lu/s rc=%lu/s ec=%lu/s ft=%lu fe=%lu nt=%lu tr=%lu",
        m_config.localNodeName != nullptr ? m_config.localNodeName : "-",
        target != nullptr ? espNowNodeDisplayName(*target) : "-",
        target != nullptr ? espNowNodeStateCode(*target) : '-',
        static_cast<unsigned>(m_espNowNode.nodeCount()),
        static_cast<unsigned long>(txHz),
        static_cast<unsigned long>(rxResultHz),
        static_cast<unsigned long>(rxCmdHz),
        static_cast<unsigned long>(echoHz),
        static_cast<unsigned long>(m_espNowTxFailCount),
        static_cast<unsigned long>(m_espNowEchoFailCount),
        static_cast<unsigned long>(m_espNowNoTargetCount),
        static_cast<unsigned long>(m_espNowTargetResetCount));

    m_lastEspNowStatsMs = nowMs;
    m_lastEspNowStatsTxCount = m_espNowTxCommandCount;
    m_lastEspNowStatsRxCommandCount = m_espNowRxCommandCount;
    m_lastEspNowStatsRxResultCount = m_espNowRxResultCount;
    m_lastEspNowStatsEchoCount = m_espNowTxEchoCount;
}

const char* EspNowEchoController::espNowNodeDisplayName(const EspNowNode::Node& node) const {
    return node.nodeName[0] != '\0' ? node.nodeName : node.nodeId;
}

const char* EspNowEchoController::espNowEventDisplayName(const EspNowNode::Event& event) const {
    return event.nodeName[0] != '\0' ? event.nodeName : event.nodeId;
}

char EspNowEchoController::espNowNodeStateCode(const EspNowNode::Node& node) const {
    switch (node.state) {
        case EspNowNode::NodeState::UNKNOWN:
            return 'U';
        case EspNowNode::NodeState::DISCOVERED:
            return 'D';
        case EspNowNode::NodeState::ONLINE:
            return 'O';
        case EspNowNode::NodeState::STALE:
            return 'S';
        case EspNowNode::NodeState::OFFLINE:
            return 'F';
        case EspNowNode::NodeState::ERROR:
            return 'E';
    }

    return '?';
}

void EspNowEchoController::markEspNowActivity() {
    m_lastEspNowActivityMs = millis();
}

void EspNowEchoController::updateAuxLed() {
    const uint32_t nowMs = millis();
    const bool espNowActive = nowMs - m_lastEspNowActivityMs <= m_config.activityLedWindowMs;

    m_ledAux.updateByIntervalMs(
        espNowActive ? m_config.activityLedIntervalMs : 1000);
}
