#pragma once

#include "src/prt/BleAgentLightProtocol.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#if defined(ARDUINO_ARCH_ESP32)
#include "freertos/FreeRTOS.h"
#else
#include <mutex>
#endif

namespace ble_agent_light
{
    /**
     * @brief 一个无所有权的 AgentRegistry 事件接收器。
     * A non-owning AgentRegistry event sink.
     *
     * 回调中的 name/text 指针只在该次回调期间有效。需要延迟处理时，请保存 id，
     * 稍后通过 `AgentRegistry::agent(id)` 获取并发安全的快照。
     */
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
    public:
        static constexpr std::size_t MaxSubscribers = 4;

        /**
         * @brief 指定 Agent 槽位的只读快照。A read-only snapshot of one agent slot.
         *
         * `agent(id)` 每次只复制这一项；DisplayService 不需要维护第二份 Agent 容器。
         */
        struct AgentSnapshot
        {
            bool                                   active {};
            uint32_t                               key {};
            protocol::AgentState                   state {protocol::AgentState::Idle};
            uint8_t                                nameSize {};
            std::array<uint8_t, protocol::MaxName> name {};
            uint8_t                                textSize {};
            std::array<uint8_t, protocol::MaxText> text {};
        };

        enum class Detail : uint8_t
        {
            INVALID_SUBSCRIBER = 1,
            ALREADY_SUBSCRIBED,
            SUBSCRIBER_FULL,
        };

        using SubscribeResult = Result<void, ErrorSet<Detail::INVALID_SUBSCRIBER, Detail::ALREADY_SUBSCRIBED, Detail::SUBSCRIBER_FULL>>;
        using RegisterErrors  = ErrorSet<
            protocol::Status::InvalidLength,
            protocol::Status::NoFreeSlot>;
        using RegisterResult = Result<uint8_t, RegisterErrors>;
        using MutationErrors = ErrorSet<
            protocol::Status::InvalidLength,
            protocol::Status::UnknownAgent>;
        using MutationResult = Result<void, MutationErrors>;

        AgentRegistry() = default;

        AgentRegistry(const AgentRegistry&)            = delete;
        AgentRegistry& operator=(const AgentRegistry&) = delete;

        /**
         * @brief 注册一个长生命周期事件接收器。Subscribe a long-lived event sink.
         *
         * 固定容量、无堆分配。事件按订阅顺序发送；订阅者必须比 Registry 活得更久。
         */
        [[nodiscard]] SubscribeResult subscribe(EventHandlers handlers) {
            if (!validHandlers(handlers))
                return Err<Detail::INVALID_SUBSCRIBER>("AgentRegistry subscriber has no callbacks");

            Guard guard {*this};
            for (std::size_t index = 0; index < m_subscriberCount; ++index) {
                if (sameHandlers(m_subscribers[index], handlers))
                    return Err<Detail::ALREADY_SUBSCRIBED>("AgentRegistry subscriber is already registered");
            }
            if (m_subscriberCount >= m_subscribers.size())
                return Err<Detail::SUBSCRIBER_FULL>("AgentRegistry subscriber table is full");

            m_subscribers[m_subscriberCount++] = handlers;
            return Ok();
        }

        [[nodiscard]] RegisterResult registerAgent(
            uint32_t       key,
            const uint8_t* name,
            uint8_t        size) {
            if (key == 0 || name == nullptr || size == 0 || size > protocol::MaxName)
                return Err<protocol::Status::InvalidLength>("Agent key or name is invalid");

            uint8_t       registeredId = protocol::NoAgent;
            AgentSnapshot snapshot;
            {
                Guard guard {*this};
                for (uint8_t id = 0; id < m_slots.size(); ++id) {
                    Slot& slot = m_slots[id];
                    if (slot.used && slot.key == key) {
                        slot.active = true;
                        setName(slot, name, size);
                        registeredId = id;
                        snapshot     = makeSnapshot(slot);
                        break;
                    }
                }

                if (registeredId == protocol::NoAgent) {
                    for (uint8_t id = 0; id < m_slots.size(); ++id) {
                        Slot& slot = m_slots[id];
                        if (!slot.used) {
                            slot        = Slot {};
                            slot.used   = true;
                            slot.active = true;
                            slot.key    = key;
                            slot.state  = protocol::AgentState::Idle;
                            setName(slot, name, size);
                            registeredId = id;
                            snapshot     = makeSnapshot(slot);
                            break;
                        }
                    }
                }
            }

            if (registeredId == protocol::NoAgent)
                return Err<protocol::Status::NoFreeSlot>();

            notifyRegistered(registeredId, snapshot.name.data(), snapshot.nameSize);
            return Ok(registeredId);
        }

        [[nodiscard]] MutationResult setState(uint8_t id, protocol::AgentState state) {
            {
                Guard guard {*this};
                if (!isActiveUnlocked(id))
                    return Err<protocol::Status::UnknownAgent>();
                m_slots[id].state = state;
            }
            notifyState(id, state);
            return Ok();
        }

        [[nodiscard]] MutationResult setText(uint8_t id, const uint8_t* text, uint8_t size) {
            if ((text == nullptr && size != 0) || size > protocol::MaxText)
                return Err<protocol::Status::InvalidLength>("Agent text is invalid");

            AgentSnapshot snapshot;
            {
                Guard guard {*this};
                if (!isActiveUnlocked(id))
                    return Err<protocol::Status::UnknownAgent>();
                setText(m_slots[id], text, size);
                snapshot = makeSnapshot(m_slots[id]);
            }
            notifyText(id, snapshot.text.data(), snapshot.textSize);
            return Ok();
        }

        [[nodiscard]] MutationResult unregisterAgent(uint8_t id) {
            {
                Guard guard {*this};
                if (id >= m_slots.size() || !m_slots[id].used)
                    return Err<protocol::Status::UnknownAgent>();
                m_slots[id] = Slot {};
            }
            notifyRemoved(id);
            return Ok();
        }

        /**
         * @brief 获取一个槽位的并发安全快照。Get a thread-safe snapshot by display slot id.
         * @return `id` 不在 0..6 或槽位空闲时返回 `std::nullopt`。
         */
        [[nodiscard]] std::optional<AgentSnapshot> agent(uint8_t id) const {
            Guard guard {*this};
            if (id >= m_slots.size() || !m_slots[id].used)
                return std::nullopt;
            return makeSnapshot(m_slots[id]);
        }

        void beginLease(uint32_t nowMs) {
            Guard guard {*this};
            for (Slot& slot : m_slots) {
                if (slot.used && slot.active) {
                    slot.active       = false;
                    slot.leaseStarted = nowMs;
                }
            }
        }

        void reap(uint32_t nowMs) {
            uint8_t removedMask = 0;
            {
                Guard guard {*this};
                for (uint8_t id = 0; id < m_slots.size(); ++id) {
                    Slot& slot = m_slots[id];
                    if (slot.used && !slot.active && elapsed(nowMs, slot.leaseStarted, protocol::LeaseMs)) {
                        slot = Slot {};
                        removedMask |= slotBit(id);
                    }
                }
            }

            for (uint8_t id = 0; id < protocol::AgentCount; ++id) {
                if ((removedMask & slotBit(id)) != 0)
                    notifyRemoved(id);
            }
        }

    private:
        struct Slot
        {
            bool                                   used {};
            bool                                   active {};
            uint32_t                               key {};
            uint32_t                               leaseStarted {};
            protocol::AgentState                   state {protocol::AgentState::Idle};
            uint8_t                                nameSize {};
            std::array<uint8_t, protocol::MaxName> name {};
            uint8_t                                textSize {};
            std::array<uint8_t, protocol::MaxText> text {};
        };

        class Guard
        {
        public:
            explicit Guard(const AgentRegistry& registry) : m_registry(registry) {
                m_registry.lock();
            }
            ~Guard() {
                m_registry.unlock();
            }

            Guard(const Guard&)            = delete;
            Guard& operator=(const Guard&) = delete;

        private:
            const AgentRegistry& m_registry;
        };

        struct SubscriberSnapshot
        {
            std::array<EventHandlers, MaxSubscribers> handlers {};
            std::size_t                               count {};
        };

        [[nodiscard]] static bool elapsed(uint32_t now, uint32_t then, uint32_t duration) {
            return static_cast<uint32_t>(now - then) >= duration;
        }

        [[nodiscard]] bool isActiveUnlocked(uint8_t id) const {
            return id < m_slots.size() && m_slots[id].used && m_slots[id].active;
        }

        [[nodiscard]] static constexpr uint8_t slotBit(uint8_t id) {
            return static_cast<uint8_t>(uint8_t {1} << id);
        }

        [[nodiscard]] static bool validHandlers(const EventHandlers& handlers) {
            return handlers.onRegistered != nullptr ||
                   handlers.onState != nullptr ||
                   handlers.onText != nullptr ||
                   handlers.onRemoved != nullptr;
        }

        [[nodiscard]] static bool sameHandlers(const EventHandlers& lhs, const EventHandlers& rhs) {
            return lhs.context == rhs.context &&
                   lhs.onRegistered == rhs.onRegistered &&
                   lhs.onState == rhs.onState &&
                   lhs.onText == rhs.onText &&
                   lhs.onRemoved == rhs.onRemoved;
        }

        static void setName(Slot& slot, const uint8_t* name, uint8_t size) {
            slot.name.fill(0);
            slot.nameSize = size;
            std::copy_n(name, size, slot.name.begin());
        }

        static void setText(Slot& slot, const uint8_t* text, uint8_t size) {
            slot.text.fill(0);
            slot.textSize = size;
            if (size != 0)
                std::copy_n(text, size, slot.text.begin());
        }

        [[nodiscard]] static AgentSnapshot makeSnapshot(const Slot& slot) {
            return {
                .active   = slot.active,
                .key      = slot.key,
                .state    = slot.state,
                .nameSize = slot.nameSize,
                .name     = slot.name,
                .textSize = slot.textSize,
                .text     = slot.text,
            };
        }

        [[nodiscard]] SubscriberSnapshot subscribers() const {
            Guard              guard {*this};
            SubscriberSnapshot snapshot;
            snapshot.count = m_subscriberCount;
            std::copy_n(m_subscribers.begin(), snapshot.count, snapshot.handlers.begin());
            return snapshot;
        }

        void notifyRegistered(uint8_t id, const uint8_t* name, uint8_t size) const {
            const SubscriberSnapshot snapshot = subscribers();
            for (std::size_t index = 0; index < snapshot.count; ++index) {
                const EventHandlers& handler = snapshot.handlers[index];
                if (handler.onRegistered != nullptr)
                    handler.onRegistered(handler.context, id, name, size);
            }
        }

        void notifyState(uint8_t id, protocol::AgentState state) const {
            const SubscriberSnapshot snapshot = subscribers();
            for (std::size_t index = 0; index < snapshot.count; ++index) {
                const EventHandlers& handler = snapshot.handlers[index];
                if (handler.onState != nullptr)
                    handler.onState(handler.context, id, state);
            }
        }

        void notifyText(uint8_t id, const uint8_t* text, uint8_t size) const {
            const SubscriberSnapshot snapshot = subscribers();
            for (std::size_t index = 0; index < snapshot.count; ++index) {
                const EventHandlers& handler = snapshot.handlers[index];
                if (handler.onText != nullptr)
                    handler.onText(handler.context, id, text, size);
            }
        }

        void notifyRemoved(uint8_t id) const {
            const SubscriberSnapshot snapshot = subscribers();
            for (std::size_t index = 0; index < snapshot.count; ++index) {
                const EventHandlers& handler = snapshot.handlers[index];
                if (handler.onRemoved != nullptr)
                    handler.onRemoved(handler.context, id);
            }
        }

        void lock() const {
#if defined(ARDUINO_ARCH_ESP32)
            portENTER_CRITICAL(&m_lock);
#else
            m_lock.lock();
#endif
        }

        void unlock() const {
#if defined(ARDUINO_ARCH_ESP32)
            portEXIT_CRITICAL(&m_lock);
#else
            m_lock.unlock();
#endif
        }

        std::array<Slot, protocol::AgentCount>    m_slots {};
        std::array<EventHandlers, MaxSubscribers> m_subscribers {};
        std::size_t                               m_subscriberCount {};
#if defined(ARDUINO_ARCH_ESP32)
        mutable portMUX_TYPE m_lock = portMUX_INITIALIZER_UNLOCKED;
#else
        mutable std::mutex m_lock;
#endif
    };
}
