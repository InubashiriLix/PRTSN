#pragma once

#include "src/prt/BleAgentLightProtocol.h"
#include "src/svc/inc/BleAgentLight/AgentRegistry.h"
#include "src/svc/inc/BleAgentLight/BleGattTransport.h"

#include <cstddef>
#include <cstdint>

class SerialConsoleService;

class BleAgentLightService
{
public:
    using Opcode        = ble_agent_light::protocol::Opcode;
    using AgentState    = ble_agent_light::protocol::AgentState;
    using Status        = ble_agent_light::protocol::Status;
    using Command       = ble_agent_light::protocol::Command;
    using EventHandlers = ble_agent_light::EventHandlers;

    inline constexpr static std::size_t AgentCount = ble_agent_light::protocol::AgentCount;
    inline constexpr static std::size_t MaxName    = ble_agent_light::protocol::MaxName;
    inline constexpr static std::size_t MaxText    = ble_agent_light::protocol::MaxText;
    inline constexpr static uint32_t    LeaseMs    = ble_agent_light::protocol::LeaseMs;
    inline constexpr static uint8_t     NoAgent    = ble_agent_light::protocol::NoAgent;

    struct Config
    {
        const char* deviceName  = ble_agent_light::protocol::DefaultDeviceName;
        const char* serviceUuid = ble_agent_light::protocol::DefaultServiceUuid;
        const char* infoUuid    = ble_agent_light::protocol::DefaultInfoUuid;
        const char* commandUuid = ble_agent_light::protocol::DefaultCommandUuid;
        const char* eventUuid   = ble_agent_light::protocol::DefaultEventUuid;
        /** Optional, non-owning diagnostic output. It must outlive this service. */
        SerialConsoleService* console = nullptr;
    };

    using SetupErrorCode = ble_agent_light::BleGattTransport::SetupErrorCode;
    using SetupErrors    = ble_agent_light::BleGattTransport::SetupErrors;
    using SetupResult    = ble_agent_light::BleGattTransport::SetupResult;

    explicit BleAgentLightService(ble_agent_light::AgentRegistry& registry);
    BleAgentLightService(ble_agent_light::AgentRegistry& registry, const Config& config);
    BleAgentLightService(const BleAgentLightService&)            = delete;
    BleAgentLightService& operator=(const BleAgentLightService&) = delete;
    BleAgentLightService(BleAgentLightService&&)                 = delete;
    BleAgentLightService& operator=(BleAgentLightService&&)      = delete;

    [[nodiscard]] SetupResult setup();
    /** 跨核心读取当前 BLE 连接状态。Thread-safe BLE connection-state query. */
    [[nodiscard]] bool connected() const noexcept;
    void               poll(uint32_t nowMs);

private:
    using MutationResult = ble_agent_light::AgentRegistry::MutationResult;

    static ble_agent_light::BleGattTransport::Callbacks makeTransportCallbacks(BleAgentLightService* service);
    static void                                         onConnected(void* context);
    static void                                         onDisconnected(void* context);
    static void                                         onWrite(void* context, const uint8_t* data, std::size_t size);

    void handleConnected();
    void handleDisconnected();
    void handleWrite(const uint8_t* data, std::size_t size);
    void respondMutation(uint8_t opcode, uint8_t id, const MutationResult& result);
    void sendStatus(uint8_t opcode, Status status, uint8_t id = NoAgent);
    void logInfo(const char* format, ...) const __attribute__((format(printf, 2, 3)));
    void logWarn(const char* format, ...) const __attribute__((format(printf, 2, 3)));

    Config                            m_config;
    ble_agent_light::AgentRegistry&   m_registry;
    ble_agent_light::BleGattTransport m_transport;
};
