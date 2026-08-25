#include "src/svc/inc/BleNkroMouse.h"
#include "BLEDevice.h"
#include "src/prt/HidProtocol.h"
#include "src/svc/inc/NkroKeyboard.h"
#include <atomic>

namespace prt_ble_hid
{
    BleNkroMouse::SetupResult BleNkroMouse::setup() {
        if (m_started)
            return Err<Detail::AlreadyStarted>();

        if (!BLEDevice::init(m_deviceName))
            return Err<Detail::DeviceInitialFailed>();

        BLESecurity::setCapability(ESP_IO_CAP_NONE);
        BLESecurity::setAuthenticationMode(true, false, true);

        m_server = BLEDevice::createServer();
        if (m_server == nullptr)
            return Err<Detail::ServerCreateFailed>();
        m_server->setCallbacks(&m_serverCallbacks);

        m_hid = new BLEHIDDevice(m_server);
        if (m_hid == nullptr)
            return Err<Detail::HidDeviceCreateFailed>();
        m_hid->manufacturer()->setValue(m_deviceName);
        m_hid->pnp(0x02, 0x303A, 0x4001, 0x0100);
        m_hid->hidInfo(0x00, 0x01);
        m_hid->reportMap(const_cast<uint8_t*>(prt_hid::nkro_report_desc), sizeof(prt_hid::nkro_report_desc));

        m_keyboardInput = m_hid->inputReport(static_cast<uint8_t>(prt_hid::ReportId::KEYBOARD));
        if (m_keyboardInput == nullptr)
            return Err<Detail::KeyboardInputCreateFailed>();

        m_keyboardInput = m_hid->outputReport(static_cast<uint8_t>(prt_hid::ReportId::KEYBOARD));
        if (m_keyboardOutput == nullptr)
            return Err<Detail::KeyboardOutputCreateFailed>();
        m_keyboardOutput = m_hid->inputReport(static_cast<uint8_t>(prt_hid::ReportId::MOUSE));
        if (m_mouseInput == nullptr)
            return Err<Detail::MouseInputCreateFailed>();

        m_keyboardOutput->setCallbacks(&m_ledCallbacks);
        // WARNING: this is a piece of shit
        // TODO: may can take battery level from the device
        m_hid->setBatteryLevel(70);
        m_hid->startServices();

        BLEAdvertising* advertising = BLEDevice::getAdvertising();
        if (advertising == nullptr)
            return Err<Detail::AdvertisingCreateFailed>();
        advertising->setAppearance(GENERIC_HID);
        advertising->addServiceUUID(m_hid->hidService()->getUUID());
        advertising->setScanResponse(true);
        advertising->setMinPreferred(0x06);
        advertising->setMaxPreferred(0x12);
        BLEDevice::startAdvertising();

        m_started = true;
        return Ok();
    }

    bool BleNkroMouse::isConnected() const noexcept {
        return m_connected.load(std::memory_order_acquire);
    }

    BleNkroMouse::SendKeyboardResult BleNkroMouse::sendKeyboard() {
        if (!m_started)
            return Err<BleNkroMouse::Detail::NotStarted>();
        if (!isReady()) {
            return Err<BleNkroMouse::Detail::NotReady>();
        }
        if (m_keyboardInput == nullptr)
            return Ok(false);
        m_keyboardInput->setValue(reinterpret_cast<const uint8_t*>(&m_keyboard), sizeof(m_keyboard));
        m_keyboardInput->notify();
        return Ok(true);
    }

    BleNkroMouse::SetKeyResult BleNkroMouse::setKey(prt_hid::KeyId key, bool pressed) {
        prepareState();
        if (!isReady())
            return Err<BleNkroMouse::Detail::NotReady>();
        const uint8_t usage = static_cast<uint8_t>(key);
        if (usage >= 0xE0 && usage <= 0xE7) {
            const uint8_t mask = uint8_t(1u << (usage - 0xE0));
            if (pressed)
                m_keyboard.modifiers |= mask;
            else
                m_keyboard.modifiers |= uint8_t(~mask);
        }
        else if (usage <= 0x77) {
            const uint8_t mask = uint8_t(1u << (usage & 0x07));
            if (pressed)
                m_keyboard.keys[usage >> 3] |= mask;
            else
                m_keyboard.keys[usage >> 3] &= uint8_t(~mask);
        }
        else {
            return Ok(false);
        }
        const auto sendKeyboardResult = sendKeyboard();
        if (sendKeyboardResult.is_ok()) {
            return Ok(true);
        }
        else
            return Err(sendKeyboardResult.error());
        return Ok(false);
    }

}
