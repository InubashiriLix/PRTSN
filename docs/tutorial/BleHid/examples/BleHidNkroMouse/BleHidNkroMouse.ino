#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include <atomic>
#include <cstdint>
#include <cstring>

namespace
{
    /**
     * BLE HID Report Map / BLE HID 报告描述符
     *
     * Report ID 1 is an NKRO keyboard:
     *   - one modifier byte for usages E0..E7;
     *   - fifteen bitmap bytes for usages 00..77;
     *   - one output byte for the host-controlled keyboard LEDs.
     *
     * Report ID 2 is a relative mouse:
     *   - five buttons;
     *   - signed 8-bit X, Y and vertical wheel deltas.
     *
     * BLEHIDDevice::inputReport(id) puts the Report ID in the Report Reference
     * descriptor (UUID 0x2908). Therefore the characteristic value sent by
     * this example contains only the report body and does not repeat the ID.
     */
    // clang-format off
    constexpr uint8_t ReportMap[] = {
        // ---------- Report ID 1: NKRO keyboard / 全键无冲键盘 ----------
        0x05, 0x01,       // Usage Page (Generic Desktop)
        0x09, 0x06,       // Usage (Keyboard)
        0xA1, 0x01,       // Collection (Application)
        0x85, 0x01,       //   Report ID (1)

        0x05, 0x07,       //   Usage Page (Keyboard/Keypad)
        0x19, 0xE0,       //   Usage Minimum (Left Control)
        0x29, 0xE7,       //   Usage Maximum (Right GUI)
        0x15, 0x00,       //   Logical Minimum (0)
        0x25, 0x01,       //   Logical Maximum (1)
        0x75, 0x01,       //   Report Size (1 bit)
        0x95, 0x08,       //   Report Count (8)
        0x81, 0x02,       //   Input (Data, Variable, Absolute)

        0x05, 0x07,       //   Usage Page (Keyboard/Keypad)
        0x19, 0x00,       //   Usage Minimum (0)
        0x29, 0x77,       //   Usage Maximum (0x77)
        0x15, 0x00,       //   Logical Minimum (0)
        0x25, 0x01,       //   Logical Maximum (1)
        0x75, 0x01,       //   Report Size (1 bit)
        0x95, 0x78,       //   Report Count (120 bits = 15 bytes)
        0x81, 0x02,       //   Input (Data, Variable, Absolute)

        0x05, 0x08,       //   Usage Page (LEDs)
        0x19, 0x01,       //   Usage Minimum (Num Lock)
        0x29, 0x05,       //   Usage Maximum (Kana)
        0x75, 0x01,       //   Report Size (1 bit)
        0x95, 0x05,       //   Report Count (5)
        0x91, 0x02,       //   Output (Data, Variable, Absolute)
        0x75, 0x03,       //   Report Size (3-bit padding)
        0x95, 0x01,       //   Report Count (1)
        0x91, 0x03,       //   Output (Constant, Variable, Absolute)
        0xC0,             // End Collection

        // ---------- Report ID 2: relative mouse / 相对位移鼠标 ----------
        0x05, 0x01,       // Usage Page (Generic Desktop)
        0x09, 0x02,       // Usage (Mouse)
        0xA1, 0x01,       // Collection (Application)
        0x85, 0x02,       //   Report ID (2)
        0x09, 0x01,       //   Usage (Pointer)
        0xA1, 0x00,       //   Collection (Physical)

        0x05, 0x09,       //     Usage Page (Button)
        0x19, 0x01,       //     Usage Minimum (Button 1)
        0x29, 0x05,       //     Usage Maximum (Button 5)
        0x15, 0x00,       //     Logical Minimum (0)
        0x25, 0x01,       //     Logical Maximum (1)
        0x75, 0x01,       //     Report Size (1 bit)
        0x95, 0x05,       //     Report Count (5)
        0x81, 0x02,       //     Input (Data, Variable, Absolute)
        0x75, 0x03,       //     Report Size (3-bit padding)
        0x95, 0x01,       //     Report Count (1)
        0x81, 0x03,       //     Input (Constant, Variable, Absolute)

        0x05, 0x01,       //     Usage Page (Generic Desktop)
        0x09, 0x30,       //     Usage (X)
        0x09, 0x31,       //     Usage (Y)
        0x09, 0x38,       //     Usage (Wheel)
        0x15, 0x81,       //     Logical Minimum (-127)
        0x25, 0x7F,       //     Logical Maximum (127)
        0x75, 0x08,       //     Report Size (8 bits)
        0x95, 0x03,       //     Report Count (3)
        0x81, 0x06,       //     Input (Data, Variable, Relative)
        0xC0,             //   End Collection
        0xC0,             // End Collection
    };
    // clang-format on
    //
    enum class ReportId : uint8_t
    {
        Keyboard = 1,
        Mouse    = 2,
    };

    /** Keyboard input value. Report ID is carried by descriptor 0x2908. */
    struct KeyboardReport
    {
        uint8_t modifiers = 0;
        uint8_t keys[15]  = {};
    } __attribute__((packed));

    /** Mouse input value. Movement fields are relative deltas, not positions. */
    struct MouseReport
    {
        uint8_t buttons = 0;
        int8_t  x       = 0;
        int8_t  y       = 0;
        int8_t  wheel   = 0;
    } __attribute__((packed));

    static_assert(sizeof(KeyboardReport) == 16, "keyboard descriptor and report disagree");
    static_assert(sizeof(MouseReport) == 4, "mouse descriptor and report disagree");

    enum class Key : uint8_t
    {
        A           = 0x04,
        B           = 0x05,
        C           = 0x06,
        Enter       = 0x28,
        Space       = 0x2C,
        LeftControl = 0xE0,
        LeftShift   = 0xE1,
        LeftAlt     = 0xE2,
        LeftGui     = 0xE3,
    };

    enum MouseButton : uint8_t
    {
        MouseLeft    = 1u << 0,
        MouseRight   = 1u << 1,
        MouseMiddle  = 1u << 2,
        MouseBack    = 1u << 3,
        MouseForward = 1u << 4,
    };

    class BleNkroMouse
    {
    public:
        BleNkroMouse() : m_serverCallbacks(*this), m_ledCallbacks(*this) {}

        /**
         * Build the HOGP database and begin advertising.
         * 构建 HID-over-GATT 服务并开始广播；对象应在整个程序期间存活。
         */
        bool begin(const char* deviceName) {
            if (m_started || deviceName == nullptr || deviceName[0] == '\0')
                return false;

            if (!BLEDevice::init(deviceName))
                return false;

            // Secure Connections + bonding, without MITM authentication.
            // 这是无屏幕设备常用的 Just Works；方便，但不能抵抗主动中间人。
            BLESecurity::setCapability(ESP_IO_CAP_NONE);
            BLESecurity::setAuthenticationMode(true, false, true);

            m_server = BLEDevice::createServer();
            if (m_server == nullptr)
                return false;
            m_server->setCallbacks(&m_serverCallbacks);

            m_hid = new BLEHIDDevice(m_server);
            if (m_hid == nullptr)
                return false;

            m_hid->manufacturer()->setValue("PRTN Project");
            m_hid->pnp(0x02, 0x303A, 0x4001, 0x0100);
            m_hid->hidInfo(0x00, 0x01);
            m_hid->reportMap(const_cast<uint8_t*>(ReportMap), sizeof(ReportMap));

            m_keyboardInput  = m_hid->inputReport(static_cast<uint8_t>(ReportId::Keyboard));
            m_keyboardOutput = m_hid->outputReport(static_cast<uint8_t>(ReportId::Keyboard));
            m_mouseInput     = m_hid->inputReport(static_cast<uint8_t>(ReportId::Mouse));
            if (m_keyboardInput == nullptr || m_keyboardOutput == nullptr || m_mouseInput == nullptr)
                return false;

            m_keyboardOutput->setCallbacks(&m_ledCallbacks);
            m_hid->setBatteryLevel(100);
            m_hid->startServices();

            BLEAdvertising* advertising = BLEDevice::getAdvertising();
            if (advertising == nullptr)
                return false;
            advertising->setAppearance(GENERIC_HID);
            advertising->addServiceUUID(m_hid->hidService()->getUUID());
            advertising->setScanResponse(true);
            advertising->setMinPreferred(0x06);
            advertising->setMaxPreferred(0x12);
            BLEDevice::startAdvertising();

            m_started = true;
            return true;
        }

        [[nodiscard]] bool connected() const noexcept {
            return m_connected.load(std::memory_order_acquire);
        }

        /** Set or clear one HID keyboard usage and publish the complete state. */
        bool setKey(Key key, bool pressed) {
            prepareState();
            if (!ready())
                return false;
            const uint8_t usage = static_cast<uint8_t>(key);
            if (usage >= 0xE0 && usage <= 0xE7) {
                const uint8_t mask = uint8_t(1u << (usage - 0xE0));
                if (pressed)
                    m_keyboard.modifiers |= mask;
                else
                    m_keyboard.modifiers &= uint8_t(~mask);
            }
            else if (usage <= 0x77) {
                const uint8_t mask = uint8_t(1u << (usage & 0x07));
                if (pressed)
                    m_keyboard.keys[usage >> 3] |= mask;
                else
                    m_keyboard.keys[usage >> 3] &= uint8_t(~mask);
            }
            else {
                return false;
            }
            return sendKeyboard();
        }

        /** Release every key. Always send this after an interrupted key sequence. */
        bool releaseAllKeys() {
            prepareState();
            m_keyboard = {};
            return sendKeyboard();
        }

        /** Change the persistent button state and send a zero-motion report. */
        bool setMouseButton(MouseButton button, bool pressed) {
            prepareState();
            if (!ready())
                return false;
            if (pressed)
                m_mouseButtons |= button;
            else
                m_mouseButtons &= uint8_t(~button);
            return moveMouse(0, 0, 0);
        }

        /** Send one relative movement report. Deltas are consumed by the host once. */
        bool moveMouse(int8_t x, int8_t y, int8_t wheel = 0) {
            prepareState();
            if (!ready() || m_mouseInput == nullptr)
                return false;
            const MouseReport report {m_mouseButtons, x, y, wheel};
            m_mouseInput->setValue(reinterpret_cast<const uint8_t*>(&report), sizeof(report));
            m_mouseInput->notify();
            return true;
        }

        /** Last LED byte written by the host: bits 0..4 are Num/Caps/Scroll/Compose/Kana. */
        [[nodiscard]] uint8_t keyboardLeds() const noexcept {
            return m_keyboardLeds.load(std::memory_order_acquire);
        }

    private:
        // Inheritance exists only at the Arduino BLE callback boundary. The HID
        // service itself is composed from these tiny adapters.
        class ServerCallbacks final : public BLEServerCallbacks
        {
        public:
            explicit ServerCallbacks(BleNkroMouse& owner) : m_owner(owner) {}
            void onConnect(BLEServer*) override {
                m_owner.m_connected.store(true, std::memory_order_release);
            }
            void onDisconnect(BLEServer*) override {
                m_owner.m_connected.store(false, std::memory_order_release);
                // Do not mutate application-owned reports from the BLE task.
                // The application task consumes this request before its next update.
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
                if (characteristic == nullptr || characteristic->getLength() < 1)
                    return;
                m_owner.m_keyboardLeds.store(characteristic->getData()[0], std::memory_order_release);
            }

        private:
            BleNkroMouse& m_owner;
        };

        [[nodiscard]] bool ready() const noexcept {
            return m_started && connected();
        }

        void prepareState() noexcept {
            if (!m_resetPending.exchange(false, std::memory_order_acq_rel))
                return;
            m_keyboard     = {};
            m_mouseButtons = 0;
        }

        bool sendKeyboard() {
            if (!ready() || m_keyboardInput == nullptr)
                return false;
            m_keyboardInput->setValue(
                reinterpret_cast<const uint8_t*>(&m_keyboard), sizeof(m_keyboard));
            m_keyboardInput->notify();
            return true;
        }

        BLEServer*          m_server         = nullptr;
        BLEHIDDevice*       m_hid            = nullptr;
        BLECharacteristic*  m_keyboardInput  = nullptr;
        BLECharacteristic*  m_keyboardOutput = nullptr;
        BLECharacteristic*  m_mouseInput     = nullptr;
        KeyboardReport      m_keyboard {};
        uint8_t             m_mouseButtons = 0;
        std::atomic_bool    m_connected {false};
        std::atomic_bool    m_resetPending {false};
        std::atomic_uint8_t m_keyboardLeds {0};
        bool                m_started = false;
        ServerCallbacks     m_serverCallbacks;
        LedCallbacks        m_ledCallbacks;
    };

    BleNkroMouse hid;
}

void setup() {
    Serial.begin(115200);
    if (!hid.begin("PRTN-NKRO-Mouse")) {
        Serial.println("BLE HID setup failed");
        return;
    }
    Serial.println("BLE HID is advertising; pair it in the operating-system Bluetooth settings.");
}

void loop() {
    // This deliberately does not inject keystrokes automatically. After pairing,
    // replace this block with scanner events, and call setKey() on every edge.
    static bool wasConnected = false;
    const bool  isConnected  = hid.connected();
    if (isConnected != wasConnected) {
        Serial.println(isConnected ? "BLE HID connected" : "BLE HID disconnected");
        wasConnected = isConnected;
    }

    delay(10);
}
