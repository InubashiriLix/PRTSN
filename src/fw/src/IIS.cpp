#include "src/fw/inc/IIS.h"
#include "driver/i2s.h"
#include "driver/i2s_types_legacy.h"

// clang-format off
IIS::IIS(const Config& config) : m_config(config) {}
IIS::~IIS() {end();}
// clang-format on

IIS::Err IIS::setup() {
    if (m_started) {
        // you shall not setup an already started IIS
        return Err::INVALID_STATE;
    }

    // TODO: the queue might should be supported
    auto err = toErr(
        i2s_driver_install(
            m_config.port,
            &m_config.driverConfig,
            0,
            NULL));
    if (err != Err::OK) {
        return err;
    }
    err = toErr(i2s_set_pin(m_config.port, &m_config.pinConfig));
    if (err != Err::OK) {
        i2s_driver_uninstall(m_config.port);
        return err;
    }

    m_started = true;
    return Err::OK;
}

void IIS::end() {
    if (m_started) {
        i2s_driver_uninstall(m_config.port);
        m_started = false;
    }
}

IIS::Err IIS::read(void* data, size_t size, size_t& bytesRead, TickType_t ticksToWait) {
    bytesRead = 0;
    if (!m_started || data == nullptr || size == 0) {
        return Err::INVALID_STATE;
    }

    return toErr(i2s_read(m_config.port, data, size, &bytesRead, ticksToWait));
}
