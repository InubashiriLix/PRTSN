#include "src/prt/BleAgentLightProtocol.h"
#include "src/svc/inc/BleAgentLight/AgentRegistry.h"

#include <array>
#include <cassert>
#include <cstdint>

namespace
{
    namespace protocol = ble_agent_light::protocol;

    struct EventLog
    {
        uint8_t  registeredId {protocol::NoAgent};
        uint8_t  stateId {protocol::NoAgent};
        uint8_t  textId {protocol::NoAgent};
        uint8_t  removedId {protocol::NoAgent};
        unsigned registeredCount {};
        unsigned stateCount {};
        unsigned textCount {};
        unsigned removedCount {};
    };

    void onRegistered(void* context, uint8_t id, const uint8_t*, uint8_t) {
        auto& log        = *static_cast<EventLog*>(context);
        log.registeredId = id;
        ++log.registeredCount;
    }

    void onState(void* context, uint8_t id, protocol::AgentState) {
        auto& log   = *static_cast<EventLog*>(context);
        log.stateId = id;
        ++log.stateCount;
    }

    void onText(void* context, uint8_t id, const uint8_t*, uint8_t) {
        auto& log  = *static_cast<EventLog*>(context);
        log.textId = id;
        ++log.textCount;
    }

    void onRemoved(void* context, uint8_t id) {
        auto& log     = *static_cast<EventLog*>(context);
        log.removedId = id;
        ++log.removedCount;
    }

    ble_agent_light::EventHandlers handlers(EventLog& log) {
        return {
            &log,
            &onRegistered,
            &onState,
            &onText,
            &onRemoved,
        };
    }

    void testProtocol() {
        static_assert(static_cast<uint8_t>(protocol::Status::Ok) == 0x00);
        static_assert(static_cast<uint8_t>(protocol::Status::InvalidOpcode) == 0x01);
        static_assert(static_cast<uint8_t>(protocol::Status::InternalError) == 0x08);

        assert(protocol::decode(nullptr, 0).error().is<protocol::Status::InvalidLength>());

        const uint8_t unknown[] {0x7F};
        assert(protocol::decode(unknown, sizeof(unknown)).error().is<protocol::Status::InvalidOpcode>());

        const uint8_t registration[] {
            static_cast<uint8_t>(protocol::Opcode::Register),
            0x78,
            0x56,
            0x34,
            0x12,
            'A',
        };
        const auto registrationResult = protocol::decode(registration, sizeof(registration));
        assert(registrationResult.is_ok());
        assert(registrationResult.value().key == 0x12345678);
        assert(registrationResult.value().size == 1);
        assert(registrationResult.value().bytes[0] == 'A');

        const uint8_t state[] {
            static_cast<uint8_t>(protocol::Opcode::SetState),
            3,
            static_cast<uint8_t>(protocol::AgentState::Working),
        };
        const auto stateResult = protocol::decode(state, sizeof(state));
        assert(stateResult.is_ok());
        assert(stateResult.value().agentId == 3);
        assert(stateResult.value().state == protocol::AgentState::Working);

        const uint8_t invalidState[] {
            static_cast<uint8_t>(protocol::Opcode::SetState),
            0,
            0xFF,
        };
        assert(protocol::decode(invalidState, sizeof(invalidState)).error().is<protocol::Status::InvalidState>());

        const uint8_t invalidUtf8[] {
            static_cast<uint8_t>(protocol::Opcode::SetText),
            0,
            0xC0,
        };
        assert(protocol::decode(invalidUtf8, sizeof(invalidUtf8)).error().is<protocol::Status::InvalidUtf8>());

        const auto frame = protocol::resultFrame(
            static_cast<uint8_t>(protocol::Opcode::Register),
            protocol::Status::Ok,
            4);
        assert((frame == std::array<uint8_t, 4> {0x80, 0x01, 0x00, 0x04}));
    }

    void testRegistry() {
        EventLog                       log;
        EventLog                       secondLog;
        ble_agent_light::AgentRegistry registry;
        const uint8_t                  name[] {'A'};

        assert(registry.subscribe(handlers(log)).is_ok());
        assert(registry.subscribe(handlers(secondLog)).is_ok());
        assert(registry.subscribe(handlers(log)).error().is<ble_agent_light::AgentRegistry::Detail::ALREADY_SUBSCRIBED>());

        for (uint32_t key = 1; key <= protocol::AgentCount; ++key) {
            const auto result = registry.registerAgent(key, name, sizeof(name));
            assert(result.is_ok());
            assert(result.value() == key - 1);
        }
        assert(log.registeredCount == protocol::AgentCount);
        assert(secondLog.registeredCount == protocol::AgentCount);

        for (uint8_t id = 0; id < protocol::AgentCount; ++id) {
            const auto agent = registry.agent(id);
            assert(agent.has_value());
            assert(agent->key == static_cast<uint32_t>(id) + 1);
            assert(agent->nameSize == sizeof(name));
            assert(agent->name[0] == 'A');
        }
        assert(!registry.agent(protocol::NoAgent).has_value());

        const auto duplicate = registry.registerAgent(1, name, sizeof(name));
        assert(duplicate.is_ok());
        assert(duplicate.value() == 0);
        assert(log.registeredId == 0);

        const auto full = registry.registerAgent(100, name, sizeof(name));
        assert(full.is_err());
        assert(full.error().is<protocol::Status::NoFreeSlot>());

        assert(registry.setState(0, protocol::AgentState::Working).is_ok());
        assert(log.stateCount == 1 && log.stateId == 0);
        const uint8_t text[] {'O', 'K'};
        assert(registry.setText(0, text, sizeof(text)).is_ok());
        assert(log.textCount == 1 && log.textId == 0);
        const auto changed = registry.agent(0);
        assert(changed.has_value());
        assert(changed->state == protocol::AgentState::Working);
        assert(changed->textSize == sizeof(text));
        assert(changed->text[0] == 'O' && changed->text[1] == 'K');
        assert(registry.setState(protocol::NoAgent, protocol::AgentState::Idle).error().is<protocol::Status::UnknownAgent>());
        assert(registry.setText(0, nullptr, 1).error().is<protocol::Status::InvalidLength>());

        assert(registry.unregisterAgent(0).is_ok());
        assert(log.removedCount == 1 && log.removedId == 0);
        assert(secondLog.removedCount == 1 && secondLog.removedId == 0);
        assert(!registry.agent(0).has_value());
        assert(registry.unregisterAgent(0).error().is<protocol::Status::UnknownAgent>());

        const uint8_t replacementName[] {'B'};
        const auto    replacement = registry.registerAgent(100, replacementName, sizeof(replacementName));
        assert(replacement.is_ok());
        assert(replacement.value() == 0);

        assert(registry.registerAgent(0, name, sizeof(name)).error().is<protocol::Status::InvalidLength>());
        assert(registry.registerAgent(101, nullptr, 1).error().is<protocol::Status::InvalidLength>());
    }

    void testSubscriberCapacity() {
        ble_agent_light::AgentRegistry                                           registry;
        std::array<EventLog, ble_agent_light::AgentRegistry::MaxSubscribers + 1> logs;

        assert(registry.subscribe({}).error().is<ble_agent_light::AgentRegistry::Detail::INVALID_SUBSCRIBER>());
        for (std::size_t index = 0; index < ble_agent_light::AgentRegistry::MaxSubscribers; ++index)
            assert(registry.subscribe(handlers(logs[index])).is_ok());
        assert(registry.subscribe(handlers(logs.back())).error().is<ble_agent_light::AgentRegistry::Detail::SUBSCRIBER_FULL>());

        const uint8_t name[] {'A'};
        assert(registry.registerAgent(1, name, sizeof(name)).is_ok());
        for (std::size_t index = 0; index < ble_agent_light::AgentRegistry::MaxSubscribers; ++index)
            assert(logs[index].registeredCount == 1);
        assert(logs.back().registeredCount == 0);
    }

    void testLeaseAndWraparound() {
        EventLog                       log;
        ble_agent_light::AgentRegistry registry;
        const uint8_t                  name[] {'A'};
        assert(registry.subscribe(handlers(log)).is_ok());
        assert(registry.registerAgent(1, name, sizeof(name)).is_ok());

        const uint32_t start = UINT32_MAX - 10'000;
        registry.beginLease(start);
        registry.reap(start + 20'000);
        assert(log.removedCount == 0);
        const auto leased = registry.agent(0);
        assert(leased.has_value() && !leased->active);
        registry.reap(start + 35'000);
        assert(log.removedCount == 1 && log.removedId == 0);
        assert(!registry.agent(0).has_value());
    }
}

int main() {
    testProtocol();
    testRegistry();
    testSubscriberCapacity();
    testLeaseAndWraparound();
}
