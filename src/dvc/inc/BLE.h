#pragma once
// #include <BLEDriver.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>

class BLE
{
public:
    struct Config
    {
        const char* deviceName;
        bool        advertiseOnStart;
    };

private:
    Config m_config;
    bool   m_started     = false;
    bool   m_advertising = false;
    bool   m_connected   = false;

public:
    BLE(const Config& config);

    bool setup();
    void end();

    bool startAdvertising() const;
    bool advertising() const;
    bool connect() const;
};
