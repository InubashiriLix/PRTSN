#pragma once
#include <src/fw/inc/Result.h>
#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include "BLEAdvertisedDevice.h"
#include "BLECharacteristic.h"
#include "BLEServer.h"
#include "src/prt/HidProtocol.h"
// the sideeffect of pill make me sleepy
// and the annogying ideas are gone

// TODO: reaname this as BleHidNkroKeyboardMouse
// TODO: make this service hold the reference to the device of keyboard and mouse
// the keybaord should be a scanner with interface in IKeyboardScanDevice, and mouse Interface and Device should be created
//
namespace prt_ble_hid
{

    class BleNkroMouse
    {
    public:
        enum class Detail : uint8_t
        {
            None = 0,
            NotStarted,
            AlreadyStarted,
            NotReady,
            InvalidDeviceNameParam,
            DeviceInitialFailed,
            ServerCreateFailed,
            HidDeviceCreateFailed,
            KeyboardInputCreateFailed,
            KeyboardOutputCreateFailed,
            MouseInputCreateFailed,
            AdvertisingCreateFailed,
            MouseInputNullError,
            KeyboardInputNullError,
        };

    public:
        BleNkroMouse(const char* deviceName) : m_deviceName(deviceName), m_serverCallbacks(*this), m_ledCallbacks(*this) {};

        using SetupResult = Result<void, ErrorSet<Detail::AlreadyStarted, Detail::InvalidDeviceNameParam, Detail::DeviceInitialFailed, Detail::ServerCreateFailed, Detail::HidDeviceCreateFailed, Detail::KeyboardInputCreateFailed, Detail::KeyboardOutputCreateFailed, Detail::MouseInputCreateFailed, BleNkroMouse::Detail::AdvertisingCreateFailed>>;
        [[nodiscard]] SetupResult setup();
        [[nodiscard]] bool        isConnected() const noexcept;
        void                      prepareState() noexcept {
            if (!m_resetPending.exchange(false, std::memory_order_acq_rel)) {
                return;
            }
            m_keyboard = {};
            m_mouseBtn = prt_hid::MouseBtn::None;
        }

        [[nodiscard]] bool isReady() const noexcept {
            return m_started && isConnected();
        }

        using SendKeyboardResult = Result<bool, ErrorSet<Detail::NotStarted, Detail::NotReady>>;
        SendKeyboardResult sendKeyboard();

        using SetKeyResult = Result<bool, ErrorSet<Detail::NotStarted, Detail::NotReady, Detail::KeyboardInputNullError>>;
        [[nodiscard]] SetKeyResult setKey(prt_hid::KeyId key, bool pressed);

        using ReleaseAllKeyResult = Result<bool, ErrorSet<Detail::NotStarted, Detail::NotReady, Detail::KeyboardInputNullError>>;
        [[nodiscard]] ReleaseAllKeyResult releaseAllKeys();

        using MoveMouseResult = Result<bool, ErrorSet<Detail::NotStarted, Detail::NotReady, Detail::MouseInputNullError>>;
        [[nodiscard]] MoveMouseResult moveMouse(int8_t x, int8_t y, int8_t wheel);

        using SetMouseBtnResult = Result<bool, ErrorSet<Detail::NotStarted, Detail::NotReady, Detail::MouseInputNullError>>;
        [[nodiscard]] SetMouseBtnResult setMouseBtn(prt_hid::MouseBtn btn, bool pressed);

        prt_hid::KeyboardLed getKeyboardLeds() const noexcept {
            return m_keyboardLeds.load(std::memory_order_acquire);
        }

    private:
        class ServerCallbacks final : public BLEServerCallbacks
        {
        public:
            explicit ServerCallbacks(BleNkroMouse& owner) : m_owner(owner) {}
            void onConnect(BLEServer*) override {
                m_owner.m_connected.store(true, std::memory_order_release);
            }
            void onDisconnect(BLEServer*) override {
                m_owner.m_connected.store(false, std::memory_order_release);
                m_owner.m_resetPending.store(true, std::memory_order_release);
                BLEDevice::startAdvertising();
            }

        private:
            BleNkroMouse& m_owner;
        };

        class LedCallbacks final : public BLECharacteristicCallbacks
        {
        public:
            explicit LedCallbacks(BleNkroMouse& owner) : m_owner(owner) {}
            void onWrite(BLECharacteristic* characteristic) override {
                if ((characteristic == nullptr) || characteristic->getLength() < 1)
                    return;
                // m_owner.m_ledCallbacks.store(characteristic->getData()[0], std::memory_order_release);
            }

        private:
            BleNkroMouse& m_owner;
        };

    private:
        // maybe we should ensure that the deviceName is not invalid at compiling period?
        const char* m_deviceName;

        BLEServer*         m_server         = nullptr;
        BLEHIDDevice*      m_hid            = nullptr;
        BLECharacteristic* m_keyboardInput  = nullptr;
        BLECharacteristic* m_keyboardOutput = nullptr;
        BLECharacteristic* m_mouseInput     = nullptr;

        prt_hid::KeyboardReport m_keyboard {};
        prt_hid::MouseBtn       m_mouseBtn = prt_hid::MouseBtn::None;

        std::atomic_bool                  m_connected {false};
        std::atomic_bool                  m_resetPending {false};
        std::atomic<prt_hid::KeyboardLed> m_keyboardLeds {prt_hid::KeyboardLed::None};
        bool                              m_started = false;

        ServerCallbacks m_serverCallbacks;
        LedCallbacks    m_ledCallbacks;
    };

}
