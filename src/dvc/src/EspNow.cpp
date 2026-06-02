#include "src/cfg/BuildConfig.h"

#if PRTN_ENABLE_WIFI

#include "src/dvc/inc/EspNow.h"

#include <cstring>

namespace
{
    constexpr uint8_t BROADCAST_MAC[ESP_NOW_ETH_ALEN] {
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
    };

    bool sameMac(const uint8_t* lhs, const uint8_t* rhs) {
        return lhs != nullptr && rhs != nullptr && std::memcmp(lhs, rhs, ESP_NOW_ETH_ALEN) == 0;
    }

    bool isZeroMac(const uint8_t* mac) {
        static constexpr uint8_t ZERO_MAC[ESP_NOW_ETH_ALEN] {};
        return sameMac(mac, ZERO_MAC);
    }

    void fillEspPeerInfo(const EspNow::Peer& peer, esp_now_peer_info_t& espPeerInfo) {
        std::memset(&espPeerInfo, 0, sizeof(espPeerInfo));
        std::memcpy(espPeerInfo.peer_addr, peer.mac, sizeof(espPeerInfo.peer_addr));
        std::memcpy(espPeerInfo.lmk, peer.lmk, sizeof(espPeerInfo.lmk));

        espPeerInfo.channel = peer.channel;
        espPeerInfo.ifidx   = WIFI_IF_STA;
        espPeerInfo.encrypt = peer.encrypted;
        espPeerInfo.priv    = nullptr;
    }

    EspNow::RtnErrCode mapSendError(esp_err_t err) {
        switch (err) {
            case ESP_OK:
                return EspNow::RtnErrCode::OK;
            case ESP_ERR_ESPNOW_NOT_INIT:
                return EspNow::RtnErrCode::NOT_INITIALIZED;
            case ESP_ERR_ESPNOW_NOT_FOUND:
                return EspNow::RtnErrCode::PEER_NOT_FOUND;
            default:
                return EspNow::RtnErrCode::SEND_FAILED;
        }
    }
} // namespace

esp_now_send_cb_t EspNow::m_onSendCallback = nullptr;
esp_now_recv_cb_t EspNow::m_onRecvCallback = nullptr;

EspNow& EspNow::instance() {
    static EspNow instance;
    return instance;
}

EspNow::EspNow() = default;

EspNow::RtnErrCode EspNow::setup(
    Wifi*             wifi,
    const Config&     config,
    esp_now_recv_cb_t onRecvCallback,
    esp_now_send_cb_t onSendCallback) {

    if (m_initialized) {
        return RtnErrCode::ALREADY_INITIALIZED;
    }

    if (wifi == nullptr) {
        return RtnErrCode::INVALID_WIFI_INSTANCE;
    }

    const Wifi::Status wifiStatus = wifi->status();
    if (!wifiStatus.started || wifiStatus.mode != Wifi::Mode::STA) {
        return RtnErrCode::INVALID_WIFI_INSTANCE;
    }

    if (config.mode == Mode::NONE || config.peerCount > config.maxPeers) {
        return RtnErrCode::INVALID_CONFIG;
    }

    if (config.maxPeers > 0 && config.peers == nullptr) {
        return RtnErrCode::INVALID_CONFIG;
    }

    if (config.mode == Mode::UNICAST || config.mode == Mode::UNICAST_BROADCAST) {
        for (uint8_t i = 0; i < config.peerCount; ++i) {
            if (!isValidPeer(config.peers[i])) {
                return RtnErrCode::INVALID_CONFIG;
            }
        }
    }

    if (esp_now_init() != ESP_OK) {
        return RtnErrCode::INIT_FAILED;
    }

    m_wifi           = wifi;
    m_config         = config;
    m_onRecvCallback = nullptr;
    m_onSendCallback = nullptr;
    m_initialized    = true;

    if (onRecvCallback != nullptr && setOnRecvCallback(onRecvCallback) != RtnErrCode::OK) {
        esp_now_deinit();
        m_initialized = false;
        return RtnErrCode::CALLBACK_FAILED;
    }

    if (onSendCallback != nullptr && setOnSendCallback(onSendCallback) != RtnErrCode::OK) {
        esp_now_unregister_recv_cb();
        esp_now_deinit();
        m_initialized = false;
        return RtnErrCode::CALLBACK_FAILED;
    }

    if (broadcastEnabled()) {
        const RtnErrCode err = enableBroadcastPeer();
        if (err != RtnErrCode::OK) {
            esp_now_unregister_recv_cb();
            esp_now_unregister_send_cb();
            esp_now_deinit();
            m_initialized = false;
            return err;
        }
    }

    if (unicastEnabled()) {
        for (uint8_t i = 0; i < m_config.peerCount; ++i) {
            const RtnErrCode err = enablePeer(m_config.peers[i]);
            if (err != RtnErrCode::OK) {
                for (uint8_t j = 0; j < i; ++j) {
                    disablePeer(m_config.peers[j]);
                }

                esp_now_unregister_recv_cb();
                esp_now_unregister_send_cb();
                esp_now_deinit();
                m_initialized = false;
                return err;
            }
        }
    }

    return RtnErrCode::OK;
}

EspNow::RtnErrCode EspNow::addPeer(const Peer& peer) {
    if (!m_initialized) {
        return RtnErrCode::NOT_INITIALIZED;
    }

    if (!unicastEnabled()) {
        return RtnErrCode::INVALID_CONFIG;
    }

    if (!isValidPeer(peer)) {
        return RtnErrCode::INVALID_ARGUMENT;
    }

    if (m_config.peerCount >= m_config.maxPeers) {
        return RtnErrCode::PEER_TABLE_FULL;
    }

    if (findPeerIndexByMac(peer.mac) >= 0) {
        return RtnErrCode::PEER_ALREADY_EXISTS;
    }

    const RtnErrCode err = enablePeer(peer);
    if (err != RtnErrCode::OK) {
        return err;
    }

    m_config.peers[m_config.peerCount++] = peer;
    return RtnErrCode::OK;
}

EspNow::RtnErrCode EspNow::removePeerByMac(const uint8_t* peerMac) {
    if (!m_initialized) {
        return RtnErrCode::NOT_INITIALIZED;
    }

    const int peerIndex = findPeerIndexByMac(peerMac);
    if (peerIndex < 0) {
        return RtnErrCode::PEER_NOT_FOUND;
    }

    const RtnErrCode err = disablePeer(m_config.peers[peerIndex]);
    if (err != RtnErrCode::OK) {
        return err;
    }

    removePeerFromConfig(static_cast<uint8_t>(peerIndex));
    return RtnErrCode::OK;
}

int EspNow::getPeerCount() const {
    return m_config.peerCount;
}

void EspNow::getPeerCount(int* outCnt) {
    if (outCnt != nullptr) {
        *outCnt = getPeerCount();
    }
}

EspNow::RtnErrCode EspNow::sendBroadcast(const uint8_t* data, size_t len) {
    if (!m_initialized) {
        return RtnErrCode::NOT_INITIALIZED;
    }

    if (!broadcastEnabled()) {
        return RtnErrCode::INVALID_CONFIG;
    }

    if (data == nullptr || len == 0 || len > ESP_NOW_MAX_DATA_LEN) {
        return RtnErrCode::INVALID_ARGUMENT;
    }

    return mapSendError(esp_now_send(BROADCAST_MAC, data, len));
}

EspNow::RtnErrCode EspNow::sendUnicast(const uint8_t* data, size_t len, const uint8_t* peerMac) {
    if (!m_initialized) {
        return RtnErrCode::NOT_INITIALIZED;
    }

    if (!unicastEnabled()) {
        return RtnErrCode::INVALID_CONFIG;
    }

    if (data == nullptr || len == 0 || len > ESP_NOW_MAX_DATA_LEN || peerMac == nullptr) {
        return RtnErrCode::INVALID_ARGUMENT;
    }

    if (findPeerIndexByMac(peerMac) < 0) {
        return RtnErrCode::PEER_NOT_FOUND;
    }

    return mapSendError(esp_now_send(peerMac, data, len));
}

EspNow::RtnErrCode EspNow::setOnRecvCallback(esp_now_recv_cb_t callback) {
    if (!m_initialized) {
        return RtnErrCode::NOT_INITIALIZED;
    }

    const esp_err_t err = callback == nullptr
                              ? esp_now_unregister_recv_cb()
                              : esp_now_register_recv_cb(callback);

    if (err != ESP_OK) {
        return RtnErrCode::CALLBACK_FAILED;
    }

    m_onRecvCallback = callback;
    return RtnErrCode::OK;
}

EspNow::RtnErrCode EspNow::setOnSendCallback(esp_now_send_cb_t callback) {
    if (!m_initialized) {
        return RtnErrCode::NOT_INITIALIZED;
    }

    const esp_err_t err = callback == nullptr
                              ? esp_now_unregister_send_cb()
                              : esp_now_register_send_cb(callback);

    if (err != ESP_OK) {
        return RtnErrCode::CALLBACK_FAILED;
    }

    m_onSendCallback = callback;
    return RtnErrCode::OK;
}

EspNow::RtnErrCode EspNow::enablePeer(const Peer& peer) {
    esp_now_peer_info_t espPeerInfo {};
    fillEspPeerInfo(peer, espPeerInfo);

    const esp_err_t err = esp_now_add_peer(&espPeerInfo);
    switch (err) {
        case ESP_OK:
            return RtnErrCode::OK;
        case ESP_ERR_ESPNOW_FULL:
            return RtnErrCode::PEER_TABLE_FULL;
        case ESP_ERR_ESPNOW_EXIST:
            return RtnErrCode::PEER_ALREADY_EXISTS;
        case ESP_ERR_ESPNOW_NOT_INIT:
            return RtnErrCode::NOT_INITIALIZED;
        default:
            return RtnErrCode::ADD_PEER_FAILED;
    }
}

EspNow::RtnErrCode EspNow::disablePeer(const Peer& peer) {
    const esp_err_t err = esp_now_del_peer(peer.mac);
    switch (err) {
        case ESP_OK:
            return RtnErrCode::OK;
        case ESP_ERR_ESPNOW_NOT_FOUND:
            return RtnErrCode::PEER_NOT_FOUND;
        case ESP_ERR_ESPNOW_NOT_INIT:
            return RtnErrCode::NOT_INITIALIZED;
        default:
            return RtnErrCode::REMOVE_PEER_FAILED;
    }
}

EspNow::RtnErrCode EspNow::enableBroadcastPeer() {
    Peer peer {};
    std::memcpy(peer.mac, BROADCAST_MAC, sizeof(peer.mac));
    peer.channel   = 0;
    peer.encrypted = false;

    return enablePeer(peer);
}

int EspNow::findPeerIndexByMac(const uint8_t* peerMac) const {
    if (peerMac == nullptr || m_config.peers == nullptr) {
        return -1;
    }

    for (uint8_t i = 0; i < m_config.peerCount; ++i) {
        if (sameMac(m_config.peers[i].mac, peerMac)) {
            return i;
        }
    }

    return -1;
}

bool EspNow::isValidPeer(const Peer& peer) const {
    return !isZeroMac(peer.mac) && !sameMac(peer.mac, BROADCAST_MAC);
}

bool EspNow::unicastEnabled() const {
    return m_config.mode == Mode::UNICAST || m_config.mode == Mode::UNICAST_BROADCAST;
}

bool EspNow::broadcastEnabled() const {
    return m_config.mode == Mode::BROADCAST || m_config.mode == Mode::UNICAST_BROADCAST;
}

void EspNow::removePeerFromConfig(uint8_t peerIndex) {
    if (m_config.peers == nullptr || peerIndex >= m_config.peerCount) {
        return;
    }

    for (uint8_t i = peerIndex; i + 1 < m_config.peerCount; ++i) {
        m_config.peers[i] = m_config.peers[i + 1];
    }

    --m_config.peerCount;
}

#endif
