#ifndef PRTN_SVC_MQTT_SERVER_H
#define PRTN_SVC_MQTT_SERVER_H

#include "Mqtt.h"

#include "../../cfg/BoardConfig.h"

#include <WiFi.h>
#include <cstddef>
#include <cstdint>
#include <functional>

class MqttServer
{
public:
    using MessageHandler = std::function<void(
        const char*    topic,
        const uint8_t* payload,
        size_t         payloadLen)>;

    enum class EventType : uint8_t
    {
        Started,
        Stopped,
        TcpClientAccepted,
        TcpClientRejected,
        ClientConnected,
        ClientDisconnected,
        Subscribe,
        Publish,
    };

    enum class DisconnectReason : uint8_t
    {
        None,
        TcpClosed,
        ClientRequested,
        ProtocolError,
        KeepAliveTimeout,
        ServerStopped,
        Rejected,
    };

    struct Event
    {
        EventType        type = EventType::Started;
        DisconnectReason reason = DisconnectReason::None;

        uint8_t  clientIndex = 0xFF;
        const char* clientId = "";
        const char* topic = "";
        size_t payloadLen = 0;

        uint16_t port = 0;
        uint8_t maxClients = 0;
        uint8_t activeClients = 0;
        uint8_t mqttClients = 0;
        uint8_t maxSubscriptions = 0;
        uint8_t activeSubscriptions = 0;
    };

    using EventHandler = std::function<void(const Event& event)>;

    struct Status
    {
        bool     started = false;
        uint16_t port    = 1883;

        uint8_t maxClients    = 0;
        uint8_t activeClients = 0;
        uint8_t mqttClients   = 0;

        uint8_t maxSubscriptions    = 0;
        uint8_t activeSubscriptions = 0;
    };

    explicit MqttServer(WiFiServer&             server,
                        const MqttServerConfig& config  = {},
                        MessageHandler          handler = nullptr,
                        EventHandler            eventHandler = nullptr);

    bool begin();
    void update();
    void stop();

    bool publish(const char* topic, const uint8_t* payload, size_t payloadLen);
    bool publish(const char* topic, const char* payload);

    void   setMessageHandler(MessageHandler handler);
    void   setEventHandler(EventHandler handler);
    Status getStatus() const;

private:
    struct ClientConnection
    {
        WiFiClient tcp;
        bool       used          = false;
        bool       mqttConnected = false;

        char     clientId[MqttPacket::MaxClientIdLen] = {};
        uint16_t keepAliveSec                         = 60;
        uint32_t lastSeenMs                           = 0;

        void attach(WiFiClient newTcp, uint32_t nowMs);
        void close();
    };

    struct Subscription
    {
        bool    used                           = false;
        uint8_t clientIndex                    = 0;
        char    topic[MqttPacket::MaxTopicLen] = {};
    };

private:
    MqttServerConfig m_config;
    WiFiServer&      m_server;

    ClientConnection m_clients[MqttServerConfig::MaxClientsLimit]             = {};
    Subscription     m_subscriptions[MqttServerConfig::MaxSubscriptionsLimit] = {};

    uint8_t    m_packetBuffer[MqttServerConfig::MaxPacketSizeLimit] = {};
    MqttPacket m_packet;

    MessageHandler m_handler;
    EventHandler   m_eventHandler;
    bool           m_started = false;

private:
    static MqttServerConfig normalizeConfig(MqttServerConfig config);

    void acceptClient();
    void handlePacket(uint8_t clientIndex, const MqttPacket& packet);

    bool readPacket(ClientConnection& conn, MqttPacket& packet);
    bool parsePacketBody(MqttPacket& packet, const uint8_t* data, size_t len);
    bool parseConnect(MqttPacket& packet, const uint8_t* data, size_t len);
    bool parsePublish(MqttPacket& packet, const uint8_t* data, size_t len);
    bool parseSubscribe(MqttPacket& packet, const uint8_t* data, size_t len);
    bool parsePubAck(MqttPacket& packet, const uint8_t* data, size_t len);

    void handleConnect(uint8_t clientIndex, const MqttPacket::Connect& connect);
    void handlePublish(uint8_t clientIndex, const MqttPacket::Publish& publish);
    void handleSubscribe(uint8_t clientIndex, const MqttPacket::Subscribe& subscribe);
    void handlePingReq(uint8_t clientIndex);

    bool sendConnAck(ClientConnection& conn, uint8_t returnCode);
    bool sendSubAck(ClientConnection& conn, const MqttPacket::Subscribe& subscribe);
    bool sendPubAck(ClientConnection& conn, uint16_t packetId);
    bool writePublish(ClientConnection& conn, const char* topic, const uint8_t* payload, size_t payloadLen);

    bool addSubscription(uint8_t clientIndex, const char* topic);
    void removeSubscriptions(uint8_t clientIndex);
    void forwardPublish(const MqttPacket::Publish& publish);

    bool isTimedOut(uint8_t clientIndex) const;
    void closeClient(uint8_t clientIndex, DisconnectReason reason = DisconnectReason::TcpClosed);
    void emitEvent(Event event);
};

#endif // PRTN_SVC_MQTT_SERVER_H
