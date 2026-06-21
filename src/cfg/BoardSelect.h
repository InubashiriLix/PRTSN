#pragma once

#if defined(PRTN_BOARD_ID_ESP32S3_N16R8_DEV) || defined(PRTN_BOARD_CHIP_ESP32S3)
#include "src/cfg/boards/ESP32S3_N16R8_DEV.h"
namespace prtn
{
    namespace b = s3;
}
#elif defined(PRTN_BOARD_ID_AIRM2M_CORE_ESP32C3) || defined(PRTN_BOARD_CHIP_ESP32C3)
#include "src/cfg/boards/AirM2M_CORE_ESP32C3.h"
namespace prtn
{
    namespace b = c3;
}
#else
#include "src/cfg/boards/AirM2M_CORE_ESP32C3.h"
namespace prtn
{
    namespace b = c3;
}
#endif
