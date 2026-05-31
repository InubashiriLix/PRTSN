#include "src/cfg/BuildConfig.h"

#if PRTN_ENABLE_WIFI

#include "../inc/MqttServer.h"

#include <Arduino.h>
#include <cstring>

namespace
{
    bool validText(const char* text) {
        return text != nullptr && text[0] != '\0';
    }

    MqttPacket::Type decodeType(uint8_t value) {
        switch (value) {
            case 1:
                return MqttPacket::Type::CONNECT;
            case 2:
                return MqttPacket::Type::CONNACK;
            case 3:
                return MqttPacket::Type::PUBLISH;
            case 4:
                return MqttPacket::Type::PUBACK;
            case 8:
                return MqttPacket::Type::SUBSCRIBE;
            case 9:
                return MqttPacket::Type::SUBACK;
            case 12:
                return MqttPacket::Type::PINGREQ;
            case 13:
                return MqttPacket::Type::PINGRESP;
            case 14:
                return MqttPacket::Type::DISCONNECT;
            default:
                return MqttPacket::Type::UNKNOWN;
        }
    }

    bool readByte(WiFiClient& client, uint8_t& out, uint32_t timeoutMs = 20) {
        const uint32_t startMs = millis();

        while (client.connected() && client.available() <= 0) {
            if (millis() - startMs >= timeoutMs) {
                return false;
            }
            delay(1);
        }

        if (client.available() <= 0) {
            return false;
        }

        const int value = client.read();
        if (value < 0) {
            return false;
        }

        out = static_cast<uint8_t>(value);
        return true;
    }

    bool readExact(WiFiClient& client, uint8_t* out, size_t len, uint32_t timeoutMs = 50) {
        for (size_t i = 0; i < len; ++i) {
            if (!readByte(client, out[i], timeoutMs)) {
                return false;
            }
        }

        return true;
    }

    bool readRemainingLength(WiFiClient& client, uint32_t& outLen) {
        outLen              = 0;
        uint32_t multiplier = 1;

        for (uint8_t i = 0; i < 4; ++i) {
            uint8_t encoded = 0;
            if (!readByte(client, encoded)) {
                return false;
            }

            outLen += static_cast<uint32_t>(encoded & 0x7F) * multiplier;
            if ((encoded & 0x80) == 0) {
                return true;
            }

            multiplier *= 128;
        }

        return false;
    }

    bool writeRemainingLength(WiFiClient& client, uint32_t len) {
        do {
            uint8_t encoded = len % 128;
            len /= 128;
            if (len > 0) {
                encoded |= 0x80;
            }

            if (client.write(&encoded, 1) != 1) {
                return false;
            }
        } while (len > 0);

        return true;
    }

    bool readUint16(const uint8_t* data, size_t len, size_t& offset, uint16_t& out) {
        if (offset + 2 > len) {
            return false;
        }

        out = (static_cast<uint16_t>(data[offset]) << 8) |
              static_cast<uint16_t>(data[offset + 1]);
        offset += 2;
        return true;
    }

    bool readMqttString(const uint8_t* data, size_t len, size_t& offset, char* out, size_t outSize) {
        uint16_t strLen = 0;
        if (!readUint16(data, len, offset, strLen)) {
            return false;
        }

        if (offset + strLen > len || strLen >= outSize) {
            return false;
        }

        memcpy(out, data + offset, strLen);
        out[strLen] = '\0';
        offset += strLen;
        return true;
    }

    bool skipMqttString(const uint8_t* data, size_t len, size_t& offset) {
        uint16_t strLen = 0;
        if (!readUint16(data, len, offset, strLen)) {
            return false;
        }

        if (offset + strLen > len) {
            return false;
        }

        offset += strLen;
        return true;
    }

    bool writeMqttString(WiFiClient& client, const char* text) {
        if (text == nullptr) {
            return false;
        }

        const size_t len = strlen(text);
        if (len > 0xFFFF) {
            return false;
        }

        const uint8_t header[] = {
            static_cast<uint8_t>((len >> 8) & 0xFF),
            static_cast<uint8_t>(len & 0xFF),
        };

        return client.write(header, sizeof(header)) == sizeof(header) &&
               client.write(reinterpret_cast<const uint8_t*>(text), len) == len;
    }

    bool writeUint16ToBuffer(uint8_t* buffer, size_t bufferLen, size_t& offset, uint16_t value) {
        if (offset + 2 > bufferLen) {
            return false;
        }

        buffer[offset++] = static_cast<uint8_t>((value >> 8) & 0xFF);
        buffer[offset++] = static_cast<uint8_t>(value & 0xFF);
        return true;
    }

    bool topicMatches(const char* filter, const char* topic) {
        if (filter == nullptr || topic == nullptr) {
            return false;
        }

        const char* f = filter;
        const char* t = topic;

        while (*f != '\0') {
            if (*f == '#') {
                return f[1] == '\0';
            }

            if (*f == '+') {
                while (*t != '\0' && *t != '/') {
                    ++t;
                }
                ++f;

                if (*f == '/' && *t == '/') {
                    ++f;
                    ++t;
                }

                continue;
            }

            if (*f != *t) {
                return false;
            }

            ++f;
            ++t;
        }

        return *t == '\0';
    }
} // namespace

void MqttServer::ClientConnection::attach(WiFiClient newTcp, uint32_t nowMs) {
    tcp           = newTcp;
    used          = true;
    mqttConnected = false;
    clientId[0]   = '\0';
    keepAliveSec  = 60;
    lastSeenMs    = nowMs;
}

void MqttServer::ClientConnection::close() {
    if (tcp) {
        tcp.stop();
    }

    used          = false;
    mqttConnected = false;
    clientId[0]   = '\0';
    keepAliveSec  = 60;
    lastSeenMs    = 0;
}

MqttServerConfig MqttServer::normalizeConfig(MqttServerConfig config) {
    if (config.maxClients > MqttServerConfig::MaxClientsLimit) {
        config.maxClients = MqttServerConfig::MaxClientsLimit;
    }

    if (config.maxSubscriptions > MqttServerConfig::MaxSubscriptionsLimit) {
        config.maxSubscriptions = MqttServerConfig::MaxSubscriptionsLimit;
    }

    if (config.maxPacketSize > MqttServerConfig::MaxPacketSizeLimit) {
        config.maxPacketSize = MqttServerConfig::MaxPacketSizeLimit;
    }

    return config;
}

MqttServer::MqttServer(WiFiServer&             server,
                       const MqttServerConfig& config,
                       MessageHandler          handler,
                       EventHandler            eventHandler)
    : m_config(normalizeConfig(config)),
      m_server(server),
      m_handler(handler),
      m_eventHandler(eventHandler) {}

bool MqttServer::begin() {
    m_server.begin();
    m_started = true;
    emitEvent({.type = EventType::Started});
    return true;
}

void MqttServer::update() {
    if (!m_started) {
        return;
    }

    acceptClient();

    for (uint8_t i = 0; i < m_config.maxClients; ++i) {
        ClientConnection& conn = m_clients[i];
        if (!conn.used) {
            continue;
        }

        if (!conn.tcp.connected()) {
            closeClient(i, DisconnectReason::TcpClosed);
            continue;
        }

        while (conn.tcp.available() > 0) {
            m_packet.clear();

            if (!readPacket(conn, m_packet)) {
                closeClient(i, DisconnectReason::ProtocolError);
                break;
            }

            conn.lastSeenMs = millis();
            handlePacket(i, m_packet);

            if (!conn.used) {
                break;
            }
        }

        if (conn.used && isTimedOut(i)) {
            closeClient(i, DisconnectReason::KeepAliveTimeout);
        }
    }
}

void MqttServer::stop() {
    for (uint8_t i = 0; i < m_config.maxClients; ++i) {
        closeClient(i, DisconnectReason::ServerStopped);
    }

    m_server.stop();
    m_started = false;
    emitEvent({.type = EventType::Stopped});
}

bool MqttServer::publish(const char* topic, const uint8_t* payload, size_t payloadLen) {
    if (!validText(topic) || (payload == nullptr && payloadLen > 0)) {
        return false;
    }

    MqttPacket::Publish publish {};
    strncpy(publish.topic, topic, sizeof(publish.topic) - 1);
    publish.payload    = payload;
    publish.payloadLen = payloadLen;
    publish.qos        = 0;
    publish.retain     = false;
    publish.dup        = false;

    forwardPublish(publish);
    return true;
}

bool MqttServer::publish(const char* topic, const char* payload) {
    if (payload == nullptr) {
        return publish(topic, nullptr, 0);
    }

    return publish(topic, reinterpret_cast<const uint8_t*>(payload), strlen(payload));
}

void MqttServer::setMessageHandler(MessageHandler handler) {
    m_handler = handler;
}

void MqttServer::setEventHandler(EventHandler handler) {
    m_eventHandler = handler;
}

MqttServer::Status MqttServer::getStatus() const {
    Status status {};
    status.started          = m_started;
    status.port             = m_config.port;
    status.maxClients       = m_config.maxClients;
    status.maxSubscriptions = m_config.maxSubscriptions;

    for (uint8_t i = 0; i < m_config.maxClients; ++i) {
        if (m_clients[i].used) {
            ++status.activeClients;
        }

        if (m_clients[i].used && m_clients[i].mqttConnected) {
            ++status.mqttClients;
        }
    }

    for (uint8_t i = 0; i < m_config.maxSubscriptions; ++i) {
        if (m_subscriptions[i].used) {
            ++status.activeSubscriptions;
        }
    }

    return status;
}

void MqttServer::acceptClient() {
    WiFiClient incoming = m_server.accept();
    if (!incoming) {
        return;
    }

    for (uint8_t i = 0; i < m_config.maxClients; ++i) {
        if (!m_clients[i].used) {
            incoming.setNoDelay(true);
            m_clients[i].attach(incoming, millis());
            emitEvent({.type = EventType::TcpClientAccepted, .clientIndex = i});
            return;
        }
    }

    incoming.stop();
    emitEvent({.type = EventType::TcpClientRejected, .reason = DisconnectReason::Rejected});
}

bool MqttServer::readPacket(ClientConnection& conn, MqttPacket& packet) {
    uint8_t firstByte = 0;
    if (!readByte(conn.tcp, firstByte)) {
        return false;
    }

    packet.header.type  = decodeType(firstByte >> 4);
    packet.header.flags = firstByte & 0x0F;

    if (!readRemainingLength(conn.tcp, packet.header.remainingLength)) {
        return false;
    }

    if (packet.header.remainingLength > m_config.maxPacketSize) {
        return false;
    }

    if (!readExact(conn.tcp, m_packetBuffer, packet.header.remainingLength)) {
        return false;
    }

    return parsePacketBody(packet, m_packetBuffer, packet.header.remainingLength);
}

bool MqttServer::parsePacketBody(MqttPacket& packet, const uint8_t* data, size_t len) {
    switch (packet.header.type) {
        case MqttPacket::Type::CONNECT:
            return parseConnect(packet, data, len);
        case MqttPacket::Type::PUBLISH:
            return parsePublish(packet, data, len);
        case MqttPacket::Type::SUBSCRIBE:
            return parseSubscribe(packet, data, len);
        case MqttPacket::Type::PUBACK:
            return parsePubAck(packet, data, len);
        case MqttPacket::Type::PINGREQ:
        case MqttPacket::Type::DISCONNECT:
            packet.bodyKind = MqttPacket::BodyKind::None;
            return len == 0;
        default:
            return false;
    }
}

bool MqttServer::parseConnect(MqttPacket& packet, const uint8_t* data, size_t len) {
    size_t offset   = 0;
    packet.bodyKind = MqttPacket::BodyKind::Connect;

    if (!readMqttString(data, len, offset, packet.connect.protocolName, sizeof(packet.connect.protocolName))) {
        return false;
    }

    if (offset + 4 > len) {
        return false;
    }

    packet.connect.protocolLevel = data[offset++];
    packet.connect.connectFlags  = data[offset++];
    packet.connect.keepAliveSec =
        (static_cast<uint16_t>(data[offset]) << 8) |
        static_cast<uint16_t>(data[offset + 1]);
    offset += 2;

    if ((packet.connect.connectFlags & 0x01) != 0) {
        return false;
    }

    if (!readMqttString(data, len, offset, packet.connect.clientId, sizeof(packet.connect.clientId))) {
        return false;
    }

    const bool willFlag = (packet.connect.connectFlags & 0x04) != 0;
    if (willFlag) {
        if (!skipMqttString(data, len, offset) || !skipMqttString(data, len, offset)) {
            return false;
        }
    }

    if ((packet.connect.connectFlags & 0x80) != 0 && !skipMqttString(data, len, offset)) {
        return false;
    }

    if ((packet.connect.connectFlags & 0x40) != 0 && !skipMqttString(data, len, offset)) {
        return false;
    }

    return offset == len;
}

bool MqttServer::parsePublish(MqttPacket& packet, const uint8_t* data, size_t len) {
    size_t offset         = 0;
    packet.bodyKind       = MqttPacket::BodyKind::Publish;
    packet.publish.dup    = (packet.header.flags & 0x08) != 0;
    packet.publish.qos    = (packet.header.flags & 0x06) >> 1;
    packet.publish.retain = (packet.header.flags & 0x01) != 0;

    if (packet.publish.qos > 1) {
        return false;
    }

    if (!readMqttString(data, len, offset, packet.publish.topic, sizeof(packet.publish.topic))) {
        return false;
    }

    if (packet.publish.qos == 1 && !readUint16(data, len, offset, packet.publish.packetId)) {
        return false;
    }

    packet.publish.payload    = data + offset;
    packet.publish.payloadLen = len - offset;
    return true;
}

bool MqttServer::parseSubscribe(MqttPacket& packet, const uint8_t* data, size_t len) {
    if (packet.header.flags != 0x02) {
        return false;
    }

    size_t offset   = 0;
    packet.bodyKind = MqttPacket::BodyKind::Subscribe;

    if (!readUint16(data, len, offset, packet.subscribe.packetId) ||
        packet.subscribe.packetId == 0) {
        return false;
    }

    while (offset < len) {
        if (packet.subscribe.topicCount >= MqttPacket::MaxTopicFiltersPerSubscribe) {
            return false;
        }

        MqttPacket::TopicFilter& filter = packet.subscribe.topics[packet.subscribe.topicCount];
        if (!readMqttString(data, len, offset, filter.topic, sizeof(filter.topic))) {
            return false;
        }

        if (offset >= len) {
            return false;
        }

        filter.requestedQos = data[offset++];
        if (filter.requestedQos > 1) {
            return false;
        }

        ++packet.subscribe.topicCount;
    }

    return packet.subscribe.topicCount > 0;
}

bool MqttServer::parsePubAck(MqttPacket& packet, const uint8_t* data, size_t len) {
    size_t offset   = 0;
    packet.bodyKind = MqttPacket::BodyKind::PubAck;
    return readUint16(data, len, offset, packet.pubAck.packetId) && offset == len;
}

void MqttServer::handlePacket(uint8_t clientIndex, const MqttPacket& packet) {
    ClientConnection& conn = m_clients[clientIndex];

    if (!conn.mqttConnected && packet.header.type != MqttPacket::Type::CONNECT) {
        closeClient(clientIndex, DisconnectReason::ProtocolError);
        return;
    }

    switch (packet.header.type) {
        case MqttPacket::Type::CONNECT:
            handleConnect(clientIndex, packet.connect);
            break;
        case MqttPacket::Type::PUBLISH:
            handlePublish(clientIndex, packet.publish);
            break;
        case MqttPacket::Type::SUBSCRIBE:
            handleSubscribe(clientIndex, packet.subscribe);
            break;
        case MqttPacket::Type::PINGREQ:
            handlePingReq(clientIndex);
            break;
        case MqttPacket::Type::DISCONNECT:
            closeClient(clientIndex, DisconnectReason::ClientRequested);
            break;
        case MqttPacket::Type::PUBACK:
            break;
        default:
            closeClient(clientIndex, DisconnectReason::ProtocolError);
            break;
    }
}

void MqttServer::handleConnect(uint8_t clientIndex, const MqttPacket::Connect& connect) {
    ClientConnection& conn = m_clients[clientIndex];

    if (strcmp(connect.protocolName, "MQTT") != 0 || connect.protocolLevel != 4) {
        sendConnAck(conn, 0x01);
        closeClient(clientIndex, DisconnectReason::ProtocolError);
        return;
    }

    if (!validText(connect.clientId)) {
        sendConnAck(conn, 0x02);
        closeClient(clientIndex, DisconnectReason::ProtocolError);
        return;
    }

    strncpy(conn.clientId, connect.clientId, sizeof(conn.clientId) - 1);
    conn.clientId[sizeof(conn.clientId) - 1] = '\0';
    conn.keepAliveSec                        = connect.keepAliveSec;
    conn.mqttConnected                       = true;
    conn.lastSeenMs                          = millis();

    sendConnAck(conn, 0x00);
    emitEvent({.type        = EventType::ClientConnected,
               .clientIndex = clientIndex,
               .clientId    = conn.clientId});
}

void MqttServer::handlePublish(uint8_t clientIndex, const MqttPacket::Publish& publish) {
    ClientConnection& conn = m_clients[clientIndex];

    if (publish.qos == 1) {
        sendPubAck(conn, publish.packetId);
    }

    if (m_handler) {
        m_handler(publish.topic, publish.payload, publish.payloadLen);
    }

    emitEvent({.type        = EventType::Publish,
               .clientIndex = clientIndex,
               .clientId    = conn.clientId,
               .topic       = publish.topic,
               .payloadLen  = publish.payloadLen});

    forwardPublish(publish);
}

void MqttServer::handleSubscribe(uint8_t clientIndex, const MqttPacket::Subscribe& subscribe) {
    for (uint8_t i = 0; i < subscribe.topicCount; ++i) {
        if (addSubscription(clientIndex, subscribe.topics[i].topic)) {
            emitEvent({.type        = EventType::Subscribe,
                       .clientIndex = clientIndex,
                       .clientId    = m_clients[clientIndex].clientId,
                       .topic       = subscribe.topics[i].topic});
        }
    }

    sendSubAck(m_clients[clientIndex], subscribe);
}

void MqttServer::handlePingReq(uint8_t clientIndex) {
    static const uint8_t response[]   = {0xD0, 0x00};
    m_clients[clientIndex].lastSeenMs = millis();
    m_clients[clientIndex].tcp.write(response, sizeof(response));
}

bool MqttServer::sendConnAck(ClientConnection& conn, uint8_t returnCode) {
    const uint8_t response[] = {0x20, 0x02, 0x00, returnCode};
    return conn.tcp.write(response, sizeof(response)) == sizeof(response);
}

bool MqttServer::sendSubAck(ClientConnection& conn, const MqttPacket::Subscribe& subscribe) {
    uint8_t response[2 + 2 + MqttPacket::MaxTopicFiltersPerSubscribe] = {};
    size_t  offset                                                    = 0;

    response[offset++] = 0x90;
    response[offset++] = static_cast<uint8_t>(2 + subscribe.topicCount);
    response[offset++] = static_cast<uint8_t>((subscribe.packetId >> 8) & 0xFF);
    response[offset++] = static_cast<uint8_t>(subscribe.packetId & 0xFF);

    for (uint8_t i = 0; i < subscribe.topicCount; ++i) {
        response[offset++] = 0x00;
    }

    return conn.tcp.write(response, offset) == offset;
}

bool MqttServer::sendPubAck(ClientConnection& conn, uint16_t packetId) {
    const uint8_t response[] = {
        0x40,
        0x02,
        static_cast<uint8_t>((packetId >> 8) & 0xFF),
        static_cast<uint8_t>(packetId & 0xFF),
    };

    return conn.tcp.write(response, sizeof(response)) == sizeof(response);
}

bool MqttServer::writePublish(ClientConnection& conn, const char* topic, const uint8_t* payload, size_t payloadLen) {
    if (!validText(topic) || (payload == nullptr && payloadLen > 0)) {
        return false;
    }

    const size_t   topicLen        = strlen(topic);
    const uint32_t remainingLength = 2 + topicLen + payloadLen;

    if (remainingLength > m_config.maxPacketSize) {
        return false;
    }

    const uint8_t fixedHeader = 0x30;
    if (conn.tcp.write(&fixedHeader, 1) != 1 ||
        !writeRemainingLength(conn.tcp, remainingLength) ||
        !writeMqttString(conn.tcp, topic)) {
        return false;
    }

    if (payloadLen == 0) {
        return true;
    }

    return conn.tcp.write(payload, payloadLen) == payloadLen;
}

bool MqttServer::addSubscription(uint8_t clientIndex, const char* topic) {
    if (!validText(topic)) {
        return false;
    }

    for (uint8_t i = 0; i < m_config.maxSubscriptions; ++i) {
        if (m_subscriptions[i].used &&
            m_subscriptions[i].clientIndex == clientIndex &&
            strcmp(m_subscriptions[i].topic, topic) == 0) {
            return true;
        }
    }

    for (uint8_t i = 0; i < m_config.maxSubscriptions; ++i) {
        if (!m_subscriptions[i].used) {
            m_subscriptions[i].used        = true;
            m_subscriptions[i].clientIndex = clientIndex;
            strncpy(m_subscriptions[i].topic, topic, sizeof(m_subscriptions[i].topic) - 1);
            m_subscriptions[i].topic[sizeof(m_subscriptions[i].topic) - 1] = '\0';
            return true;
        }
    }

    return false;
}

void MqttServer::removeSubscriptions(uint8_t clientIndex) {
    for (uint8_t i = 0; i < m_config.maxSubscriptions; ++i) {
        if (m_subscriptions[i].used && m_subscriptions[i].clientIndex == clientIndex) {
            m_subscriptions[i] = {};
        }
    }
}

void MqttServer::forwardPublish(const MqttPacket::Publish& publish) {
    bool sent[MqttServerConfig::MaxClientsLimit] = {};

    for (uint8_t i = 0; i < m_config.maxSubscriptions; ++i) {
        const Subscription& sub = m_subscriptions[i];
        if (!sub.used || !topicMatches(sub.topic, publish.topic)) {
            continue;
        }

        const uint8_t clientIndex = sub.clientIndex;
        if (clientIndex >= m_config.maxClients || sent[clientIndex]) {
            continue;
        }

        ClientConnection& conn = m_clients[clientIndex];
        if (!conn.used || !conn.mqttConnected || !conn.tcp.connected()) {
            continue;
        }

        writePublish(conn, publish.topic, publish.payload, publish.payloadLen);
        sent[clientIndex] = true;
    }
}

bool MqttServer::isTimedOut(uint8_t clientIndex) const {
    const ClientConnection& conn = m_clients[clientIndex];
    if (!conn.used || !conn.mqttConnected || conn.keepAliveSec == 0) {
        return false;
    }

    const uint32_t timeoutMs = static_cast<uint32_t>(conn.keepAliveSec) * 1500UL;
    return millis() - conn.lastSeenMs > timeoutMs;
}

void MqttServer::closeClient(uint8_t clientIndex, DisconnectReason reason) {
    if (clientIndex >= m_config.maxClients) {
        return;
    }

    if (!m_clients[clientIndex].used) {
        return;
    }

    char clientId[MqttPacket::MaxClientIdLen] = {};
    strncpy(clientId, m_clients[clientIndex].clientId, sizeof(clientId) - 1);

    removeSubscriptions(clientIndex);
    m_clients[clientIndex].close();

    emitEvent({.type        = EventType::ClientDisconnected,
               .reason      = reason,
               .clientIndex = clientIndex,
               .clientId    = clientId});
}

void MqttServer::emitEvent(Event event) {
    if (!m_eventHandler) {
        return;
    }

    const Status status       = getStatus();
    event.port                = status.port;
    event.maxClients          = status.maxClients;
    event.activeClients       = status.activeClients;
    event.mqttClients         = status.mqttClients;
    event.maxSubscriptions    = status.maxSubscriptions;
    event.activeSubscriptions = status.activeSubscriptions;

    m_eventHandler(event);
}

#endif
