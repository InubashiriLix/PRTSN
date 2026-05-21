#pragma once

#include "src/cfg/ProfileConfig.h"
#include "src/cfg/ProjectConfig.h"

#define PRTN_BOARD_NAME "AirM2M CORE ESP32-C3"

// Serial ---------------------------------------------------------------------
#ifndef PRTN_SERIAL_BAUD
#define PRTN_SERIAL_BAUD 115200
#endif

// LED ------------------------------------------------------------------------
#ifndef PRTN_LED_PIN
#define PRTN_LED_PIN 12
#endif

#ifndef PRTN_LED_PIN_AUX
#define PRTN_LED_PIN_AUX 13
#endif

#ifndef PRTN_LED_ACTIVE_HIGH
#define PRTN_LED_ACTIVE_HIGH 1
#endif

// I2C ------------------------------------------------------------------------
#ifndef PRTN_IIC_SCL_PIN
#define PRTN_IIC_SCL_PIN 5
#endif

#ifndef PRTN_IIC_SDA_PIN
#define PRTN_IIC_SDA_PIN 4
#endif

#ifndef PRTN_IIC_FREQUENCY
#define PRTN_IIC_FREQUENCY 400000
#endif

#ifndef PRTN_IIC_TIMEOUT_MS
#define PRTN_IIC_TIMEOUT_MS 50
#endif
