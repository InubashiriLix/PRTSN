#pragma once
#include <USB.h>
#include <USBHID.h>
#include <cstddef>
#include <cstdint>
#include "src/fw/inc/std_err.h"
#include "src/prt/HidProtocol.h"
#include "src/fw/inc/Result.h"
#include "src/dvc/inc/IKeyboardScanDevice.h"

class NkroKeyboard : USBHIDDevice
{
public:
    constexpr static uint16_t LED_NUM               = 256;
    constexpr static size_t   KEY_USAGE_BITMAP_SIZE = 32;

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

    Result<void, StdErrors> updateKeyboardState(const uint8_t* usageBitmap, size_t usageBitmapSize);

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
    uint16_t                m_consumer {0};
    uint8_t                 m_system {0};
    uint8_t                 m_leds[LED_NUM] {};
    bool                    m_started {false};
    bool                    m_registered {false};

    uint16_t               m_key_mapping[prt_hid::HID_FEATURE_MAX_SLOTS] {};
    prt_hid::FeatureReport m_pending_request {};

    Result<void, StdErrors> sendKeyboard();
    Result<void, StdErrors> sendConsumer();
    Result<void, StdErrors> sendSystem();

    inline static bool isModifier(uint8_t usage);
    inline static void clearBit(uint8_t* bitmap, uint8_t usage);
};
