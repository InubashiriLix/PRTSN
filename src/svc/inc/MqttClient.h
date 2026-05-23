#ifndef PRTN_SVC_MQTT_CLIENT_H
#define PRTN_SVC_MQTT_CLIENT_H

#include "src/prt/MqttProtocol.h"

#include "src/cfg/AppConfig.h"

#include <WiFi.h>
#include <cstddef>
#include <cstdint>
#include <functional>

class MqttClient
{
public:
    using MessageHandler = std::function<void(
        const char*    topic,
        const uint8_t* payload,
        size_t         payloadLen)>;

    enum class EventType : uint8_t
    {
        ConnectAttempt,
        Connected,
        ConnectFailed,
        Disconnected,
        SubscribeSent,
        PublishSent,
        PublishReceived,
        PingReqSent,
        PingRespReceived,
        SubAckReceived,
        PubAckReceived,
    };

    enum class DisconnectReason : uint8_t
    {
        None,
        TcpConnectFailed,
        ConnectPacketFailed,
        ConnAckRejected,
        PacketReadFailed,
        KeepAliveTimeout,
        BrokerRequested,
        ClientRequested,
        LocalStop,
    };

    struct Event
    {
        EventType        type   = EventType::ConnectAttempt;
        DisconnectReason reason = DisconnectReason::None;

        const char* host     = "";
        uint16_t    port     = 0;
        const char* clientId = "";
        const char* topic    = "";

        uint16_t packetId   = 0;
        size_t   payloadLen = 0;
        uint8_t  returnCode = 0;
    };

    using EventHandler = std::function<void(const Event& event)>;

    MqttClient();
    explicit MqttClient(const MqttClientConfig& config);

    void configure(const MqttClientConfig& config);

    bool begin();
    bool connect();
    void update();

    bool connected();
    bool checkConnection();

    bool publish(const char* topic, const uint8_t* payload, size_t payloadLen, bool retain = false);
    bool publish(const char* topic, const char* payload, bool retain = false);
    bool publish(const char* topic, const uint8_t* payload);

    bool subscribe(const char* topic, uint8_t qos = 0);
    bool sbuscribe(const char* topic, MessageHandler callback);

    bool disconnect();
    bool stop();

    void setMessageHandler(MessageHandler handler);
    void setEventHandler(EventHandler handler);

private:
    MqttClientConfig m_config;
    WiFiClient       m_tcp;

    uint8_t    m_packetBuffer[AppConfig::Mqtt::ClientPacketBufferSize] = {};
    MqttPacket m_packet;

    MessageHandler m_handler;
    EventHandler   m_eventHandler;

    uint16_t m_nextPacketId         = 1;
    uint32_t m_lastRxMs             = 0;
    uint32_t m_lastTxMs             = 0;
    uint32_t m_lastConnectAttemptMs = 0;
    bool     m_mqttConnected        = false;

private:
    bool     validConfig() const;
    uint16_t nextPacketId();

    bool sendConnect();
    bool waitForConnAck(uint32_t timeoutMs);
    bool sendPingReq();
    bool sendPingResp();
    bool sendPubAck(uint16_t packetId);

    bool readPacket(MqttPacket& packet, uint32_t timeoutMs = 20);
    bool parsePacketBody(MqttPacket& packet, const uint8_t* data, size_t len);
    bool parsePublish(MqttPacket& packet, const uint8_t* data, size_t len);
    bool parseConnAck(MqttPacket& packet, const uint8_t* data, size_t len);
    bool parseSubAck(MqttPacket& packet, const uint8_t* data, size_t len);
    bool parsePubAck(MqttPacket& packet, const uint8_t* data, size_t len);

    void handlePacket(const MqttPacket& packet);
    bool isKeepAliveTimedOut(uint32_t nowMs) const;
    void stopWithReason(DisconnectReason reason);
    void emitEvent(Event event);
};

#endif // PRTN_SVC_MQTT_CLIENT_H
