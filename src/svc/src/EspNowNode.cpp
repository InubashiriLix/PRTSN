#include "src/svc/inc/EspNowNode.h"

#include <Arduino.h>
#include <cstring>

EspNowNode& EspNowNode::instance() {
    static EspNowNode instance;
    return instance;
}

EspNowNode::EspNowNode() : m_espNow(EspNow::instance()) {}

bool EspNowNode::setup(Wifi* wifi) {
    Config config {};
    return setup(wifi, config);
}

bool EspNowNode::setup(Wifi* wifi, const Config& config) {
    if (m_initialized || wifi == nullptr) {
        return false;
    }

    m_config = config;

    EspNow::Config espNowConfig {
        .peers     = m_peerStorage,
        .peerCount = 0,
        .maxPeers  = MaxNodes,
        .mode      = EspNow::Mode::UNICAST_BROADCAST,
    };

    const EspNow::RtnErrCode err = m_espNow.setup(wifi, espNowConfig, onRecv, onSend);
    if (err != EspNow::RtnErrCode::OK) {
        return false;
    }

    m_initialized = true;
    return true;
}

void EspNowNode::update() {
    if (!m_initialized) {
        return;
    }

    updateServiceTraffic();
    updateNodeTimeouts();
    updateRetries();
}

bool EspNowNode::broadcastDiscovery() {
    uint8_t      payload[NodeIdLen + NodeNameLen] {};
    const size_t payloadLen = buildLocalNodeInfoPayload(payload, sizeof(payload));

    return sendPacket(
        nullptr,
        EspNowProtocol::Type::DISCOVERY,
        payload,
        payloadLen);
}

bool EspNowNode::sendHeartbeat() {
    return sendPacket(
        nullptr,
        EspNowProtocol::Type::HEARTBEAT,
        nullptr,
        0);
}

bool EspNowNode::sendCommand(const char* nodeId, const uint8_t* payload, size_t len, bool needAck) {
    Node* node = findNodeById(nodeId);
    if (node == nullptr || !node->peerRegistered) {
        return false;
    }

    return sendCommand(*node, payload, len, needAck);
}

bool EspNowNode::sendCommand(Node& node, const uint8_t* payload, size_t len, bool needAck) {
    if (!node.peerRegistered || node.state == NodeState::OFFLINE || node.state == NodeState::ERROR) {
        return false;
    }

    return sendPacket(
        &node,
        EspNowProtocol::Type::COMMAND,
        payload,
        len,
        needAck ? EspNowProtocol::FLAG_NEED_ACK : EspNowProtocol::FLAG_NONE);
}

bool EspNowNode::sendCommandResult(const char* nodeId, const uint8_t* payload, size_t len, bool needAck) {
    Node* node = findNodeById(nodeId);
    if (node == nullptr || !node->peerRegistered) {
        return false;
    }

    return sendCommandResult(*node, payload, len, needAck);
}

bool EspNowNode::sendCommandResult(Node& node, const uint8_t* payload, size_t len, bool needAck) {
    if (!node.peerRegistered || node.state == NodeState::OFFLINE || node.state == NodeState::ERROR) {
        return false;
    }

    return sendPacket(
        &node,
        EspNowProtocol::Type::COMMAND_RESULT,
        payload,
        len,
        needAck ? EspNowProtocol::FLAG_NEED_ACK : EspNowProtocol::FLAG_NONE);
}

bool EspNowNode::addNode(const EspNow::Peer& peer, const char* nodeId, const char* nodeName) {
    Node* node = findNodeByMac(peer.mac);
    if (node == nullptr) {
        node = allocNode(peer.mac);
    }

    if (node == nullptr) {
        return false;
    }

    node->peer         = peer;
    node->peer.channel = peer.channel == 0 ? m_config.channel : peer.channel;
    copyText(node->nodeId, sizeof(node->nodeId), nodeId);
    copyText(node->nodeName, sizeof(node->nodeName), nodeName);
    node->state      = NodeState::DISCOVERED;
    node->lastSeenMs = millis();

    return registerPeer(*node);
}

bool EspNowNode::removeNodeByMac(const uint8_t* mac) {
    Node* node = findNodeByMac(mac);
    if (node == nullptr) {
        return false;
    }

    if (node->peerRegistered) {
        m_espNow.removePeerByMac(node->peer.mac);
    }

    clearNode(*node);
    if (m_nodeCount > 0) {
        --m_nodeCount;
    }
    return true;
}

bool EspNowNode::isNodeOnlineById(const char* nodeId) const {
    auto node = findNodeById(nodeId);
    return node != nullptr && node->state != NodeState::OFFLINE && node->state != NodeState::ERROR;
}

EspNowNode::Node* EspNowNode::findNodeByMac(const uint8_t* mac) {
    if (mac == nullptr) {
        return nullptr;
    }

    for (Node& node : m_nodes) {
        if (node.active && sameMac(node.peer.mac, mac)) {
            return &node;
        }
    }

    return nullptr;
}

const EspNowNode::Node* EspNowNode::findNodeByMac(const uint8_t* mac) const {
    if (mac == nullptr) {
        return nullptr;
    }

    for (const Node& node : m_nodes) {
        if (node.active && sameMac(node.peer.mac, mac)) {
            return &node;
        }
    }

    return nullptr;
}

EspNowNode::Node* EspNowNode::findNodeById(const char* nodeId) {
    if (nodeId == nullptr || nodeId[0] == '\0') {
        return nullptr;
    }

    if (isLocalNodeId(nodeId)) {
        return nullptr;
    }

    for (Node& node : m_nodes) {
        if (node.active && std::strncmp(node.nodeId, nodeId, sizeof(node.nodeId)) == 0) {
            return &node;
        }
    }

    return nullptr;
}

const EspNowNode::Node* EspNowNode::findNodeById(const char* nodeId) const {
    if (nodeId == nullptr || nodeId[0] == '\0') {
        return nullptr;
    }

    if (isLocalNodeId(nodeId)) {
        return nullptr;
    }

    for (const Node& node : m_nodes) {
        if (node.active && std::strncmp(node.nodeId, nodeId, sizeof(node.nodeId)) == 0) {
            return &node;
        }
    }

    return nullptr;
}

size_t EspNowNode::nodeCount() const {
    return m_nodeCount;
}

const EspNowNode::Node* EspNowNode::nodeAt(size_t index) const {
    size_t activeIndex = 0;
    for (const Node& node : m_nodes) {
        if (!node.active) {
            continue;
        }

        if (activeIndex == index) {
            return &node;
        }

        ++activeIndex;
    }

    return nullptr;
}

bool EspNowNode::initialized() const {
    return m_initialized;
}

bool EspNowNode::hasEvent() const {
    portENTER_CRITICAL(&m_eventMux);
    const bool result = m_eventCount > 0;
    portEXIT_CRITICAL(&m_eventMux);
    return result;
}

bool EspNowNode::popEvent(Event& outEvent) {
    portENTER_CRITICAL(&m_eventMux);
    if (m_eventCount == 0) {
        portEXIT_CRITICAL(&m_eventMux);
        return false;
    }

    outEvent    = m_eventQueue[m_eventTail];
    m_eventTail = static_cast<uint8_t>((m_eventTail + 1) % EventQueueCapacity);
    --m_eventCount;
    portEXIT_CRITICAL(&m_eventMux);
    return true;
}

void EspNowNode::onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (info == nullptr || info->src_addr == nullptr || len < 0) {
        return;
    }

    EspNowNode::instance().handlePacket(
        info->src_addr,
        data,
        static_cast<size_t>(len));
}

void EspNowNode::onSend(const esp_now_send_info_t*, esp_now_send_status_t) {}

void EspNowNode::handlePacket(const uint8_t* mac, const uint8_t* data, size_t len) {
    EspNowProtocol::Packet       packet;
    const EspNowProtocol::Result result = EspNowProtocol::decode(data, len, packet);
    if (result != EspNowProtocol::Result::OK) {
        return;
    }

    Node* node = findNodeByMac(mac);
    if (node == nullptr) {
        node = allocNode(mac);
    }

    if (node == nullptr) {
        return;
    }

    registerPeer(*node);

    const uint32_t nowMs = millis();
    node->lastSeenMs     = nowMs;
    node->lastRxSeq      = packet.header.seq;

    if (node->state == NodeState::UNKNOWN || node->state == NodeState::OFFLINE) {
        node->state = NodeState::DISCOVERED;
    }

    if (EspNowProtocol::hasFlag(packet.header.flags, EspNowProtocol::FLAG_NEED_ACK)) {
        sendAck(*node, packet.header.seq);
    }

    switch (packet.header.type) {
        case EspNowProtocol::Type::DISCOVERY:
            handleDiscovery(*node, packet);
            break;
        case EspNowProtocol::Type::NODE_INFO:
            handleNodeInfo(*node, packet);
            break;
        case EspNowProtocol::Type::HEARTBEAT:
            handleHeartbeat(*node, packet);
            break;
        case EspNowProtocol::Type::ACK:
            handleAck(*node, packet);
            break;
        case EspNowProtocol::Type::ERROR:
            handleError(*node, packet);
            break;
        case EspNowProtocol::Type::COMMAND:
            handleCommand(*node, packet);
            break;
        case EspNowProtocol::Type::COMMAND_RESULT:
            handleCommandResult(*node, packet);
            break;
        case EspNowProtocol::Type::DATA:
            handleData(*node, packet);
            break;
        default:
            node->state = NodeState::ONLINE;
            break;
    }
}

void EspNowNode::handleDiscovery(Node& node, const EspNowProtocol::Packet& packet) {
    applyNodeInfoPayload(node, packet);
    if (isLocalNodeId(node.nodeId)) {
        node.state = NodeState::ERROR;
        return;
    }

    node.state = NodeState::DISCOVERED;
    sendNodeInfo(node);
}

void EspNowNode::handleNodeInfo(Node& node, const EspNowProtocol::Packet& packet) {
    applyNodeInfoPayload(node, packet);
    if (isLocalNodeId(node.nodeId)) {
        node.state = NodeState::ERROR;
        return;
    }

    node.state = NodeState::ONLINE;
}

void EspNowNode::handleHeartbeat(Node& node, const EspNowProtocol::Packet&) {
    node.lastHeartbeatMs = millis();
    node.state           = NodeState::ONLINE;
}

void EspNowNode::handleAck(Node& node, const EspNowProtocol::Packet& packet) {
    if (node.pendingAckSeq == packet.header.ackSeq) {
        node.pendingAckSeq = 0;
        node.retryCount    = 0;
        node.lastTxLen     = 0;
    }

    node.state = NodeState::ONLINE;
}

void EspNowNode::handleError(Node& node, const EspNowProtocol::Packet&) {
    node.state = NodeState::ERROR;
}

void EspNowNode::handleCommand(Node& node, const EspNowProtocol::Packet& packet) {
    if (isLocalNodeId(node.nodeId)) {
        node.state = NodeState::ERROR;
        return;
    }

    node.state = NodeState::ONLINE;
    pushEvent(EventType::COMMAND_RECEIVED, node, packet);
}

void EspNowNode::handleCommandResult(Node& node, const EspNowProtocol::Packet& packet) {
    if (isLocalNodeId(node.nodeId)) {
        node.state = NodeState::ERROR;
        return;
    }

    node.state = NodeState::ONLINE;
    pushEvent(EventType::COMMAND_RESULT_RECEIVED, node, packet);
}

void EspNowNode::handleData(Node& node, const EspNowProtocol::Packet& packet) {
    if (isLocalNodeId(node.nodeId)) {
        node.state = NodeState::ERROR;
        return;
    }

    node.state = NodeState::ONLINE;
    pushEvent(EventType::DATA_RECEIVED, node, packet);
}

void EspNowNode::updateServiceTraffic() {
    const uint32_t nowMs = millis();

    if (m_config.discoveryIntervalMs > 0 && nowMs - m_lastDiscoveryMs >= m_config.discoveryIntervalMs) {
        if (broadcastDiscovery()) {
            m_lastDiscoveryMs = nowMs;
        }
    }

    if (m_config.heartbeatIntervalMs > 0 && nowMs - m_lastHeartbeatMs >= m_config.heartbeatIntervalMs) {
        if (sendHeartbeat()) {
            m_lastHeartbeatMs = nowMs;
        }
    }
}

void EspNowNode::updateNodeTimeouts() {
    const uint32_t nowMs = millis();

    for (Node& node : m_nodes) {
        if (!node.active) {
            continue;
        }

        const uint32_t silentMs = nowMs - node.lastSeenMs;
        if (silentMs >= m_config.offlineTimeoutMs) {
            node.state = NodeState::OFFLINE;
            continue;
        }

        if (silentMs >= m_config.staleTimeoutMs && node.state == NodeState::ONLINE) {
            node.state = NodeState::STALE;
        }
    }
}

void EspNowNode::updateRetries() {
    const uint32_t nowMs = millis();

    for (Node& node : m_nodes) {
        if (!node.active || node.pendingAckSeq == 0 || node.lastTxLen == 0) {
            continue;
        }

        if (nowMs - node.lastTxMs < m_config.ackTimeoutMs) {
            continue;
        }

        if (node.retryCount >= m_config.maxRetries) {
            node.pendingAckSeq = 0;
            node.retryCount    = 0;
            node.lastTxLen     = 0;
            node.state         = NodeState::ERROR;
            continue;
        }

        if (m_espNow.sendUnicast(node.lastTxPacket, node.lastTxLen, node.peer.mac) == EspNow::RtnErrCode::OK) {
            ++node.retryCount;
            node.lastTxMs = nowMs;
        }
    }
}

EspNowNode::Node* EspNowNode::allocNode(const uint8_t* mac) {
    if (mac == nullptr || m_nodeCount >= MaxNodes) {
        return nullptr;
    }

    for (Node& node : m_nodes) {
        if (node.active) {
            continue;
        }

        clearNode(node);
        std::memcpy(node.peer.mac, mac, sizeof(node.peer.mac));
        node.peer.channel = m_config.channel;
        node.active       = true;
        node.state        = NodeState::DISCOVERED;
        node.lastSeenMs   = millis();
        node.nextTxSeq    = 1;
        ++m_nodeCount;
        return &node;
    }

    return nullptr;
}

bool EspNowNode::registerPeer(Node& node) {
    if (node.peerRegistered) {
        return true;
    }

    const EspNow::RtnErrCode err = m_espNow.addPeer(node.peer);
    if (err == EspNow::RtnErrCode::OK || err == EspNow::RtnErrCode::PEER_ALREADY_EXISTS) {
        node.peerRegistered = true;
        return true;
    }

    return false;
}

bool EspNowNode::sendPacket(Node*                node,
                            EspNowProtocol::Type type,
                            const uint8_t*       payload,
                            size_t               payloadLen,
                            uint8_t              flags,
                            uint16_t             ackSeq) {
    if (!m_initialized) {
        return false;
    }

    uint16_t seq = nextSeq();
    if (node != nullptr) {
        seq = node->nextTxSeq++;
        if (node->nextTxSeq == 0) {
            node->nextTxSeq = 1;
        }
    }

    size_t                       packetLen    = 0;
    const EspNowProtocol::Result encodeResult = EspNowProtocol::encode(
        m_txBuffer,
        sizeof(m_txBuffer),
        packetLen,
        type,
        seq,
        payload,
        payloadLen,
        flags,
        ackSeq);

    if (encodeResult != EspNowProtocol::Result::OK) {
        return false;
    }

    const EspNow::RtnErrCode sendResult = node == nullptr
                                              ? m_espNow.sendBroadcast(m_txBuffer, packetLen)
                                              : m_espNow.sendUnicast(m_txBuffer, packetLen, node->peer.mac);

    if (sendResult != EspNow::RtnErrCode::OK) {
        return false;
    }

    if (node != nullptr && EspNowProtocol::hasFlag(flags, EspNowProtocol::FLAG_NEED_ACK)) {
        node->pendingAckSeq = seq;
        node->retryCount    = 0;
        node->lastTxMs      = millis();
        node->lastTxLen     = packetLen;
        std::memcpy(node->lastTxPacket, m_txBuffer, packetLen);
    }

    return true;
}

bool EspNowNode::sendAck(Node& node, uint16_t ackSeq) {
    return sendPacket(
        &node,
        EspNowProtocol::Type::ACK,
        nullptr,
        0,
        EspNowProtocol::FLAG_IS_ACK,
        ackSeq);
}

bool EspNowNode::sendNodeInfo(Node& node) {
    uint8_t      payload[NodeIdLen + NodeNameLen] {};
    const size_t payloadLen = buildLocalNodeInfoPayload(payload, sizeof(payload));

    return sendPacket(
        &node,
        EspNowProtocol::Type::NODE_INFO,
        payload,
        payloadLen);
}

size_t EspNowNode::buildLocalNodeInfoPayload(uint8_t* out, size_t outCapacity) {
    if (out == nullptr || outCapacity == 0) {
        return 0;
    }

    std::memset(out, 0, outCapacity);

    char* id = reinterpret_cast<char*>(out);
    copyText(id, outCapacity, m_config.localNodeId);

    const size_t idLen = std::strlen(id);
    if (idLen + 1 >= outCapacity) {
        return outCapacity;
    }

    char*        name         = reinterpret_cast<char*>(out + idLen + 1);
    const size_t nameCapacity = outCapacity - idLen - 1;
    copyText(name, nameCapacity, m_config.localNodeName);

    return idLen + 1 + std::strlen(name) + 1;
}

void EspNowNode::applyNodeInfoPayload(Node& node, const EspNowProtocol::Packet& packet) {
    const char* payload = reinterpret_cast<const char*>(packet.payload);
    if (payload == nullptr || packet.payloadLen == 0) {
        return;
    }

    const size_t idLen = strnlen(payload, packet.payloadLen);
    copyText(node.nodeId, sizeof(node.nodeId), payload);

    if (idLen + 1 < packet.payloadLen) {
        copyText(
            node.nodeName,
            sizeof(node.nodeName),
            payload + idLen + 1);
    }
}

uint16_t EspNowNode::nextSeq() {
    const uint16_t seq = m_nextSeq++;
    if (m_nextSeq == 0) {
        m_nextSeq = 1;
    }
    return seq;
}

void EspNowNode::pushEvent(EventType type, const Node& node, const EspNowProtocol::Packet& packet) {
    Event event {};

    event.type = type;
    std::memcpy(event.mac, node.peer.mac, sizeof(event.mac));
    copyText(event.nodeId, sizeof(event.nodeId), node.nodeId);
    copyText(event.nodeName, sizeof(event.nodeName), node.nodeName);
    event.seq = packet.header.seq;

    const size_t payloadLen = packet.payloadLen <= sizeof(event.payload)
                                  ? packet.payloadLen
                                  : sizeof(event.payload);
    event.payloadLen        = payloadLen;
    if (payloadLen > 0 && packet.payload != nullptr) {
        std::memcpy(event.payload, packet.payload, payloadLen);
    }

    portENTER_CRITICAL(&m_eventMux);
    m_eventQueue[m_eventHead] = event;
    m_eventHead               = static_cast<uint8_t>((m_eventHead + 1) % EventQueueCapacity);
    if (m_eventCount < EventQueueCapacity) {
        ++m_eventCount;
        portEXIT_CRITICAL(&m_eventMux);
        return;
    }

    m_eventTail = static_cast<uint8_t>((m_eventTail + 1) % EventQueueCapacity);
    portEXIT_CRITICAL(&m_eventMux);
}

void EspNowNode::clearNode(Node& node) {
    node = {};
}

void EspNowNode::copyText(char* dst, size_t dstLen, const char* src) {
    if (dst == nullptr || dstLen == 0) {
        return;
    }

    dst[0] = '\0';
    if (src == nullptr) {
        return;
    }

    std::strncpy(dst, src, dstLen - 1);
    dst[dstLen - 1] = '\0';
}

bool EspNowNode::isLocalNodeId(const char* nodeId) const {
    return nodeId != nullptr &&
           nodeId[0] != '\0' &&
           m_config.localNodeId != nullptr &&
           m_config.localNodeId[0] != '\0' &&
           std::strncmp(nodeId, m_config.localNodeId, NodeIdLen) == 0;
}

bool EspNowNode::sameMac(const uint8_t* lhs, const uint8_t* rhs) const {
    return lhs != nullptr && rhs != nullptr && std::memcmp(lhs, rhs, ESP_NOW_ETH_ALEN) == 0;
}
