#pragma once

#include "src/alg/inc/alg_crc.h"

#include <cstddef>
#include <cstdint>

/**
 * ESP-NOW device layer sends this protocol as its data/body bytes.
 *
 * Frame layout:
 *
 *   magic       2 bytes  fixed 0x5054, little-endian on wire
 *   version     1 byte
 *   headerLen   1 byte   fixed 12 for V1
 *   type        1 byte
 *   flags       1 byte
 *   seq         2 bytes  little-endian
 *   ackSeq      2 bytes  little-endian
 *   payloadLen  2 bytes  little-endian
 *   payload     N bytes
 *   crc16       2 bytes  little-endian, over header + payload
 *
 * Total packet length must stay <= MaxPacketLen for ESP-NOW v1 compatibility.
 */
struct EspNowProtocol
{
    enum class Type : uint8_t
    {
        HEARTBEAT      = 0x01,
        DISCOVERY      = 0x02,
        NODE_INFO      = 0x03,
        COMMAND        = 0x10,
        COMMAND_RESULT = 0x11,
        DATA           = 0x20,
        ACK            = 0x7F,
        ERROR          = 0x80,
    };

    enum class Version : uint8_t
    {
        V1 = 0x01,
        V2 = 0x02,
    };

    enum Flag : uint8_t
    {
        FLAG_NONE     = 0x00,
        FLAG_NEED_ACK = 0x01,
        FLAG_IS_ACK   = 0x02,
        FLAG_IS_ERROR = 0x04,
    };

    enum class Result : uint8_t
    {
        OK = 0,
        INVALID_ARGUMENT,
        BUFFER_TOO_SMALL,
        PACKET_TOO_SMALL,
        INVALID_MAGIC,
        UNSUPPORTED_VERSION,
        INVALID_HEADER_LEN,
        INVALID_LENGTH,
        INVALID_CRC,
    };

    static constexpr uint16_t Magic         = 0x5054; // 'P''T'
    static constexpr uint8_t  HeaderLen     = 12;
    static constexpr uint8_t  CrcLen        = 2;
    static constexpr uint16_t MaxPacketLen  = 250;
    static constexpr uint16_t MaxPayloadLen = MaxPacketLen - HeaderLen - CrcLen;

    struct Header
    {
        uint16_t magic      = Magic;
        Version  version    = Version::V1;
        uint8_t  headerLen  = HeaderLen;
        Type     type       = Type::DATA;
        uint8_t  flags      = FLAG_NONE;
        uint16_t seq        = 0;
        uint16_t ackSeq     = 0;
        uint16_t payloadLen = 0;
    };

    struct Packet
    {
        Header         header;
        const uint8_t* payload    = nullptr;
        size_t         payloadLen = 0;
    };

    static constexpr size_t packetLen(size_t payloadLen) {
        return HeaderLen + payloadLen + CrcLen;
    }

    static constexpr bool hasFlag(uint8_t flags, Flag flag) {
        return (flags & static_cast<uint8_t>(flag)) != 0;
    }

    static Result encode(uint8_t*       out,
                         size_t         outCapacity,
                         size_t&        outLen,
                         Type           type,
                         uint16_t       seq,
                         const uint8_t* payload    = nullptr,
                         size_t         payloadLen = 0,
                         uint8_t        flags      = FLAG_NONE,
                         uint16_t       ackSeq     = 0,
                         Version        version    = Version::V1) {
        if (out == nullptr) {
            return Result::INVALID_ARGUMENT;
        }

        if (payloadLen > 0 && payload == nullptr) {
            return Result::INVALID_ARGUMENT;
        }

        if (payloadLen > MaxPayloadLen) {
            return Result::INVALID_LENGTH;
        }

        const size_t totalLen = packetLen(payloadLen);
        if (outCapacity < totalLen) {
            return Result::BUFFER_TOO_SMALL;
        }

        writeU16(out, 0, Magic);
        out[2] = static_cast<uint8_t>(version);
        out[3] = HeaderLen;
        out[4] = static_cast<uint8_t>(type);
        out[5] = flags;
        writeU16(out, 6, seq);
        writeU16(out, 8, ackSeq);
        writeU16(out, 10, static_cast<uint16_t>(payloadLen));

        for (size_t i = 0; i < payloadLen; ++i) {
            out[HeaderLen + i] = payload[i];
        }

        CRCCalculator::appendCRC16(out, static_cast<uint32_t>(totalLen));
        outLen = totalLen;
        return Result::OK;
    }

    static Result decode(const uint8_t* data, size_t len, Packet& outPacket) {
        outPacket = {};

        if (data == nullptr) {
            return Result::INVALID_ARGUMENT;
        }

        if (len < packetLen(0)) {
            return Result::PACKET_TOO_SMALL;
        }

        if (readU16(data, 0) != Magic) {
            return Result::INVALID_MAGIC;
        }

        const Version version = static_cast<Version>(data[2]);
        if (version != Version::V1) {
            return Result::UNSUPPORTED_VERSION;
        }

        if (data[3] != HeaderLen) {
            return Result::INVALID_HEADER_LEN;
        }

        const uint16_t payloadLen = readU16(data, 10);
        if (payloadLen > MaxPayloadLen || len != packetLen(payloadLen)) {
            return Result::INVALID_LENGTH;
        }

        if (!CRCCalculator::verifyCRC16(data, static_cast<uint32_t>(len))) {
            return Result::INVALID_CRC;
        }

        outPacket.header.magic      = Magic;
        outPacket.header.version    = version;
        outPacket.header.headerLen  = data[3];
        outPacket.header.type       = static_cast<Type>(data[4]);
        outPacket.header.flags      = data[5];
        outPacket.header.seq        = readU16(data, 6);
        outPacket.header.ackSeq     = readU16(data, 8);
        outPacket.header.payloadLen = payloadLen;
        outPacket.payload           = payloadLen > 0 ? data + HeaderLen : nullptr;
        outPacket.payloadLen        = payloadLen;

        return Result::OK;
    }

private:
    static uint16_t readU16(const uint8_t* data, size_t offset) {
        return static_cast<uint16_t>(data[offset]) | static_cast<uint16_t>(data[offset + 1] << 8);
    }

    static void writeU16(uint8_t* data, size_t offset, uint16_t value) {
        data[offset]     = static_cast<uint8_t>(value & 0xFF);
        data[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    }
};
