#include "src/svc/inc/wifi.h"

#include <WiFi.h>
#include <esp_wifi.h>

namespace
{
    wifi_mode_t toEspMode(Wifi::Mode mode) {
        switch (mode) {
            case Wifi::Mode::STA:
                return WIFI_STA;
            case Wifi::Mode::AP:
                return WIFI_AP;
            case Wifi::Mode::AP_STA:
                return WIFI_AP_STA;
        }

        return WIFI_OFF;
    }
} // namespace

Wifi::Wifi(const Config& config) : m_config(config) {}

bool Wifi::begin() {
    stop();

    if (hasSta() && m_config.sta.connect && !validText(m_config.sta.ssid)) {
        return false;
    }

    if (hasAp() && !validText(m_config.ap.ssid)) {
        return false;
    }

    WiFi.persistent(false);
    WiFi.mode(toEspMode(m_config.mode));
    WiFi.setSleep(false);

    if (hasSta() && validText(m_config.sta.hostname)) {
        WiFi.setHostname(m_config.sta.hostname);
    }

    m_started = true;

    if (hasAp() && !startAp()) {
        stop();
        return false;
    }

    if (hasSta() && m_config.sta.connect) {
        return startSta();
    }

    if (hasSta()) {
        return startStaRadio();
    }

    return true;
}

void Wifi::update() {
    if (!m_started || !hasSta() || !m_config.sta.connect || staConnected()) {
        return;
    }

    const uint32_t nowMs = millis();
    if (!m_staBeginSent || nowMs - m_lastStaBeginMs >= m_config.sta.reconnectMs) {
        startSta();
    }
}

void Wifi::updateByIntervalMs(const uint32_t updateIntervalMs) {
    static uint32_t lastUpdateMs = 0;
    const uint32_t  nowMs        = millis();

    if (nowMs - lastUpdateMs >= updateIntervalMs) {
        update();
        lastUpdateMs = nowMs;
    }
}

void Wifi::updateByIntervalNum(const uint32_t updateIntervalNum) {
    static uint32_t updateCounter = 0;

    if (++updateCounter >= updateIntervalNum) {
        update();
        updateCounter = 0;
    }
}

void Wifi::stop() {
    WiFi.disconnect(true, false);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);

    m_started        = false;
    m_apRunning      = false;
    m_staBeginSent   = false;
    m_lastStaBeginMs = 0;
}

bool Wifi::connected() const {
    switch (m_config.mode) {
        case Mode::STA:
            return staConnected();
        case Mode::AP:
            return apRunning();
        case Mode::AP_STA:
            return apRunning() && staConnected();
    }

    return false;
}

Wifi::Status Wifi::status() const {
    Status s {};

    s.mode      = m_config.mode;
    s.started   = m_started;
    s.connected = connected();

    s.sta.enabled    = hasSta();
    s.sta.connected  = staConnected();
    s.sta.statusCode = static_cast<int>(WiFi.status());

    if (s.sta.connected) {
        s.sta.rssi    = WiFi.RSSI();
        s.sta.quality = qualityFromRssi(s.sta.rssi);
        s.sta.ip      = WiFi.localIP();
        s.sta.gateway = WiFi.gatewayIP();
        s.sta.subnet  = WiFi.subnetMask();
        s.sta.dns     = WiFi.dnsIP();
    }

    s.ap.enabled    = hasAp();
    s.ap.running    = apRunning();
    s.ap.channel    = m_config.ap.channel;
    s.ap.hidden     = m_config.ap.hidden;
    s.ap.maxClients = m_config.ap.maxClients;

    if (s.ap.running) {
        s.ap.ip          = WiFi.softAPIP();
        s.ap.clientCount = WiFi.softAPgetStationNum();
    }

    return s;
}

bool Wifi::staConnected() const {
    return hasSta() && WiFi.status() == WL_CONNECTED;
}

bool Wifi::apRunning() const {
    return hasAp() && m_apRunning;
}

int32_t Wifi::rssi() const {
    return staConnected() ? WiFi.RSSI() : 0;
}

uint8_t Wifi::signalQuality() const {
    return staConnected() ? qualityFromRssi(WiFi.RSSI()) : 0;
}

uint8_t Wifi::apClientCount() const {
    return apRunning() ? WiFi.softAPgetStationNum() : 0;
}

IPAddress Wifi::localIp() const {
    return staConnected() ? WiFi.localIP() : IPAddress(0, 0, 0, 0);
}

IPAddress Wifi::apIp() const {
    return apRunning() ? WiFi.softAPIP() : IPAddress(0, 0, 0, 0);
}

bool Wifi::hasSta() const {
    return m_config.mode == Mode::STA || m_config.mode == Mode::AP_STA;
}

bool Wifi::hasAp() const {
    return m_config.mode == Mode::AP || m_config.mode == Mode::AP_STA;
}

bool Wifi::validText(const char* text) const {
    return text != nullptr && text[0] != '\0';
}

uint8_t Wifi::qualityFromRssi(int32_t rssi) const {
    if (rssi <= -100) {
        return 0;
    }

    if (rssi >= -50) {
        return 100;
    }

    return static_cast<uint8_t>((rssi + 100) * 2);
}

bool Wifi::startAp() {
    const char* password = validText(m_config.ap.password) ? m_config.ap.password : nullptr;

    m_apRunning = WiFi.softAP(
        m_config.ap.ssid,
        password,
        m_config.ap.channel,
        m_config.ap.hidden,
        m_config.ap.maxClients);

    return m_apRunning;
}

bool Wifi::startSta() {
    m_lastStaBeginMs = millis();
    m_staBeginSent   = true;

    WiFi.begin(m_config.sta.ssid, m_config.sta.password);
    return true;
}

bool Wifi::startStaRadio() {
    m_staBeginSent   = false;
    m_lastStaBeginMs = 0;

    if (m_config.sta.channel == 0) {
        return true;
    }

    return esp_wifi_set_channel(m_config.sta.channel, WIFI_SECOND_CHAN_NONE) == ESP_OK;
}
