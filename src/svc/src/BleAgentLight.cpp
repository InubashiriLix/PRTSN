#include "src/svc/inc/BleAgentLight/BleAgentLight.h"

#include <Arduino.h>
#include <BLE2902.h>
#include <BLEAdvertising.h>
#include <BLEDevice.h>
#include <BLESecurity.h>

namespace ble_agent_light
{
    BleGattTransport::BleGattTransport(Config config, Callbacks callbacks)
        : m_config(config),
          m_callbacks(callbacks),
          m_adapter(*this) {}

    BleGattTransport::SetupResult BleGattTransport::setup() {
        if (m_isSetup)
            return Err<SetupErrorCode::AlreadySetup>();

        if (!BLEDevice::init(m_config.deviceName))
            return Err<SetupErrorCode::BleInitFailed>();

        BLESecurity::setCapability(ESP_IO_CAP_NONE);
        BLESecurity::setAuthenticationMode(true, false, true);

        m_server = BLEDevice::createServer();
        if (m_server == nullptr)
            return Err<SetupErrorCode::CreateServerFailed>();
        m_server->setCallbacks(&m_adapter);
        m_server->advertiseOnDisconnect(true);

        BLEService* service = m_server->createService(m_config.serviceUuid);
        if (service == nullptr)
            return Err<SetupErrorCode::CreateServiceFailed>();

        BLECharacteristic* info = service->createCharacteristic(
            m_config.infoUuid,
            BLECharacteristic::PROPERTY_READ);
        BLECharacteristic* commandRx = service->createCharacteristic(
            m_config.commandUuid,
            BLECharacteristic::PROPERTY_WRITE);
        m_eventTx = service->createCharacteristic(
            m_config.eventUuid,
            BLECharacteristic::PROPERTY_INDICATE);
        if (info == nullptr || commandRx == nullptr || m_eventTx == nullptr) {
            m_eventTx = nullptr;
            return Err<SetupErrorCode::CreateCharacteristicFailed>();
        }

        info->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
        commandRx->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);
        m_eventTx->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);

        info->setValue(protocol::ProtocolInfo.data(), protocol::ProtocolInfo.size());
        commandRx->setCallbacks(&m_adapter);
        m_eventTx->addDescriptor(new BLE2902());

        if (!service->start())
            return Err<SetupErrorCode::ServiceStartFailed>();

        BLEAdvertising* advertising = BLEDevice::getAdvertising();
        if (advertising == nullptr)
            return Err<SetupErrorCode::GetAdvertisingFailed>();
        advertising->addServiceUUID(m_config.serviceUuid);
        advertising->setScanResponse(true);
        if (!advertising->start())
            return Err<SetupErrorCode::AdvertisingStartFailed>();

        m_isSetup = true;
        return Ok();
    }

    BleGattTransport::SendResult BleGattTransport::send(const uint8_t* data, std::size_t size) {
        if (!m_isSetup || m_eventTx == nullptr)
            return Err<SendErrorCode::NotInitialized>();
        if (!m_connected)
            return Err<SendErrorCode::NotConnected>();

        m_eventTx->setValue(data, size);
        m_eventTx->indicate();
        return Ok();
    }

    void BleGattTransport::CallbackAdapter::onConnect(BLEServer*) {
        m_owner.handleConnected();
    }

    void BleGattTransport::CallbackAdapter::onDisconnect(BLEServer*) {
        m_owner.handleDisconnected();
    }

    void BleGattTransport::CallbackAdapter::onWrite(BLECharacteristic* characteristic) {
        m_owner.handleWrite(characteristic);
    }

    void BleGattTransport::handleConnected() {
        m_connected = true;
        if (m_callbacks.onConnected != nullptr)
            m_callbacks.onConnected(m_callbacks.context);
    }

    void BleGattTransport::handleDisconnected() {
        m_connected = false;
        if (m_callbacks.onDisconnected != nullptr)
            m_callbacks.onDisconnected(m_callbacks.context);
    }

    void BleGattTransport::handleWrite(BLECharacteristic* characteristic) {
        if (characteristic == nullptr || m_callbacks.onWrite == nullptr)
            return;
        m_callbacks.onWrite(
            m_callbacks.context,
            characteristic->getData(),
            characteristic->getLength());
    }
}

BleAgentLightService::BleAgentLightService()
    : BleAgentLightService(Config {}) {}

BleAgentLightService::BleAgentLightService(const Config& config)
    : m_config(config),
      m_registry(config.handlers),
      m_transport(
          {
              config.deviceName,
              config.serviceUuid,
              config.infoUuid,
              config.commandUuid,
              config.eventUuid,
          },
          makeTransportCallbacks(this)) {}

BleAgentLightService::SetupResult BleAgentLightService::setup() {
    return m_transport.setup();
}

void BleAgentLightService::poll(uint32_t nowMs) {
    m_registry.reap(nowMs);
}

ble_agent_light::BleGattTransport::Callbacks BleAgentLightService::makeTransportCallbacks(
    BleAgentLightService* service) {
    return {
        service,
        &BleAgentLightService::onConnected,
        &BleAgentLightService::onDisconnected,
        &BleAgentLightService::onWrite,
    };
}

void BleAgentLightService::onConnected(void*) {}

void BleAgentLightService::onDisconnected(void* context) {
    static_cast<BleAgentLightService*>(context)->handleDisconnected();
}

void BleAgentLightService::onWrite(void* context, const uint8_t* data, std::size_t size) {
    static_cast<BleAgentLightService*>(context)->handleWrite(data, size);
}

void BleAgentLightService::handleDisconnected() {
    m_registry.beginLease(millis());
}

void BleAgentLightService::handleWrite(const uint8_t* data, std::size_t size) {
    const uint8_t hintedOpcode = size == 0 || data == nullptr ? 0 : data[0];
    const auto    parsed       = ble_agent_light::protocol::decode(data, size);
    if (parsed.is_err()) {
        sendStatus(hintedOpcode, parsed.error().native());
        return;
    }

    const Command& command = parsed.value();
    const uint8_t  opcode  = static_cast<uint8_t>(command.opcode);
    switch (command.opcode) {
        case Opcode::Register: {
            const auto result = m_registry.registerAgent(
                command.key,
                command.bytes.data(),
                command.size);
            if (result.is_ok())
                sendStatus(opcode, Status::Ok, result.value());
            else
                sendStatus(opcode, result.error().native());
            return;
        }

        case Opcode::SetState:
            respondMutation(opcode, command.agentId, m_registry.setState(command.agentId, command.state));
            return;

        case Opcode::SetText:
            respondMutation(
                opcode,
                command.agentId,
                m_registry.setText(command.agentId, command.bytes.data(), command.size));
            return;

        case Opcode::Unregister:
            respondMutation(opcode, command.agentId, m_registry.unregisterAgent(command.agentId));
            return;
    }
}

void BleAgentLightService::respondMutation(
    uint8_t               opcode,
    uint8_t               id,
    const MutationResult& result) {
    sendStatus(
        opcode,
        result.is_ok() ? Status::Ok : result.error().native(),
        id);
}

void BleAgentLightService::sendStatus(uint8_t opcode, Status status, uint8_t id) {
    const auto frame = ble_agent_light::protocol::resultFrame(opcode, status, id);
    (void)m_transport.send(frame.data(), frame.size());
}
