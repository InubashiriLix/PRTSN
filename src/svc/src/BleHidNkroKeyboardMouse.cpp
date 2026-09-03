#include "src/svc/inc/BleHidNkroKeyboardMouse.h"
#include "BLEDevice.h"
#include "src/prt/HidProtocol.h"
#include "src/svc/inc/NkroKeyboard.h"
#include <atomic>

namespace prt_ble_hid
{
    BleHidNkroKeyboardMouse::SetupResult BleHidNkroKeyboardMouse::setup() {
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

        m_keyboardOutput = m_hid->outputReport(static_cast<uint8_t>(prt_hid::ReportId::KEYBOARD));
        if (m_keyboardOutput == nullptr)
            return Err<Detail::KeyboardOutputCreateFailed>();
        m_mouseInput = m_hid->inputReport(static_cast<uint8_t>(prt_hid::ReportId::MOUSE));
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

        m_keyboard                  = {};
        m_lastSentKeyboard          = {};
        m_lastSentKeyboardValid     = false;
        m_mouseBtn                  = prt_hid::MouseBtn::None;
        m_lastSentMouseButtons      = prt_hid::MouseBtn::None;
        m_lastSentMouseButtonsValid = false;
        m_started                   = true;
        return Ok();
    }

    bool BleHidNkroKeyboardMouse::isConnected() const noexcept {
        return m_connected.load(std::memory_order_acquire);
    }

    BleHidNkroKeyboardMouse::SendKeyboardResult BleHidNkroKeyboardMouse::sendKeyboard() {
        prepareState();
        if (!m_started)
            return Err<BleHidNkroKeyboardMouse::Detail::NotStarted>();
        if (!isReady()) {
            return Err<BleHidNkroKeyboardMouse::Detail::NotReady>();
        }
        if (m_keyboardInput == nullptr)
            return Err<BleHidNkroKeyboardMouse::Detail::KeyboardInputNullError>();
        if (m_lastSentKeyboardValid && prt_hid::keyboardReportsEqual(m_keyboard, m_lastSentKeyboard)) {
            return Ok(false);
        }
        m_keyboardInput->setValue(reinterpret_cast<const uint8_t*>(&m_keyboard), sizeof(m_keyboard));
        m_keyboardInput->notify();
        m_lastSentKeyboard      = m_keyboard;
        m_lastSentKeyboardValid = true;
        return Ok(true);
    }

    BleHidNkroKeyboardMouse::UpdateKeyboardStateResult BleHidNkroKeyboardMouse::updateKeyboardState(const prt_hid::KeyboardReport& report) {
        prepareState();
        m_keyboard = report;
        return sendKeyboard();
    }

    BleHidNkroKeyboardMouse::SetKeyResult BleHidNkroKeyboardMouse::setKey(prt_hid::KeyId key, bool pressed) {
        prepareState();
        if (!isReady())
            return Err<BleHidNkroKeyboardMouse::Detail::NotReady>();
        if (m_keyboardInput == nullptr)
            return Err<BleHidNkroKeyboardMouse::Detail::KeyboardInputNullError>();

        if (!prt_hid::setKeyboardKey(m_keyboard, key, pressed)) {
            return Ok(false);
        }
        return sendKeyboard();
    }

    BleHidNkroKeyboardMouse::ReleaseAllKeyResult BleHidNkroKeyboardMouse::releaseAllKeys() {
        prepareState();
        if (!isReady())
            return Err<BleHidNkroKeyboardMouse::Detail::NotReady>();
        m_keyboard = {};
        return sendKeyboard();
    }

    BleHidNkroKeyboardMouse::UpdateMouseStateResult BleHidNkroKeyboardMouse::updateMouseState(const prt_hid::MouseReport& report) {
        prepareState();
        if (!prt_hid::isMouseReportValid(report))
            return Err<BleHidNkroKeyboardMouse::Detail::InvalidMouseReport>();

        m_mouseBtn = report.buttons;
        return sendMouse(report);
    }

    BleHidNkroKeyboardMouse::UpdateMouseStateResult BleHidNkroKeyboardMouse::sendMouse(const prt_hid::MouseReport& report) {
        if (!m_started)
            return Err<BleHidNkroKeyboardMouse::Detail::NotStarted>();
        if (!isReady())
            return Err<BleHidNkroKeyboardMouse::Detail::NotReady>();
        if (m_mouseInput == nullptr)
            return Err<BleHidNkroKeyboardMouse::Detail::MouseInputNullError>();
        if (!prt_hid::hasMouseMotion(report) && m_lastSentMouseButtonsValid && report.buttons == m_lastSentMouseButtons) {
            return Ok(false);
        }
        m_mouseInput->setValue(reinterpret_cast<const uint8_t*>(&report), sizeof(report));
        m_mouseInput->notify();
        m_lastSentMouseButtons      = report.buttons;
        m_lastSentMouseButtonsValid = true;
        return Ok(true);
    }

    BleHidNkroKeyboardMouse::MoveMouseResult BleHidNkroKeyboardMouse::moveMouse(int8_t x, int8_t y, int8_t wheel) {
        return updateMouseState({.buttons = m_mouseBtn, .x = x, .y = y, .wheel = wheel});
    }

    BleHidNkroKeyboardMouse::SetMouseBtnResult BleHidNkroKeyboardMouse::setMouseBtn(prt_hid::MouseBtn btn, bool pressed) {
        prepareState();
        auto nextButtons = m_mouseBtn;
        if (!prt_hid::setMouseButton(nextButtons, btn, pressed))
            return Err<BleHidNkroKeyboardMouse::Detail::InvalidMouseReport>();
        return updateMouseState({.buttons = nextButtons});
    }

}
