#pragma once

#include "src/fw/inc/Result.h"

#include <BLECharacteristic.h>
#include <BLEServer.h>

#include <cstddef>
#include <cstdint>

namespace ble_agent_light
{
    class BleGattTransport
    {
    public:
        struct Config
        {
            const char* deviceName;
            const char* serviceUuid;
            const char* infoUuid;
            const char* commandUuid;
            const char* eventUuid;
        };

        struct Callbacks
        {
            using Connection = void (*)(void*);
            using Write      = void (*)(void*, const uint8_t*, std::size_t);

            void*      context {};
            Connection onConnected {};
            Connection onDisconnected {};
            Write      onWrite {};
        };

        enum class SetupErrorCode : uint8_t
        {
            AlreadySetup,
            BleInitFailed,
            CreateServerFailed,
            CreateServiceFailed,
            CreateCharacteristicFailed,
            ServiceStartFailed,
            GetAdvertisingFailed,
            AdvertisingStartFailed,
        };

        using SetupErrors = ErrorSet<
            SetupErrorCode::AlreadySetup,
            SetupErrorCode::BleInitFailed,
            SetupErrorCode::CreateServerFailed,
            SetupErrorCode::CreateServiceFailed,
            SetupErrorCode::CreateCharacteristicFailed,
            SetupErrorCode::ServiceStartFailed,
            SetupErrorCode::GetAdvertisingFailed,
            SetupErrorCode::AdvertisingStartFailed>;
        using SetupResult = Result<void, SetupErrors>;

        enum class SendErrorCode : uint8_t
        {
            NotInitialized,
            NotConnected,
        };

        using SendErrors = ErrorSet<
            SendErrorCode::NotInitialized,
            SendErrorCode::NotConnected>;
        using SendResult = Result<void, SendErrors>;

        BleGattTransport(Config config, Callbacks callbacks);
        BleGattTransport(const BleGattTransport&)            = delete;
        BleGattTransport& operator=(const BleGattTransport&) = delete;
        BleGattTransport(BleGattTransport&&)                 = delete;
        BleGattTransport& operator=(BleGattTransport&&)      = delete;

        [[nodiscard]] SetupResult setup();
        [[nodiscard]] SendResult  send(const uint8_t* data, std::size_t size);

    private:
        class CallbackAdapter final : public BLEServerCallbacks, public BLECharacteristicCallbacks
        {
        public:
            explicit CallbackAdapter(BleGattTransport& owner)
                : m_owner(owner) {}

            void onConnect(BLEServer*) override;
            void onDisconnect(BLEServer*) override;
            void onWrite(BLECharacteristic* characteristic) override;

        private:
            BleGattTransport& m_owner;
        };

        void handleConnected();
        void handleDisconnected();
        void handleWrite(BLECharacteristic* characteristic);

        Config             m_config;
        Callbacks          m_callbacks;
        CallbackAdapter    m_adapter;
        BLEServer*         m_server    = nullptr;
        BLECharacteristic* m_eventTx   = nullptr;
        bool               m_isSetup   = false;
        bool               m_connected = false;
    };
}
