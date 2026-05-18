#include "../inc/MqttClient.h"

#include <Arduino.h>
#include <cstring>

namespace
{
    bool validText(const char* text) {
        return text != nullptr && text[0] != '\0';
    }

    MqttPacket::Type decodeType(uint8_t value) {
        switch (value) {
            case 2:
                return MqttPacket::Type::CONNACK;
            case 3:
                return MqttPacket::Type::PUBLISH;
            case 4:
                return MqttPacket::Type::PUBACK;
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

    bool readByte(WiFiClient& client, uint8_t& out, uint32_t timeoutMs) {
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

    bool readExact(WiFiClient& client, uint8_t* out, size_t len, uint32_t timeoutMs) {
        for (size_t i = 0; i < len; ++i) {
            if (!readByte(client, out[i], timeoutMs)) {
                return false;
            }
        }

        return true;
    }

    bool readRemainingLength(WiFiClient& client, uint32_t& outLen, uint32_t timeoutMs) {
        outLen = 0;
        uint32_t multiplier = 1;

        for (uint8_t i = 0; i < 4; ++i) {
            uint8_t encoded = 0;
            if (!readByte(client, encoded, timeoutMs)) {
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

    bool writeMqttStringToBuffer(uint8_t* buffer, size_t bufferLen, size_t& offset, const char* text) {
        if (text == nullptr) {
            return false;
        }

        const size_t len = strlen(text);
        if (len > 0xFFFF || offset + 2 + len > bufferLen) {
            return false;
        }

        buffer[offset++] = static_cast<uint8_t>((len >> 8) & 0xFF);
        buffer[offset++] = static_cast<uint8_t>(len & 0xFF);
        memcpy(buffer + offset, text, len);
        offset += len;
        return true;
    }

    bool writeUint16ToBuffer(uint8_t* buffer, size_t bufferLen, size_t& offset, uint16_t value) {
        if (offset + 2 > bufferLen) {
            return false;
        }

        buffer[offset++] = static_cast<uint8_t>((value >> 8) & 0xFF);
        buffer[offset++] = static_cast<uint8_t>(value & 0xFF);
        return true;
    }
} // namespace

MqttClient::MqttClient() = default;

MqttClient::MqttClient(const MqttClientConfig& config) : m_config(config) {}

void MqttClient::configure(const MqttClientConfig& config) {
    m_config = config;
}

bool MqttClient::begin() {
    return connect();
}

bool MqttClient::connect() {
    if (!validConfig()) {
        emitEvent({.type = EventType::ConnectFailed, .reason = DisconnectReason::TcpConnectFailed});
        return false;
    }

    if (connected()) {
        return true;
    }

    const uint32_t nowMs = millis();
    if (m_lastConnectAttemptMs != 0 &&
        nowMs - m_lastConnectAttemptMs < m_config.reconnectIntervalMs) {
        return false;
    }

    m_lastConnectAttemptMs = nowMs;
    m_mqttConnected = false;
    m_tcp.stop();

    emitEvent({.type = EventType::ConnectAttempt});

    if (!m_tcp.connect(m_config.host, m_config.port)) {
        emitEvent({.type = EventType::ConnectFailed, .reason = DisconnectReason::TcpConnectFailed});
        return false;
    }

    m_tcp.setNoDelay(true);

    if (!sendConnect() || !waitForConnAck(2000)) {
        m_tcp.stop();
        m_mqttConnected = false;
        emitEvent({.type = EventType::ConnectFailed, .reason = DisconnectReason::ConnAckRejected});
        return false;
    }

    m_mqttConnected = true;
    m_lastRxMs = millis();
    m_lastTxMs = m_lastRxMs;
    emitEvent({.type = EventType::Connected});
    return true;
}

void MqttClient::update() {
    if (!connected()) {
        checkConnection();
        return;
    }

    while (m_tcp.available() > 0) {
        m_packet.clear();

        if (!readPacket(m_packet)) {
            stopWithReason(DisconnectReason::PacketReadFailed);
            return;
        }

        m_lastRxMs = millis();
        handlePacket(m_packet);
    }

    const uint32_t nowMs = millis();

    if (isKeepAliveTimedOut(nowMs)) {
        stopWithReason(DisconnectReason::KeepAliveTimeout);
        return;
    }

    if (m_config.keepAliveSec > 0 &&
        nowMs - m_lastTxMs >= static_cast<uint32_t>(m_config.keepAliveSec) * 500UL) {
        sendPingReq();
    }
}

bool MqttClient::connected() {
    return m_mqttConnected && m_tcp.connected();
}

bool MqttClient::checkConnection() {
    if (connected()) {
        return true;
    }

    return connect();
}

bool MqttClient::publish(const char* topic, const uint8_t* payload, size_t payloadLen, bool retain) {
    if (!connected() || !validText(topic) || (payload == nullptr && payloadLen > 0)) {
        return false;
    }

    const size_t topicLen = strlen(topic);
    const uint32_t remainingLength = 2 + topicLen + payloadLen;
    const uint16_t packetLimit = m_config.maxPacketSize < PRTN_MQTT_PACKET_BUFFER_SIZE
                                     ? m_config.maxPacketSize
                                     : PRTN_MQTT_PACKET_BUFFER_SIZE;

    if (remainingLength > packetLimit) {
        return false;
    }

    const uint8_t fixedHeader = retain ? 0x31 : 0x30;
    if (m_tcp.write(&fixedHeader, 1) != 1 ||
        !writeRemainingLength(m_tcp, remainingLength)) {
        return false;
    }

    size_t offset = 0;
    if (!writeMqttStringToBuffer(m_packetBuffer, sizeof(m_packetBuffer), offset, topic)) {
        return false;
    }

    if (m_tcp.write(m_packetBuffer, offset) != offset) {
        return false;
    }

    if (payloadLen > 0 && m_tcp.write(payload, payloadLen) != payloadLen) {
        return false;
    }

    m_lastTxMs = millis();
    emitEvent({.type = EventType::PublishSent, .topic = topic, .payloadLen = payloadLen});
    return true;
}

bool MqttClient::publish(const char* topic, const char* payload, bool retain) {
    if (payload == nullptr) {
        return publish(topic, nullptr, 0, retain);
    }

    return publish(topic, reinterpret_cast<const uint8_t*>(payload), strlen(payload), retain);
}

bool MqttClient::publish(const char* topic, const uint8_t* payload) {
    if (payload == nullptr) {
        return publish(topic, nullptr, 0, false);
    }

    return publish(topic, payload, strlen(reinterpret_cast<const char*>(payload)), false);
}

bool MqttClient::subscribe(const char* topic, uint8_t qos) {
    if (!connected() || !validText(topic) || qos > 1) {
        return false;
    }

    const uint16_t packetId = nextPacketId();
    size_t offset = 0;

    if (!writeUint16ToBuffer(m_packetBuffer, sizeof(m_packetBuffer), offset, packetId) ||
        !writeMqttStringToBuffer(m_packetBuffer, sizeof(m_packetBuffer), offset, topic)) {
        return false;
    }

    if (offset + 1 > sizeof(m_packetBuffer)) {
        return false;
    }
    m_packetBuffer[offset++] = qos;

    const uint8_t fixedHeader = 0x82;
    if (m_tcp.write(&fixedHeader, 1) != 1 ||
        !writeRemainingLength(m_tcp, offset) ||
        m_tcp.write(m_packetBuffer, offset) != offset) {
        return false;
    }

    m_lastTxMs = millis();
    emitEvent({.type = EventType::SubscribeSent, .topic = topic, .packetId = packetId});
    return true;
}

bool MqttClient::sbuscribe(const char* topic, MessageHandler callback) {
    setMessageHandler(callback);
    return subscribe(topic, 0);
}

bool MqttClient::disconnect() {
    if (m_tcp.connected()) {
        static const uint8_t packet[] = {0xE0, 0x00};
        m_tcp.write(packet, sizeof(packet));
    }

    stopWithReason(DisconnectReason::ClientRequested);
    return true;
}

bool MqttClient::stop() {
    stopWithReason(DisconnectReason::LocalStop);
    return true;
}

void MqttClient::setMessageHandler(MessageHandler handler) {
    m_handler = handler;
}

void MqttClient::setEventHandler(EventHandler handler) {
    m_eventHandler = handler;
}

bool MqttClient::validConfig() const {
    return validText(m_config.host) && validText(m_config.clientId);
}

uint16_t MqttClient::nextPacketId() {
    const uint16_t packetId = m_nextPacketId++;
    if (m_nextPacketId == 0) {
        m_nextPacketId = 1;
    }

    return packetId;
}

bool MqttClient::sendConnect() {
    size_t offset = 0;

    if (!writeMqttStringToBuffer(m_packetBuffer, sizeof(m_packetBuffer), offset, "MQTT")) {
        return false;
    }

    if (offset + 4 > sizeof(m_packetBuffer)) {
        return false;
    }

    uint8_t connectFlags = 0;
    if (m_config.cleanSession) {
        connectFlags |= 0x02;
    }
    if (validText(m_config.username)) {
        connectFlags |= 0x80;
    }
    if (validText(m_config.password)) {
        connectFlags |= 0x40;
    }

    m_packetBuffer[offset++] = 4;
    m_packetBuffer[offset++] = connectFlags;
    m_packetBuffer[offset++] = static_cast<uint8_t>((m_config.keepAliveSec >> 8) & 0xFF);
    m_packetBuffer[offset++] = static_cast<uint8_t>(m_config.keepAliveSec & 0xFF);

    if (!writeMqttStringToBuffer(m_packetBuffer, sizeof(m_packetBuffer), offset, m_config.clientId)) {
        return false;
    }

    if (validText(m_config.username) &&
        !writeMqttStringToBuffer(m_packetBuffer, sizeof(m_packetBuffer), offset, m_config.username)) {
        return false;
    }

    if (validText(m_config.password) &&
        !writeMqttStringToBuffer(m_packetBuffer, sizeof(m_packetBuffer), offset, m_config.password)) {
        return false;
    }

    const uint8_t fixedHeader = 0x10;
    if (m_tcp.write(&fixedHeader, 1) != 1 ||
        !writeRemainingLength(m_tcp, offset) ||
        m_tcp.write(m_packetBuffer, offset) != offset) {
        return false;
    }

    m_lastTxMs = millis();
    return true;
}

bool MqttClient::waitForConnAck(uint32_t timeoutMs) {
    MqttPacket packet;
    if (!readPacket(packet, timeoutMs)) {
        return false;
    }

    return packet.header.type == MqttPacket::Type::CONNACK &&
           packet.connAck.returnCode == 0;
}

bool MqttClient::sendPingReq() {
    static const uint8_t packet[] = {0xC0, 0x00};
    if (!connected() || m_tcp.write(packet, sizeof(packet)) != sizeof(packet)) {
        return false;
    }

    m_lastTxMs = millis();
    emitEvent({.type = EventType::PingReqSent});
    return true;
}

bool MqttClient::sendPingResp() {
    static const uint8_t packet[] = {0xD0, 0x00};
    if (!connected() || m_tcp.write(packet, sizeof(packet)) != sizeof(packet)) {
        return false;
    }

    m_lastTxMs = millis();
    return true;
}

bool MqttClient::sendPubAck(uint16_t packetId) {
    const uint8_t packet[] = {
        0x40,
        0x02,
        static_cast<uint8_t>((packetId >> 8) & 0xFF),
        static_cast<uint8_t>(packetId & 0xFF),
    };

    if (!connected() || m_tcp.write(packet, sizeof(packet)) != sizeof(packet)) {
        return false;
    }

    m_lastTxMs = millis();
    return true;
}

bool MqttClient::readPacket(MqttPacket& packet, uint32_t timeoutMs) {
    uint8_t firstByte = 0;
    if (!readByte(m_tcp, firstByte, timeoutMs)) {
        return false;
    }

    packet.header.type = decodeType(firstByte >> 4);
    packet.header.flags = firstByte & 0x0F;

    if (!readRemainingLength(m_tcp, packet.header.remainingLength, timeoutMs)) {
        return false;
    }

    const uint16_t packetLimit = m_config.maxPacketSize < PRTN_MQTT_PACKET_BUFFER_SIZE
                                     ? m_config.maxPacketSize
                                     : PRTN_MQTT_PACKET_BUFFER_SIZE;

    if (packet.header.remainingLength > packetLimit) {
        return false;
    }

    if (!readExact(m_tcp, m_packetBuffer, packet.header.remainingLength, timeoutMs)) {
        return false;
    }

    return parsePacketBody(packet, m_packetBuffer, packet.header.remainingLength);
}

bool MqttClient::parsePacketBody(MqttPacket& packet, const uint8_t* data, size_t len) {
    switch (packet.header.type) {
        case MqttPacket::Type::CONNACK:
            return parseConnAck(packet, data, len);
        case MqttPacket::Type::PUBLISH:
            return parsePublish(packet, data, len);
        case MqttPacket::Type::SUBACK:
            return parseSubAck(packet, data, len);
        case MqttPacket::Type::PUBACK:
            return parsePubAck(packet, data, len);
        case MqttPacket::Type::PINGRESP:
        case MqttPacket::Type::PINGREQ:
        case MqttPacket::Type::DISCONNECT:
            packet.bodyKind = MqttPacket::BodyKind::None;
            return len == 0;
        default:
            return false;
    }
}

bool MqttClient::parsePublish(MqttPacket& packet, const uint8_t* data, size_t len) {
    size_t offset = 0;
    packet.bodyKind = MqttPacket::BodyKind::Publish;
    packet.publish.dup = (packet.header.flags & 0x08) != 0;
    packet.publish.qos = (packet.header.flags & 0x06) >> 1;
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

    packet.publish.payload = data + offset;
    packet.publish.payloadLen = len - offset;
    return true;
}

bool MqttClient::parseConnAck(MqttPacket& packet, const uint8_t* data, size_t len) {
    if (len != 2) {
        return false;
    }

    packet.bodyKind = MqttPacket::BodyKind::ConnAck;
    packet.connAck.sessionPresent = (data[0] & 0x01) != 0;
    packet.connAck.returnCode = data[1];
    return true;
}

bool MqttClient::parseSubAck(MqttPacket& packet, const uint8_t* data, size_t len) {
    size_t offset = 0;
    packet.bodyKind = MqttPacket::BodyKind::SubAck;
    return readUint16(data, len, offset, packet.subAck.packetId) && offset < len;
}

bool MqttClient::parsePubAck(MqttPacket& packet, const uint8_t* data, size_t len) {
    size_t offset = 0;
    packet.bodyKind = MqttPacket::BodyKind::PubAck;
    return readUint16(data, len, offset, packet.pubAck.packetId) && offset == len;
}

void MqttClient::handlePacket(const MqttPacket& packet) {
    switch (packet.header.type) {
        case MqttPacket::Type::PUBLISH:
            if (packet.publish.qos == 1) {
                sendPubAck(packet.publish.packetId);
            }

            emitEvent({.type = EventType::PublishReceived,
                       .topic = packet.publish.topic,
                       .payloadLen = packet.publish.payloadLen});

            if (m_handler) {
                m_handler(packet.publish.topic, packet.publish.payload, packet.publish.payloadLen);
            }
            break;

        case MqttPacket::Type::PINGREQ:
            sendPingResp();
            break;

        case MqttPacket::Type::PINGRESP:
            emitEvent({.type = EventType::PingRespReceived});
            break;

        case MqttPacket::Type::SUBACK:
            emitEvent({.type = EventType::SubAckReceived, .packetId = packet.subAck.packetId});
            break;

        case MqttPacket::Type::PUBACK:
            emitEvent({.type = EventType::PubAckReceived, .packetId = packet.pubAck.packetId});
            break;

        case MqttPacket::Type::DISCONNECT:
            stopWithReason(DisconnectReason::BrokerRequested);
            break;

        default:
            stopWithReason(DisconnectReason::PacketReadFailed);
            break;
    }
}

bool MqttClient::isKeepAliveTimedOut(uint32_t nowMs) const {
    if (m_config.keepAliveSec == 0) {
        return false;
    }

    return nowMs - m_lastRxMs > static_cast<uint32_t>(m_config.keepAliveSec) * 1500UL;
}

void MqttClient::stopWithReason(DisconnectReason reason) {
    const bool wasConnected = m_mqttConnected || m_tcp.connected();
    m_tcp.stop();
    m_mqttConnected = false;

    if (wasConnected) {
        emitEvent({.type = EventType::Disconnected, .reason = reason});
    }
}

void MqttClient::emitEvent(Event event) {
    if (!m_eventHandler) {
        return;
    }

    event.host = m_config.host != nullptr ? m_config.host : "";
    event.port = m_config.port;
    event.clientId = m_config.clientId != nullptr ? m_config.clientId : "";

    m_eventHandler(event);
}
