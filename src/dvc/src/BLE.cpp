#include "src/dvc/inc/BLE.h"

#include "src/cfg/BuildConfig.h"

#if PRTN_ENABLE_BLE

BLE::BLE(const Config& config) : m_config(config) {}

bool BLE::setup() {
    if (m_started)
        return false;
}

void BLE::end() {}

bool BLE::startAdvertising() const {}

#endif
