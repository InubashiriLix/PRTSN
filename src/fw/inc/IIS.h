#pragma once

#include "driver/i2s_types.h"
#include "driver/i2s_types_legacy.h"
#include <driver/i2s.h>

class IIS
{
public:
    struct Config
    {
        i2s_port_t          port         = I2S_NUM_0;
        i2s_driver_config_t driverConfig = {};
        i2s_pin_config_t    pinConfig    = {};
    };

    enum class Err : int32_t
    {
        UNKNOWN       = 0xFFF,
        IO_ERROR      = ESP_FAIL,
        OK            = ESP_OK,
        NO_MEM        = ESP_ERR_NO_MEM,
        INVALID_ARG   = ESP_ERR_INVALID_ARG,
        INVALID_STATE = ESP_ERR_INVALID_STATE,
    };

    [[nodiscard]] constexpr Err toErr(esp_err_t code) noexcept {
        const auto err = static_cast<Err>(code);

        switch (err) {
            case Err::IO_ERROR:
            case Err::OK:
            case Err::NO_MEM:
            case Err::INVALID_ARG:
            case Err::INVALID_STATE:
                return err;

            default:
                return Err::UNKNOWN;
        }
    }

    explicit IIS(const Config& config);
    ~IIS();
    Err  setup();
    void end();
    Err  read(void* data, size_t size, size_t& bytesRead, TickType_t ticksToWait);

protected:
    const Config& getConfig() const {
        return m_config;
    }

private:
    Config m_config;
    bool   m_started = false;
};
