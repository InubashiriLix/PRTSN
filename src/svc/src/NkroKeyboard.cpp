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

Result<void, StdError> NkroKeyboard::setup() {
    if (m_started) {
        return Err(StdError::INVALID_STATE);
    }

    if (!m_registered) {
        if (!USBHID::addDevice(this, sizeof(prt_hid::nkro_report_desc))) {
            return Err(StdError::FAIL);
        }
        m_registered = true;
    }

    m_hid.begin();
    if (!USB.begin()) {
        m_hid.end();
        return Err(StdError::FAIL);
    }
    m_started = true;

    return Ok();
}

Result<void, StdError> NkroKeyboard::end() {
    if (!m_started) {
        return Err(StdError::INVALID_STATE);
    }
    m_hid.end();
    m_started = false;

    return Ok();
}

Result<bool, StdError> NkroKeyboard::ready() {
    if (!m_started) {
        return Err(StdError::INVALID_STATE);
    }
    return Ok(m_hid.ready());
}

uint16_t NkroKeyboard::_onGetFeature(uint8_t report_id, uint8_t* buffer, uint16_t len) {
    if (buffer == nullptr || report_id != static_cast<uint8_t>(ReportId::KEYBOARD)) {
        return 0;
    }

    const uint16_t report_len = sizeof(m_pending_request) < len ? sizeof(m_pending_request) : len;
    memcpy(buffer, &m_pending_request, report_len);
    return report_len;
}

void NkroKeyboard::_onSetFeature(uint8_t report_id, const uint8_t* buffer, uint16_t len) {
    if (buffer == nullptr || report_id != static_cast<uint8_t>(ReportId::KEYBOARD) || len < 4) {
        return;
    }

    FeatureReport  report {};
    const uint16_t report_len = sizeof(report) < len ? sizeof(report) : len;
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
    if (report_id == static_cast<uint8_t>(ReportId::KEYBOARD) && buffer != nullptr && len >= 1 && len <= this->LED_NUM) {
        memcpy(m_leds, buffer, len);
    }
}

Result<void, StdError> NkroKeyboard::press(uint8_t usage) {
    if (isModifier(usage)) {
        m_report.modifiers |= uint8_t(1u << (usage & 0x07));
    }
    else if (usage <= prt_hid::KEY_ID_NKRO_MAX) {
        m_report.keys[usage >> 3] |= (1u << (usage & 0x07));
    }
    else {
        return Err(StdError::INVALID_ARGUMENT);
    }

    return sendKeyboard();
}

Result<void, StdError> NkroKeyboard::release(uint8_t usage) {
    if (isModifier(usage)) {
        m_report.modifiers &= uint8_t(~(1u << (usage & 0x07)));
    }
    else if (usage <= prt_hid::KEY_ID_NKRO_MAX) {
        clearBit(m_report.keys, usage);
    }
    else {
        return Err(StdError::INVALID_ARGUMENT);
    }
    return sendKeyboard();
}

Result<void, StdError> NkroKeyboard::releaseAll() {
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
    return sendConsumer();
}

Result<void, StdError> NkroKeyboard::updateKeyboardState(const uint8_t* usageBitmap, size_t usageBitmapSize) {
    if (usageBitmap == nullptr || usageBitmapSize != KEY_USAGE_BITMAP_SIZE) {
        return Err(StdError::INVALID_ARGUMENT);
    }

    for (size_t index = sizeof(m_report.keys); index < prt_hid::KEY_ID_MODIFIERS_START / 8; ++index) {
        if (usageBitmap[index] != 0) {
            return Err(StdError::INVALID_ARGUMENT);
        }
    }
    for (size_t index = prt_hid::KEY_ID_MODIFIERS_END / 8 + 1; index < usageBitmapSize; ++index) {
        if (usageBitmap[index] != 0) {
            return Err(StdError::INVALID_ARGUMENT);
        }
    }

    KeyboardReport nextReport {};
    nextReport.modifiers = usageBitmap[prt_hid::KEY_ID_MODIFIERS_START / 8];
    memcpy(nextReport.keys, usageBitmap, sizeof(nextReport.keys));

    if (memcmp(&m_report, &nextReport, sizeof(m_report)) == 0) {
        return Ok();
    }

    const KeyboardReport previousReport = m_report;
    m_report                            = nextReport;

    auto result = sendKeyboard();
    if (result.is_err()) {
        m_report = previousReport;
    }
    return result;
}

Result<void, StdError> NkroKeyboard::pressConsumer(uint16_t usage) {
    if (usage > prt_hid::HID_CONSUMER_USAGE_MAX) {
        return Err(StdError::INVALID_ARGUMENT);
    }

    m_consumer = usage;
    return sendConsumer();
}
Result<void, StdError> NkroKeyboard::releaseConsumer() {
    m_consumer = 0u;
    return sendConsumer();
}
Result<void, StdError> NkroKeyboard::pressSystem(uint8_t bit) {
    if (bit > 2) {
        return Err(StdError::INVALID_ARGUMENT);
    }
    m_system |= uint8_t(1u << bit);
    return sendSystem();
}

Result<void, StdError> NkroKeyboard::releaseSystem() {
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

/// tell wheter given usage is a modifier key (E0 ~ E7)
inline bool NkroKeyboard::isModifier(uint8_t usage) {
    return usage >= prt_hid::KEY_ID_MODIFIERS_START && usage <= prt_hid::KEY_ID_MODIFIERS_END;
}
/// clear a bit in the bitmap for the given usage
inline void NkroKeyboard::clearBit(uint8_t* bitmap, uint8_t usage) {
    bitmap[usage >> 3] &= uint8_t(~(1u << (usage & 0x07)));
}

Result<void, StdError> NkroKeyboard::sendKeyboard() {
    if (!m_started) {
        return Err(StdError::INVALID_STATE);
    }

    if (!m_hid.SendReport(static_cast<uint8_t>(ReportId::KEYBOARD), &m_report, sizeof(m_report))) {
        return Err(StdError::FAIL);
    }
    return Ok();
}

Result<void, StdError> NkroKeyboard::sendConsumer() {
    if (!m_started) {
        return Err(StdError::INVALID_STATE);
    }

    if (!m_hid.SendReport(static_cast<uint8_t>(ReportId::CONSUMER), &m_consumer, sizeof(m_consumer))) {
        return Err(StdError::FAIL);
    }
    return Ok();
}
Result<void, StdError> NkroKeyboard::sendSystem() {
    if (!m_started) {
        return Err(StdError::INVALID_STATE);
    }

    if (!m_hid.SendReport(static_cast<uint8_t>(ReportId::SYSTEM), &m_system, sizeof(m_system))) {
        return Err(StdError::FAIL);
    }
    return Ok();
}
