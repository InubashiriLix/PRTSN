#include "../inc/SerialConsole.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <Arduino.h>
#include <cstdarg>

SerialConsole::SerialConsole(uint32_t baudrate) : m_baudrate(baudrate), m_updateMs(0), m_mutex(xSemaphoreCreateMutex()) {}

bool SerialConsole::lock() {
    return m_mutex != nullptr && xSemaphoreTake(m_mutex, portMAX_DELAY) ==
                                     pdTRUE;
}

void SerialConsole::unlock() {
    if (m_mutex != nullptr) {
        xSemaphoreGive(m_mutex);
    }
}

bool SerialConsole::setup() {
    return setup(m_baudrate);
}

bool SerialConsole::setup(unsigned long baud) {
    if (!lock()) {
        return false;
    }

    m_baudrate = baud;
    Serial.begin(m_baudrate);
    m_updateMs = millis();
    unlock();

    return true;
}

bool SerialConsole::print(const char* message) {
    if (!lock()) {
        return false;
    }

    const bool ok = Serial.print(message) > 0;
    m_updateMs = millis();
    unlock();

    return ok;
}

bool SerialConsole::println() {
    if (!lock()) {
        return false;
    }

    const bool ok = Serial.println() > 0;
    m_updateMs = millis();
    unlock();

    return ok;
}

bool SerialConsole::println(const char* message) {
    if (!lock()) {
        return false;
    }

    const bool ok = Serial.println(message) > 0;
    m_updateMs = millis();
    unlock();

    return ok;
}

bool SerialConsole::printf(const char* format, ...) {
    if (!lock()) {
        return false;
    }

    va_list args;
    va_start(args, format);
    const size_t written = Serial.vprintf(format, args);
    va_end(args);
    m_updateMs = millis();
    unlock();

    return written > 0;
}

void SerialConsole::log(const char* format, ...) {
    if (!lock()) {
        return;
    }

    va_list args;
    va_start(args, format);
    Serial.print("[log] ");
    Serial.vprintf(format, args);
    Serial.println();
    va_end(args);
    m_updateMs = millis();
    unlock();
}

void SerialConsole::error(const char* format, ...) {
    if (!lock()) {
        return;
    }

    va_list args;
    va_start(args, format);
    Serial.print("[error] ");
    Serial.vprintf(format, args);
    Serial.println();
    va_end(args);
    m_updateMs = millis();
    unlock();
}

void SerialConsole::debug(const char* format, ...) {
    if (!lock()) {
        return;
    }

    va_list args;
    va_start(args, format);
    Serial.print("[debug] ");
    Serial.vprintf(format, args);
    Serial.println();
    va_end(args);
    m_updateMs = millis();
    unlock();
}

void SerialConsole::printBootBanner(const NodeInfo& info) {
    if (!lock()) {
        return;
    }

    Serial.println();
    Serial.println("== PRTN boot ==");
    Serial.printf("Project : %s (%s)\n", info.projectName, info.fullName);
    Serial.printf("Board   : %s\n", info.boardName);
    Serial.printf("Version : %s\n", info.version);
    Serial.printf("Node    : %s (%s)\n", info.nodeName, info.nodeId);
    m_updateMs = millis();
    unlock();
}

void SerialConsole::printState(NodeState state) {
    if (!lock()) {
        return;
    }

    Serial.printf("[state] %s\n", stateName(state));
    m_updateMs = millis();
    unlock();
}

void SerialConsole::printHeartbeat(const NodeInfo& info) {
    if (!lock()) {
        return;
    }

    Serial.printf("[heartbeat] node=%s uptime=%lu ms\n", info.nodeId, millis());
    m_updateMs = millis();
    unlock();
}

const char* SerialConsole::stateName(NodeState state) const {
    switch (state) {
        case BOOTING:
            return "Booting";
        case RUNNING:
            return "Running";
        case ERROR:
            return "Error";
    }

    return "Unknown";
}
