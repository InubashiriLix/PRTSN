#pragma once

#include <cstddef>

#include "src/cfg/BoardConfig.h"
#include "src/cfg/ProfileConfig.h"
#include "src/cfg/ProjectConfig.h"
#include "src/ctl/inc/EspNowEchoController.h"
#include "src/dvc/inc/LED.h"
#include "src/dvc/inc/Serial.h"
#include "src/svc/inc/EspNowNode.h"
#include "src/svc/inc/wifi.h"

struct AppConfig
{
    struct Runtime
    {
        static constexpr uint32_t AppLoopIntervalMs       = 10;
        static constexpr uint32_t HeartbeatIntervalMs     = 1000;
        static constexpr uint32_t EspNowTaskStackWords    = 1024 * 4;
        static constexpr uint32_t EspNowTaskPriority      = 4;
        static constexpr uint32_t HeartbeatTaskStackWords = 1024 * 3;
        static constexpr uint32_t HeartbeatTaskPriority   = 1;
    };

    struct Mqtt
    {
        static constexpr uint8_t  ServerMaxClients       = 4;
        static constexpr uint8_t  ServerMaxSubscriptions = 16;
        static constexpr uint16_t PacketBufferSize       = 256;
        static constexpr uint16_t ClientPacketBufferSize = PacketBufferSize;
        static constexpr uint16_t ServerPort             = 1883;
        static constexpr uint16_t ClientKeepAliveSec     = 30;
        static constexpr uint16_t ClientMaxPacketSize    = PacketBufferSize;
        static constexpr uint32_t ClientReconnectMs      = 5000;
    };

    struct Identity
    {
        static constexpr const char* ProjectName     = PRTN_PROJECT_NAME;
        static constexpr const char* ProjectFullName = PRTN_PROJECT_FULL_NAME;
        static constexpr const char* BoardName       = PRTN_BOARD_NAME;
        static constexpr const char* VersionString   = PRTN_VERSION_STRING;
        static constexpr const char* NodeId          = PRTN_NODE_ID;
        static constexpr const char* NodeName        = PRTN_NODE_NAME;
    };

    struct Hardware
    {
        static constexpr uint8_t  LedMainPin     = LED::DefaultMainPin;
        static constexpr uint8_t  LedAuxPin      = LED::DefaultAuxPin;
        static constexpr uint32_t SerialBaudrate = dvc::Serial::DefaultBaudrate;
    };

    struct Network
    {
        static constexpr Wifi::Mode  WifiMode       = Wifi::Mode::STA;
        static constexpr const char* StaSsid        = "PRTN-AP-MQTT";
        static constexpr const char* StaPassword    = "12345678";
        static constexpr bool        StaConnect     = false;
        static constexpr uint8_t     StaChannel     = Wifi::DefaultStaChannel;
        static constexpr uint32_t    StaReconnectMs = Wifi::DefaultStaReconnectMs;
    };

    struct EspNowEcho
    {
        static constexpr bool     Sender                = PRTN_ESPNOW_ECHO_ENABLE_SENDER != 0;
        static constexpr bool     Receiver              = PRTN_ESPNOW_ECHO_ENABLE_RECEIVER != 0;
        static constexpr bool     UseAck                = EspNowEchoController::DefaultUseAck;
        static constexpr bool     VerboseLog            = EspNowEchoController::DefaultVerboseLog;
        static constexpr bool     StatsLog              = EspNowEchoController::DefaultStatsLog;
        static constexpr size_t   PayloadLen            = EspNowEchoController::DefaultPayloadLen;
        static constexpr uint32_t SendIntervalMs        = EspNowEchoController::DefaultSendIntervalMs;
        static constexpr uint32_t StatsLogIntervalMs    = EspNowEchoController::DefaultStatsLogIntervalMs;
        static constexpr uint32_t ActivityLedWindowMs   = EspNowEchoController::DefaultActivityLedWindowMs;
        static constexpr uint32_t ActivityLedIntervalMs = EspNowEchoController::DefaultActivityLedIntervalMs;
    };

    struct EspNow
    {
        static constexpr uint32_t DiscoveryIntervalMs = EspNowNode::DefaultDiscoveryIntervalMs;
        static constexpr uint32_t HeartbeatIntervalMs = EspNowNode::DefaultHeartbeatIntervalMs;
        static constexpr uint32_t StaleTimeoutMs      = EspNowNode::DefaultStaleTimeoutMs;
        static constexpr uint32_t OfflineTimeoutMs    = EspNowNode::DefaultOfflineTimeoutMs;
        static constexpr uint32_t AckTimeoutMs        = EspNowNode::DefaultAckTimeoutMs;
        static constexpr uint8_t  MaxRetries          = EspNowNode::DefaultMaxRetries;
    };
};
