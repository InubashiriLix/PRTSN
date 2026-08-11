#pragma once

#include "src/prt/BleAgentLightProtocol.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace ble_agent_light
{
    struct EventHandlers
    {
        using Registered = void (*)(void*, uint8_t, const uint8_t*, uint8_t);
        using State      = void (*)(void*, uint8_t, protocol::AgentState);
        using Text       = void (*)(void*, uint8_t, const uint8_t*, uint8_t);
        using Removed    = void (*)(void*, uint8_t);

        void*      context {};
        Registered onRegistered {};
        State      onState {};
        Text       onText {};
        Removed    onRemoved {};
    };

    class AgentRegistry
    {
        struct Slot
        {
            bool                                   used {};
            bool                                   active {};
            uint32_t                               key {};
            uint32_t                               leaseStarted {};
            protocol::AgentState                   state {protocol::AgentState::Idle};
            uint8_t                                nameSize {};
            std::array<uint8_t, protocol::MaxName> name {};
        };

    public:
        using RegisterErrors = ErrorSet<protocol::Status::NoFreeSlot>;
        using RegisterResult = Result<uint8_t, RegisterErrors>;
        using MutationErrors = ErrorSet<protocol::Status::UnknownAgent>;
        using MutationResult = Result<void, MutationErrors>;

        explicit AgentRegistry(EventHandlers handlers = {})
            : m_handlers(handlers) {}

        [[nodiscard]] RegisterResult registerAgent(
            uint32_t       key,
            const uint8_t* name,
            uint8_t        size) {
            for (uint8_t id = 0; id < m_slots.size(); ++id) {
                Slot& slot = m_slots[id];
                if (slot.used && slot.key == key) {
                    slot.active = true;
                    setName(slot, name, size);
                    notifyRegistered(id, name, size);
                    return Ok(id);
                }
            }

            for (uint8_t id = 0; id < m_slots.size(); ++id) {
                Slot& slot = m_slots[id];
                if (!slot.used) {
                    slot.used   = true;
                    slot.active = true;
                    slot.key    = key;
                    slot.state  = protocol::AgentState::Idle;
                    setName(slot, name, size);
                    notifyRegistered(id, name, size);
                    return Ok(id);
                }
            }

            return Err<protocol::Status::NoFreeSlot>();
        }

        [[nodiscard]] MutationResult setState(uint8_t id, protocol::AgentState state) {
            if (!isActive(id))
                return Err<protocol::Status::UnknownAgent>();
            m_slots[id].state = state;
            if (m_handlers.onState != nullptr)
                m_handlers.onState(m_handlers.context, id, state);
            return Ok();
        }

        [[nodiscard]] MutationResult setText(uint8_t id, const uint8_t* text, uint8_t size) {
            if (!isActive(id))
                return Err<protocol::Status::UnknownAgent>();
            if (m_handlers.onText != nullptr)
                m_handlers.onText(m_handlers.context, id, text, size);
            return Ok();
        }

        [[nodiscard]] MutationResult unregisterAgent(uint8_t id) {
            if (id >= m_slots.size() || !m_slots[id].used)
                return Err<protocol::Status::UnknownAgent>();
            m_slots[id] = Slot {};
            notifyRemoved(id);
            return Ok();
        }

        void beginLease(uint32_t nowMs) {
            for (Slot& slot : m_slots) {
                if (slot.used && slot.active) {
                    slot.active       = false;
                    slot.leaseStarted = nowMs;
                }
            }
        }

        void reap(uint32_t nowMs) {
            for (uint8_t id = 0; id < m_slots.size(); ++id) {
                Slot& slot = m_slots[id];
                if (slot.used && !slot.active && elapsed(nowMs, slot.leaseStarted, protocol::LeaseMs)) {
                    slot = Slot {};
                    notifyRemoved(id);
                }
            }
        }

    private:
        [[nodiscard]] static bool elapsed(uint32_t now, uint32_t then, uint32_t duration) {
            return static_cast<uint32_t>(now - then) >= duration;
        }

        [[nodiscard]] bool isActive(uint8_t id) const {
            return id < m_slots.size() && m_slots[id].used && m_slots[id].active;
        }

        static void setName(Slot& slot, const uint8_t* name, uint8_t size) {
            slot.nameSize = size;
            for (uint8_t i = 0; i < size; ++i)
                slot.name[i] = name[i];
        }

        void notifyRegistered(uint8_t id, const uint8_t* name, uint8_t size) const {
            if (m_handlers.onRegistered != nullptr)
                m_handlers.onRegistered(m_handlers.context, id, name, size);
        }

        void notifyRemoved(uint8_t id) const {
            if (m_handlers.onRemoved != nullptr)
                m_handlers.onRemoved(m_handlers.context, id);
        }

        std::array<Slot, protocol::AgentCount> m_slots {};
        EventHandlers                          m_handlers;
    };
}
