#pragma once

#include "src/dvc/inc/EspNow.h"
#include "src/prt/EspNowProtocol.h"
#include "src/svc/inc/wifi.h"

#include <cstddef>
#include <cstdint>
#include <esp_now.h>
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

class EspNowNode
{
public:
    static constexpr uint8_t  MaxNodes                   = 16;
    static constexpr uint8_t  EventQueueCapacity         = 8;
    static constexpr size_t   NodeIdLen                  = 32;
    static constexpr size_t   NodeNameLen                = 32;
    static constexpr uint32_t DefaultDiscoveryIntervalMs = 1000;
    static constexpr uint32_t DefaultHeartbeatIntervalMs = 2000;
    static constexpr uint32_t DefaultStaleTimeoutMs      = 5000;
    static constexpr uint32_t DefaultOfflineTimeoutMs    = 15000;
    static constexpr uint32_t DefaultAckTimeoutMs        = 300;
    static constexpr uint8_t  DefaultMaxRetries          = 3;

    enum class NodeState : uint8_t
    {
        UNKNOWN,
        DISCOVERED,
        ONLINE,
        STALE,
        OFFLINE,
        ERROR,
    };

    struct Config
    {
        const char* localNodeId         = nullptr;
        const char* localNodeName       = nullptr;
        uint8_t     channel             = 0;
        uint32_t    discoveryIntervalMs = DefaultDiscoveryIntervalMs;
        uint32_t    heartbeatIntervalMs = DefaultHeartbeatIntervalMs;
        uint32_t    staleTimeoutMs      = DefaultStaleTimeoutMs;
        uint32_t    offlineTimeoutMs    = DefaultOfflineTimeoutMs;
        uint32_t    ackTimeoutMs        = DefaultAckTimeoutMs;
        uint8_t     maxRetries          = DefaultMaxRetries;
    };

    struct Node
    {
        EspNow::Peer peer;

        char nodeId[NodeIdLen] {};
        char nodeName[NodeNameLen] {};

        NodeState state          = NodeState::UNKNOWN;
        bool      active         = false;
        bool      peerRegistered = false;

        uint32_t lastSeenMs      = 0;
        uint32_t lastHeartbeatMs = 0;

        uint16_t lastRxSeq = 0;
        uint16_t nextTxSeq = 1;

        uint16_t pendingAckSeq = 0;
        uint8_t  retryCount    = 0;
        uint32_t lastTxMs      = 0;

        uint8_t lastTxPacket[EspNowProtocol::MaxPacketLen] {};
        size_t  lastTxLen = 0;
    };

    enum class EventType : uint8_t
    {
        COMMAND_RECEIVED,
        COMMAND_RESULT_RECEIVED,
        DATA_RECEIVED,
    };

    struct Event
    {
        EventType type = EventType::DATA_RECEIVED;

        uint8_t mac[ESP_NOW_ETH_ALEN] {};
        char    nodeId[NodeIdLen] {};
        char    nodeName[NodeNameLen] {};

        uint16_t seq = 0;
        uint8_t  payload[EspNowProtocol::MaxPayloadLen] {};
        size_t   payloadLen = 0;
    };

public:
    static EspNowNode& instance();

    EspNowNode(const EspNowNode&)            = delete;
    EspNowNode& operator=(const EspNowNode&) = delete;

    EspNowNode(EspNowNode&&)            = delete;
    EspNowNode& operator=(EspNowNode&&) = delete;

    bool setup(Wifi* wifi);
    bool setup(Wifi* wifi, const Config& config);
    void update();

    bool sendCommand(const char* nodeId, const uint8_t* payload, size_t len, bool needAck = true);
    bool sendCommand(Node& node, const uint8_t* payload, size_t len, bool needAck = true);
    bool sendCommandResult(const char* nodeId, const uint8_t* payload, size_t len, bool needAck = true);
    bool sendCommandResult(Node& node, const uint8_t* payload, size_t len, bool needAck = true);

    bool addNode(const EspNow::Peer& peer, const char* nodeId = nullptr, const char* nodeName = nullptr);
    bool removeNodeByMac(const uint8_t* mac);
    bool isNodeOnlineById(const char* nodeId) const;

    Node*       findNodeByMac(const uint8_t* mac);
    const Node* findNodeByMac(const uint8_t* mac) const;
    Node*       findNodeById(const char* nodeId);
    const Node* findNodeById(const char* nodeId) const;

    size_t      nodeCount() const;
    const Node* nodeAt(size_t index) const;
    bool        initialized() const;

    bool hasEvent() const;
    bool popEvent(Event& outEvent);

private:
    EspNowNode();

    static void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len);
    static void onSend(const esp_now_send_info_t* info, esp_now_send_status_t status);

    bool broadcastDiscovery();
    bool sendHeartbeat();

    void handlePacket(const uint8_t* mac, const uint8_t* data, size_t len);
    void handleDiscovery(Node& node, const EspNowProtocol::Packet& packet);
    void handleNodeInfo(Node& node, const EspNowProtocol::Packet& packet);
    void handleHeartbeat(Node& node, const EspNowProtocol::Packet& packet);
    void handleAck(Node& node, const EspNowProtocol::Packet& packet);
    void handleError(Node& node, const EspNowProtocol::Packet& packet);
    void handleCommand(Node& node, const EspNowProtocol::Packet& packet);
    void handleCommandResult(Node& node, const EspNowProtocol::Packet& packet);
    void handleData(Node& node, const EspNowProtocol::Packet& packet);

    void updateServiceTraffic();
    void updateNodeTimeouts();
    void updateRetries();

    Node*  allocNode(const uint8_t* mac);
    bool   registerPeer(Node& node);
    bool   sendPacket(Node*                node,
                      EspNowProtocol::Type type,
                      const uint8_t*       payload,
                      size_t               payloadLen,
                      uint8_t              flags  = EspNowProtocol::FLAG_NONE,
                      uint16_t             ackSeq = 0);
    bool   sendAck(Node& node, uint16_t ackSeq);
    bool   sendNodeInfo(Node& node);
    size_t buildLocalNodeInfoPayload(uint8_t* out, size_t outCapacity);
    void   applyNodeInfoPayload(Node& node, const EspNowProtocol::Packet& packet);

    uint16_t nextSeq();
    void     pushEvent(EventType type, const Node& node, const EspNowProtocol::Packet& packet);
    void     clearNode(Node& node);
    void     copyText(char* dst, size_t dstLen, const char* src);
    bool     isLocalNodeId(const char* nodeId) const;
    bool     sameMac(const uint8_t* lhs, const uint8_t* rhs) const;

private:
    EspNow& m_espNow;
    Config  m_config;

    EspNow::Peer m_peerStorage[MaxNodes] {};
    Node         m_nodes[MaxNodes] {};
    size_t       m_nodeCount = 0;

    uint8_t  m_txBuffer[EspNowProtocol::MaxPacketLen] {};
    uint16_t m_nextSeq         = 1;
    bool     m_initialized     = false;
    uint32_t m_lastDiscoveryMs = 0;
    uint32_t m_lastHeartbeatMs = 0;

    Event                m_eventQueue[EventQueueCapacity] {};
    uint8_t              m_eventHead  = 0;
    uint8_t              m_eventTail  = 0;
    uint8_t              m_eventCount = 0;
    mutable portMUX_TYPE m_eventMux   = portMUX_INITIALIZER_UNLOCKED;
};
