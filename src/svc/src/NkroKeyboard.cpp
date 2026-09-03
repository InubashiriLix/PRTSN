#include "src/svc/inc/NkroKeyboard.h"
#include "USBHID.h"
#include "src/fw/inc/Result.h"
#include "src/prt/HidProtocol.h"
#include <cstring>

NkroKeyboard::NkroKeyboard() : m_hid(hid_interface_protocol_enum_t::HID_ITF_PROTOCOL_NONE) {
}

NkroKeyboard::~NkroKeyboard() {
    if (m_started) {
        end();
    }
}

Result<void, StdErrors> NkroKeyboard::setup() {
    if (m_started) {
        return Err<StdError::INVALID_STATE>();
    }

    if (!m_registered) {
        if (!USBHID::addDevice(this, sizeof(prt_hid::nkro_report_desc))) {
            return Err<StdError::FAIL>();
        }
        m_registered = true;
    }

    m_hid.begin();
    if (!USB.begin()) {
        m_hid.end();
        return Err<StdError::FAIL>();
    }
    m_report                    = {};
    m_lastSentReport            = {};
    m_lastSentReportValid       = false;
    m_mouseButtons              = prt_hid::MouseBtn::None;
    m_lastSentMouseButtons      = prt_hid::MouseBtn::None;
    m_lastSentMouseButtonsValid = false;
    m_started                   = true;

    return Ok();
}

Result<void, StdErrors> NkroKeyboard::end() {
    if (!m_started) {
        return Err<StdError::INVALID_STATE>();
    }
    m_hid.end();
    m_started                   = false;
    m_lastSentReportValid       = false;
    m_lastSentMouseButtonsValid = false;

    return Ok();
}

Result<bool, StdErrors> NkroKeyboard::ready() {
    if (!m_started) {
        return Err<StdError::INVALID_STATE>();
    }
    const bool isReady = m_hid.ready();
    if (!isReady) {
        m_lastSentReportValid       = false;
        m_lastSentMouseButtonsValid = false;
    }
    return Ok(isReady);
}

uint16_t NkroKeyboard::_onGetFeature(uint8_t report_id, uint8_t* buffer, uint16_t len) {
    if (buffer == nullptr || report_id != static_cast<uint8_t>(prt_hid::ReportId::KEYBOARD)) {
        return 0;
    }

    const uint16_t report_len = sizeof(m_pending_request) < len ? sizeof(m_pending_request) : len;
    memcpy(buffer, &m_pending_request, report_len);
    return report_len;
}

void NkroKeyboard::_onSetFeature(uint8_t report_id, const uint8_t* buffer, uint16_t len) {
    if (buffer == nullptr || report_id != static_cast<uint8_t>(prt_hid::ReportId::KEYBOARD) || len < 4) {
        return;
    }

    prt_hid::FeatureReport report {};
    const uint16_t         report_len = sizeof(report) < len ? sizeof(report) : len;
    memcpy(&report, buffer, report_len);

    m_pending_request          = report;
    m_key_mapping[report.slot] = uint16_t(report.usage_lo) | (uint16_t(report.usage_hi) << 8);
}

uint16_t NkroKeyboard::_onGetDescriptor(uint8_t* buffer) {
    memcpy(buffer, prt_hid::nkro_report_desc, sizeof(prt_hid::nkro_report_desc));
    return sizeof(prt_hid::nkro_report_desc);
}

void NkroKeyboard::_onOutput(uint8_t report_id, const uint8_t* buffer, uint16_t len) {
    // if the report_id is 1 (keyboard) and the length is between 1 and LED_NUM, copy the buffer to m_leds
    if (report_id == static_cast<uint8_t>(prt_hid::ReportId::KEYBOARD) && buffer != nullptr && len >= 1 && len <= this->LED_NUM) {
        memcpy(m_leds, buffer, len);
    }
}

Result<void, StdErrors> NkroKeyboard::press(uint8_t usage) {
    if (!prt_hid::setKeyboardKey(m_report, static_cast<prt_hid::KeyId>(usage), true)) {
        return Err<StdError::INVALID_ARGUMENT>();
    }

    return sendKeyboard();
}

Result<void, StdErrors> NkroKeyboard::release(uint8_t usage) {
    if (!prt_hid::setKeyboardKey(m_report, static_cast<prt_hid::KeyId>(usage), false)) {
        return Err<StdError::INVALID_ARGUMENT>();
    }
    return sendKeyboard();
}

Result<void, StdErrors> NkroKeyboard::releaseAll() {
    memset(&m_report, 0, sizeof(m_report));
    m_consumer = 0;
    m_system   = 0;

    auto result = sendKeyboard();
    if (result.is_err()) {
        return result;
    }
    result = sendSystem();
    if (result.is_err()) {
        return result;
    }
    result = sendConsumer();
    if (result.is_err()) {
        return result;
    }

    m_mouseButtons = prt_hid::MouseBtn::None;
    return sendMouse({});
}

Result<void, StdErrors> NkroKeyboard::updateKeyboardState(const prt_hid::KeyboardReport& report) {
    m_report = report;
    return sendKeyboard();
}

Result<void, StdErrors> NkroKeyboard::updateMouseState(const prt_hid::MouseReport& report) {
    if (!prt_hid::isMouseReportValid(report)) {
        return Err<StdError::INVALID_ARGUMENT>();
    }

    m_mouseButtons = report.buttons;
    return sendMouse(report);
}

Result<void, StdErrors> NkroKeyboard::moveMouse(int8_t x, int8_t y, int8_t wheel) {
    return updateMouseState({.buttons = m_mouseButtons, .x = x, .y = y, .wheel = wheel});
}

Result<void, StdErrors> NkroKeyboard::setMouseBtn(prt_hid::MouseBtn buttonMask, bool pressed) {
    auto nextButtons = m_mouseButtons;
    if (!prt_hid::setMouseButton(nextButtons, buttonMask, pressed)) {
        return Err<StdError::INVALID_ARGUMENT>();
    }

    return updateMouseState({.buttons = nextButtons});
}

Result<void, StdErrors> NkroKeyboard::pressConsumer(uint16_t usage) {
    if (usage > prt_hid::HID_CONSUMER_USAGE_MAX) {
        return Err<StdError::INVALID_ARGUMENT>();
    }

    m_consumer = usage;
    return sendConsumer();
}
Result<void, StdErrors> NkroKeyboard::releaseConsumer() {
    m_consumer = 0u;
    return sendConsumer();
}
Result<void, StdErrors> NkroKeyboard::pressSystem(uint8_t bit) {
    if (bit > 2) {
        return Err<StdError::INVALID_ARGUMENT>();
    }
    m_system |= uint8_t(1u << bit);
    return sendSystem();
}

Result<void, StdErrors> NkroKeyboard::releaseSystem() {
    m_system = 0u;
    return sendSystem();
}

uint16_t NkroKeyboard::lookupUsage(uint8_t slot) const {
    return m_key_mapping[slot];
}

void NkroKeyboard::resetKeyMapping() {
    memset(m_key_mapping, 0, sizeof(m_key_mapping));
    m_pending_request = {};
}

Result<void, StdErrors> NkroKeyboard::sendKeyboard() {
    if (!m_started) {
        return Err<StdError::INVALID_STATE>();
    }

    if (m_lastSentReportValid && prt_hid::keyboardReportsEqual(m_report, m_lastSentReport)) {
        return Ok();
    }
    if (!m_hid.SendReport(static_cast<uint8_t>(prt_hid::ReportId::KEYBOARD), &m_report, sizeof(m_report))) {
        return Err<StdError::FAIL>();
    }
    m_lastSentReport      = m_report;
    m_lastSentReportValid = true;
    return Ok();
}

Result<void, StdErrors> NkroKeyboard::sendMouse(const prt_hid::MouseReport& report) {
    if (!m_started) {
        return Err<StdError::INVALID_STATE>();
    }
    if (!prt_hid::hasMouseMotion(report) && m_lastSentMouseButtonsValid && report.buttons == m_lastSentMouseButtons) {
        return Ok();
    }
    if (!m_hid.SendReport(static_cast<uint8_t>(prt_hid::ReportId::MOUSE), &report, sizeof(report))) {
        return Err<StdError::FAIL>();
    }

    m_lastSentMouseButtons      = report.buttons;
    m_lastSentMouseButtonsValid = true;
    return Ok();
}

Result<void, StdErrors> NkroKeyboard::sendConsumer() {
    if (!m_started) {
        return Err<StdError::INVALID_STATE>();
    }

    if (!m_hid.SendReport(static_cast<uint8_t>(prt_hid::ReportId::CONSUMER), &m_consumer, sizeof(m_consumer))) {
        return Err<StdError::FAIL>();
    }
    return Ok();
}
Result<void, StdErrors> NkroKeyboard::sendSystem() {
    if (!m_started) {
        return Err<StdError::INVALID_STATE>();
    }

    if (!m_hid.SendReport(static_cast<uint8_t>(prt_hid::ReportId::SYSTEM), &m_system, sizeof(m_system))) {
        return Err<StdError::FAIL>();
    }
    return Ok();
}
