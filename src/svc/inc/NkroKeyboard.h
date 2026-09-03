#pragma once
#include <USB.h>
#include <USBHID.h>
#include <cstdint>
#include "src/fw/inc/std_err.h"
#include "src/prt/HidProtocol.h"
#include "src/fw/inc/Result.h"

class NkroKeyboard : USBHIDDevice
{
public:
    constexpr static uint16_t LED_NUM = 256;

private:
    static_assert(sizeof(prt_hid::FeatureReport) == 32, "FeatureReport must be 32 bytes");
    static_assert(prt_hid::HID_FEATURE_MAX_SLOTS <= 256,
                  "Feature slot count must fit in FeatureReport::slot");

public:
    NkroKeyboard();
    ~NkroKeyboard();

    Result<void, StdErrors> setup();
    Result<void, StdErrors> end();
    Result<bool, StdErrors> ready();

#define PRTN_TEST_USAGE_METHOD [[deprecated("test-only API sends HID reports immediately; use NkroKeyboardService::update() for batched state updates")]]
    PRTN_TEST_USAGE_METHOD Result<void, StdErrors> press(uint8_t usage);
    PRTN_TEST_USAGE_METHOD Result<void, StdErrors> release(uint8_t usage);
    PRTN_TEST_USAGE_METHOD Result<void, StdErrors> releaseAll();
#undef PRTN_TEST_USAGE_METHOD

    Result<void, StdErrors> updateKeyboardState(const prt_hid::KeyboardReport& report);
    Result<void, StdErrors> updateMouseState(const prt_hid::MouseReport& report);
    Result<void, StdErrors> moveMouse(int8_t x, int8_t y, int8_t wheel);
    Result<void, StdErrors> setMouseBtn(prt_hid::MouseBtn buttonMask, bool pressed);

    Result<void, StdErrors> pressConsumer(uint16_t usage);
    Result<void, StdErrors> releaseConsumer();
    Result<void, StdErrors> pressSystem(uint8_t bit);
    Result<void, StdErrors> releaseSystem();

    uint16_t lookupUsage(uint8_t slot) const;
    void     resetKeyMapping();

    // clang-format off
    const uint8_t* getLeds() const { return m_leds; }
    // clang-format on

    uint16_t _onGetFeature(uint8_t report_id, uint8_t* buffer, uint16_t len) override;
    uint16_t _onGetDescriptor(uint8_t* buffer) override;
    void     _onSetFeature(uint8_t report_id, const uint8_t* buffer, uint16_t len) override;
    void     _onOutput(uint8_t report_id, const uint8_t* buffer, uint16_t len) override;

private:
    USBHID                  m_hid;
    prt_hid::KeyboardReport m_report {};
    prt_hid::KeyboardReport m_lastSentReport {};
    prt_hid::MouseBtn       m_mouseButtons {prt_hid::MouseBtn::None};
    prt_hid::MouseBtn       m_lastSentMouseButtons {prt_hid::MouseBtn::None};
    uint16_t                m_consumer {0};
    uint8_t                 m_system {0};
    uint8_t                 m_leds[LED_NUM] {};
    bool                    m_started {false};
    bool                    m_registered {false};
    bool                    m_lastSentReportValid {false};
    bool                    m_lastSentMouseButtonsValid {false};

    uint16_t               m_key_mapping[prt_hid::HID_FEATURE_MAX_SLOTS] {};
    prt_hid::FeatureReport m_pending_request {};

    Result<void, StdErrors> sendKeyboard();
    Result<void, StdErrors> sendMouse(const prt_hid::MouseReport& report);
    Result<void, StdErrors> sendConsumer();
    Result<void, StdErrors> sendSystem();
};
