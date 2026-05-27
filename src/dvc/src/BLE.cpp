#include "src/dvc/inc/BLE.h"

BLE::BLE(const Config& config) : m_config(config) {}

bool BLE::setup() {
    if (m_started)
        return false;
}

void BLE::end() {}

bool BLE::startAdvertising() const {}
