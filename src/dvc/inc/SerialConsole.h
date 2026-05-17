#pragma once

#include <cstddef>
#include <cstdint>

#include "../../cfg/BoardConfig.h"
#include "../../dom/NodeInfo.h"
#include "freertos/idf_additions.h"

class SerialConsole
{
private:
    uint32_t m_baudrate;
    uint32_t m_updateMs;

    // mutex in case of multiple tasks writing to the console
    SemaphoreHandle_t m_mutex = nullptr;
    bool              lock();
    void              unlock();

public:
    SerialConsole(uint32_t baudrate = PRTN_SERIAL_BAUD);

    bool setup();
    bool setup(unsigned long baud);

    bool print(const char* message);
    bool println();
    bool println(const char* message);
    bool printf(const char* format, ...) __attribute__((format(printf, 2, 3)));

    void log(const char* format, ...) __attribute__((format(printf, 2, 3)));
    void error(const char* format, ...) __attribute__((format(printf, 2, 3)));
    void debug(const char* format, ...) __attribute__((format(printf, 2, 3)));

    void printBootBanner(const NodeInfo& info);
    void printState(NodeState state);
    void printHeartbeat(const NodeInfo& info);

private:
    const char* stateName(NodeState state) const;
};
