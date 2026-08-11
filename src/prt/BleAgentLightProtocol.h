#pragma once

#include "src/fw/inc/Result.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace ble_agent_light::protocol
{
    inline constexpr char DefaultServiceUuid[] = "6e291bc2-80c8-484d-a505-49509c3c868e";
    inline constexpr char DefaultCommandUuid[] = "b6c66dd2-4047-473c-93f3-d97d9405330c";
    inline constexpr char DefaultEventUuid[]   = "1c69273f-d0fb-447a-90c1-0c472a0b7b53";
    inline constexpr char DefaultInfoUuid[]    = "c2ef6547-91b4-4367-8b1c-0514c5bb8f42";
    inline constexpr char DefaultDeviceName[]  = "PRTN-AgentPanel";

    inline constexpr std::size_t AgentCount = 7;
    inline constexpr std::size_t MaxName    = 15;
    inline constexpr std::size_t MaxText    = 18;
    inline constexpr std::size_t MaxFrame   = 20;
    inline constexpr uint32_t    LeaseMs    = 30'000;
    inline constexpr uint8_t     NoAgent    = 0xFF;

    enum class Opcode : uint8_t
    {
        Register   = 0x01,
        SetState   = 0x02,
        SetText    = 0x03,
        Unregister = 0x04,
    };

    enum class AgentState : uint8_t
    {
        Off = 0,
        Idle,
        Working,
        WaitPermission,
        WaitOption,
        Done,
        Error,
    };

    enum class Status : uint8_t
    {
        Ok            = 0x00,
        InvalidOpcode = 0x01,
        InvalidLength = 0x02,
        InvalidUtf8   = 0x03,
        NoFreeSlot    = 0x04,
        UnknownAgent  = 0x05,
        InvalidState  = 0x06,
        Busy          = 0x07,
        InternalError = 0x08,
    };

    struct Command
    {
        Opcode                       opcode {};
        uint32_t                     key {};
        uint8_t                      agentId {NoAgent};
        AgentState                   state {AgentState::Off};
        uint8_t                      size {};
        std::array<uint8_t, MaxText> bytes {};
    };

    using DecodeErrors = ErrorSet<
        Status::InvalidLength,
        Status::InvalidOpcode,
        Status::InvalidUtf8,
        Status::InvalidState>;
    using DecodeResult = Result<Command, DecodeErrors>;

    [[nodiscard]] inline bool validUtf8(const uint8_t* data, std::size_t size) {
        std::size_t i = 0;
        while (i < size) {
            const uint8_t leading = data[i++];
            if (leading <= 0x7F)
                continue;

            std::size_t trailing;
            uint8_t     lower = 0x80;
            uint8_t     upper = 0xBF;
            if (leading >= 0xC2 && leading <= 0xDF) {
                trailing = 1;
            }
            else if (leading >= 0xE0 && leading <= 0xEF) {
                trailing = 2;
                if (leading == 0xE0)
                    lower = 0xA0;
                else if (leading == 0xED)
                    upper = 0x9F;
            }
            else if (leading >= 0xF0 && leading <= 0xF4) {
                trailing = 3;
                if (leading == 0xF0)
                    lower = 0x90;
                else if (leading == 0xF4)
                    upper = 0x8F;
            }
            else {
                return false;
            }

            if (i + trailing > size || data[i] < lower || data[i] > upper)
                return false;
            ++i;
            while (--trailing > 0) {
                if ((data[i++] & 0xC0) != 0x80)
                    return false;
            }
        }
        return true;
    }

    [[nodiscard]] inline uint32_t readLe32(const uint8_t* data) {
        return uint32_t {data[0]} |
               (uint32_t {data[1]} << 8) |
               (uint32_t {data[2]} << 16) |
               (uint32_t {data[3]} << 24);
    }

    [[nodiscard]] inline DecodeResult decode(const uint8_t* data, std::size_t size) {
        if (data == nullptr || size == 0 || size > MaxFrame)
            return Err<Status::InvalidLength>();

        Command command {};
        command.opcode = static_cast<Opcode>(data[0]);

        switch (command.opcode) {
            case Opcode::Register:
                if (size < 6 || size > MaxFrame)
                    return Err<Status::InvalidLength>();
                command.key = readLe32(data + 1);
                if (command.key == 0)
                    return Err<Status::InvalidLength>();
                command.size = static_cast<uint8_t>(size - 5);
                if (!validUtf8(data + 5, command.size))
                    return Err<Status::InvalidUtf8>();
                for (uint8_t i = 0; i < command.size; ++i)
                    command.bytes[i] = data[5 + i];
                return Ok(command);

            case Opcode::SetState:
                if (size != 3)
                    return Err<Status::InvalidLength>();
                if (data[2] > static_cast<uint8_t>(AgentState::Error))
                    return Err<Status::InvalidState>();
                command.agentId = data[1];
                command.state   = static_cast<AgentState>(data[2]);
                return Ok(command);

            case Opcode::SetText:
                if (size < 2 || size > MaxFrame)
                    return Err<Status::InvalidLength>();
                command.agentId = data[1];
                command.size    = static_cast<uint8_t>(size - 2);
                if (!validUtf8(data + 2, command.size))
                    return Err<Status::InvalidUtf8>();
                for (uint8_t i = 0; i < command.size; ++i)
                    command.bytes[i] = data[2 + i];
                return Ok(command);

            case Opcode::Unregister:
                if (size != 2)
                    return Err<Status::InvalidLength>();
                command.agentId = data[1];
                return Ok(command);
        }

        return Err<Status::InvalidOpcode>();
    }

    [[nodiscard]] inline constexpr std::array<uint8_t, 4> resultFrame(
        uint8_t opcode,
        Status  status,
        uint8_t id = NoAgent) {
        return {0x80, opcode, static_cast<uint8_t>(status), id};
    }

    inline constexpr std::array<uint8_t, 8> ProtocolInfo {1, 0, 7, 15, 18, 0x0F, 30, 0};
}
