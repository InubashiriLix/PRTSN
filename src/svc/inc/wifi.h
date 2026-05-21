#pragma once

#include <Arduino.h>
#include <IPAddress.h>

/*
 * Wifi 是 svc 层的轻量 WiFi 封装。
 *
 * 控制接口只做三件事：
 *   1. begin()  : 按配置启动 STA / AP / AP_STA。
 *   2. update() : 周期性维护 STA 重连；需要在 Controller 循环里反复调用。
 *   3. stop()   : 关闭 STA、AP 和 WiFi 外设。
 *
 * 状态信息通过 status() 一次性读取。这样 Controller 和 MQTT 不需要直接碰
 * ESP32 Arduino 的 WiFi 全局对象。
 *
 * 使用示例：
 *
 *   static Wifi wifi({
 *       Wifi::Mode::STA,
 *       {"router-ssid", "router-password", "prtn-node", 5000},
 *       {},
 *   });
 *
 *   void setup() {
 *       wifi.begin();
 *   }
 *
 *   void loop_or_task() {
 *       wifi.update();
 *       if (wifi.staConnected()) {
 *           // 这里再运行 MQTT / NTP / OTA
 *       }
 *   }
 *
 * 三种模式：
 *   STA    : ESP32-C3 连接外部路由器。
 *   AP     : ESP32-C3 自己开热点。
 *   AP_STA : 同时开热点并连接外部路由器。
 *
 * connected() 的含义：
 *   STA    -> STA 已连上路由器
 *   AP     -> AP 热点已启动
 *   AP_STA -> AP 已启动，并且 STA 已连上路由器
 */
class Wifi
{
public:
    enum class Mode : uint8_t
    {
        STA,
        AP,
        AP_STA,
    };

    struct STAConfig
    {
        const char* ssid        = nullptr;
        const char* password    = nullptr;
        const char* hostname    = nullptr;
        uint32_t    reconnectMs = 5000;
        bool        connect     = false;
        uint8_t     channel     = 1;
    };

    struct APConfig
    {
        const char* ssid       = nullptr;
        const char* password   = nullptr;
        uint8_t     channel    = 1;
        bool        hidden     = false;
        uint8_t     maxClients = 4;
    };

    struct Config
    {
        Mode      mode = Mode::STA;
        STAConfig sta;
        APConfig  ap;
    };

    struct STAStatus
    {
        bool enabled   = false;
        bool connected = false;

        // ESP32 Arduino 的 WiFi.status() 原始状态码，便于调试底层连接状态。
        int statusCode = 0;

        // RSSI 单位是 dBm。常见范围大约 -30 到 -90，数值越接近 0 信号越好。
        int32_t rssi = 0;

        // 将 RSSI 粗略换算成 0-100 的质量百分比，只用于显示/日志。
        uint8_t quality = 0;

        IPAddress ip;
        IPAddress gateway;
        IPAddress subnet;
        IPAddress dns;
    };

    struct APStatus
    {
        bool enabled = false;
        bool running = false;

        uint8_t channel     = 1;
        bool    hidden      = false;
        uint8_t maxClients  = 4;
        uint8_t clientCount = 0;

        IPAddress ip;
    };

    struct Status
    {
        Mode mode = Mode::STA;

        bool started   = false;
        bool connected = false;

        STAStatus sta;
        APStatus  ap;
    };

public:
    explicit Wifi(const Config& config);

    bool begin();
    void update();
    void updateByIntervalMs(const uint32_t updateIntervalMs);
    void updateByIntervalNum(const uint32_t updateIntervalNum);
    void stop();

    char* getApLogText() {
        static char apLogText[64];

        if (!hasAp()) {
            snprintf(apLogText, sizeof(apLogText), "AP DISABLED");
            return apLogText;
        }

        snprintf(
            apLogText,
            sizeof(apLogText),
            "AP %s (ch %d, clients %d)",
            apRunning() ? "ON" : "OFF",
            m_config.ap.channel,
            apClientCount());
        return apLogText;
    }

    char* getStaLogText() {
        static char staLogText[64];

        if (!hasSta()) {
            snprintf(staLogText, sizeof(staLogText), "STA DISABLED");
            return staLogText;
        }

        snprintf(
            staLogText,
            sizeof(staLogText),
            "STA %s (rssi %d dBm, quality %d%%)",
            staConnected() ? "ON" : "OFF",
            rssi(),
            signalQuality());
        return staLogText;
    }

    Status status() const;

    bool connected() const;
    bool staConnected() const;
    bool apRunning() const;

    int32_t rssi() const;
    uint8_t signalQuality() const;
    uint8_t apClientCount() const;

    IPAddress localIp() const;
    IPAddress apIp() const;

private:
    Config   m_config;
    bool     m_started        = false;
    bool     m_apRunning      = false;
    bool     m_staBeginSent   = false;
    uint32_t m_lastStaBeginMs = 0;

    bool    hasSta() const;
    bool    hasAp() const;
    bool    validText(const char* text) const;
    uint8_t qualityFromRssi(int32_t rssi) const;
    bool    startAp();
    bool    startSta();
    bool    startStaRadio();
};
