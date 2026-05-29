#pragma once

#include <Arduino.h>
#include <cstdint>
#include <esp_now.h>
#include "src/svc/inc/wifi.h"

class EspNow
{
public:
    struct Peer
    {
        uint8_t mac[ESP_NOW_ETH_ALEN] {};
        uint8_t channel   = 0;
        bool    encrypted = false;
        uint8_t lmk[ESP_NOW_KEY_LEN] {};
    };

    enum Mode : uint8_t
    {
        NONE,
        UNICAST,
        BROADCAST,
        UNICAST_BROADCAST,
    };

    struct Config
    {
        Peer*   peers     = nullptr;
        uint8_t peerCount = 0;
        uint8_t maxPeers  = 0;
        Mode    mode      = NONE;
    };

    enum RtnErrCode : uint8_t
    {
        OK = 0,
        INVALID_WIFI_INSTANCE,
        INVALID_CONFIG,
        INIT_FAILED,
        ADD_PEER_FAILED,
        REMOVE_PEER_FAILED,
        PEER_NOT_FOUND,
        PEER_TABLE_FULL,
        SEND_FAILED,
        CALLBACK_FAILED,
        NOT_INITIALIZED,
        ALREADY_INITIALIZED,
        INVALID_ARGUMENT,
        PEER_ALREADY_EXISTS,
    };

public:
    static EspNow& instance();

    EspNow(const EspNow&)            = delete;
    EspNow& operator=(const EspNow&) = delete;

    EspNow(EspNow&&)            = delete;
    EspNow& operator=(EspNow&&) = delete;

public:
    RtnErrCode setup(
        Wifi*             wifi,
        const Config&     config,
        esp_now_recv_cb_t onRecvCallback = nullptr,
        esp_now_send_cb_t onSendCallback = nullptr);

    // peers settings
    RtnErrCode addPeer(const Peer& peer);
    RtnErrCode removePeerByMac(const uint8_t* peerMac);
    int        getPeerCount() const;
    void       getPeerCount(int* outCnt);

    // send settings
    RtnErrCode sendBroadcast(const uint8_t* data, size_t len);
    RtnErrCode sendUnicast(const uint8_t* data, size_t len, const uint8_t* peerMac);

    // callback settings
    RtnErrCode setOnRecvCallback(esp_now_recv_cb_t callback);
    RtnErrCode setOnSendCallback(esp_now_send_cb_t callback);

private:
    EspNow();
    RtnErrCode enablePeer(const Peer& peer);
    RtnErrCode disablePeer(const Peer& peer);
    RtnErrCode enableBroadcastPeer();
    int        findPeerIndexByMac(const uint8_t* peerMac) const;
    bool       isValidPeer(const Peer& peer) const;
    bool       unicastEnabled() const;
    bool       broadcastEnabled() const;
    void       removePeerFromConfig(uint8_t peerIndex);

private:
    Wifi*  m_wifi = nullptr;
    Config m_config;

    bool m_initialized = false;

    static esp_now_send_cb_t m_onSendCallback;
    static esp_now_recv_cb_t m_onRecvCallback;
};
