#ifndef PRTN_SVC_MQTT_H
#define PRTN_SVC_MQTT_H

#include <cstddef>
#include <cstdint>

#include "src/cfg/AppConfig.h"

struct MqttServerConfig
{
    static constexpr uint8_t  MaxClientsLimit       = AppConfig::Mqtt::ServerMaxClients;
    static constexpr uint8_t  MaxSubscriptionsLimit = AppConfig::Mqtt::ServerMaxSubscriptions;
    static constexpr uint16_t MaxPacketSizeLimit    = AppConfig::Mqtt::PacketBufferSize;

    uint16_t port                      = AppConfig::Mqtt::ServerPort;
    uint8_t  maxClients                = MaxClientsLimit;
    uint8_t  maxSubscriptions          = MaxSubscriptionsLimit;
    uint16_t maxPacketSize             = MaxPacketSizeLimit;
    uint32_t defaultKeepAliveTimeoutMs = 90000;
};

struct MqttClientConfig
{
    const char* host = nullptr;
    uint16_t    port = 1883;

    const char* clientId = nullptr;
    const char* username = nullptr;
    const char* password = nullptr;

    uint16_t keepAliveSec        = AppConfig::Mqtt::ClientKeepAliveSec;
    bool     cleanSession        = true;
    uint16_t maxPacketSize       = AppConfig::Mqtt::ClientMaxPacketSize;
    uint32_t reconnectIntervalMs = AppConfig::Mqtt::ClientReconnectMs;
};

struct MqttPacket
{
    static constexpr uint8_t MaxTopicFiltersPerSubscribe = 4;
    static constexpr uint8_t MaxProtocolNameLen          = 8;
    static constexpr uint8_t MaxClientIdLen              = 32;
    static constexpr uint8_t MaxTopicLen                 = 64;

    enum class Type : uint8_t
    {
        CONNECT    = 1,
        CONNACK    = 2,
        PUBLISH    = 3,
        PUBACK     = 4,
        SUBSCRIBE  = 8,
        SUBACK     = 9,
        PINGREQ    = 12,
        PINGRESP   = 13,
        DISCONNECT = 14,
        UNKNOWN    = 255,
    };

    enum class BodyKind : uint8_t
    {
        None,
        Connect,
        ConnAck,
        Publish,
        PubAck,
        Subscribe,
        SubAck,
    };

    struct Header
    {
        Type     type            = Type::UNKNOWN;
        uint8_t  flags           = 0;
        uint32_t remainingLength = 0;
    };

    struct Connect
    {
        char     protocolName[MaxProtocolNameLen] = {};
        uint8_t  protocolLevel                    = 0;
        uint8_t  connectFlags                     = 0;
        uint16_t keepAliveSec                     = 0;
        char     clientId[MaxClientIdLen]         = {};
    };

    struct ConnAck
    {
        bool    sessionPresent = false;
        uint8_t returnCode     = 0;
    };

    struct Publish
    {
        char           topic[MaxTopicLen] = {};
        const uint8_t* payload            = nullptr;
        size_t         payloadLen         = 0;

        uint16_t packetId = 0;
        bool     retain   = false;
        bool     dup      = false;
        uint8_t  qos      = 0;
    };

    struct TopicFilter
    {
        char    topic[MaxTopicLen] = {};
        uint8_t requestedQos       = 0;
    };

    struct Subscribe
    {
        uint16_t    packetId                            = 0;
        uint8_t     topicCount                          = 0;
        TopicFilter topics[MaxTopicFiltersPerSubscribe] = {};
    };

    struct SubAck
    {
        uint16_t packetId = 0;
    };

    struct PubAck
    {
        uint16_t packetId = 0;
    };

    Header    header;
    BodyKind  bodyKind = BodyKind::None;
    Connect   connect;
    ConnAck   connAck;
    Publish   publish;
    PubAck    pubAck;
    Subscribe subscribe;
    SubAck    subAck;

    void clear() {
        header    = {};
        bodyKind  = BodyKind::None;
        connect   = {};
        connAck   = {};
        publish   = {};
        pubAck    = {};
        subscribe = {};
        subAck    = {};
    }
};

#endif // PRTN_SVC_MQTT_H
