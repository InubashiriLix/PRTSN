#include "src/dvc/inc/Serial.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <Arduino.h>
#include <cstdarg>

#ifdef Serial
#undef Serial
#endif

namespace dvc
{
    Serial::Serial(uint32_t baudrate, uint8_t uartNumber)
        : m_port(uartNumber),
          m_baudrate(baudrate),
          m_updateMs(0),
          m_mutex(xSemaphoreCreateMutex()) {}

    bool Serial::lock() {
        return m_mutex != nullptr && xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE;
    }

    void Serial::unlock() {
        if (m_mutex != nullptr) {
            xSemaphoreGive(m_mutex);
        }
    }

    bool Serial::setup() {
        return setup(m_baudrate);
    }

    bool Serial::setup(unsigned long baud) {
        if (!lock()) {
            return false;
        }

        m_baudrate = baud;
        m_port.begin(m_baudrate);
        m_updateMs = millis();
        unlock();

        return true;
    }

    int Serial::available() {
        if (!lock()) {
            return 0;
        }

        const int count = m_port.available();
        unlock();
        return count;
    }

    int Serial::read() {
        if (!lock()) {
            return -1;
        }

        const int byte = m_port.read();
        if (byte >= 0) {
            m_updateMs = millis();
        }
        unlock();
        return byte;
    }

    size_t Serial::read(uint8_t* buffer, size_t len) {
        if (buffer == nullptr || len == 0 || !lock()) {
            return 0;
        }

        size_t readLen = 0;
        while (readLen < len && m_port.available() > 0) {
            const int byte = m_port.read();
            if (byte < 0) {
                break;
            }
            buffer[readLen++] = static_cast<uint8_t>(byte);
        }

        if (readLen > 0) {
            m_updateMs = millis();
        }
        unlock();
        return readLen;
    }

    bool Serial::write(uint8_t byte) {
        if (!lock()) {
            return false;
        }

        const bool ok = m_port.write(byte) == 1;
        m_updateMs    = millis();
        unlock();
        return ok;
    }

    bool Serial::write(const uint8_t* data, size_t len) {
        if ((data == nullptr && len > 0) || !lock()) {
            return false;
        }

        const size_t written = len > 0 ? m_port.write(data, len) : 0;
        m_updateMs           = millis();
        unlock();
        return written == len;
    }

    bool Serial::print(const char* message) {
        if (!lock()) {
            return false;
        }

        const bool ok = m_port.print(message) > 0;
        m_updateMs    = millis();
        unlock();

        return ok;
    }

    bool Serial::println() {
        if (!lock()) {
            return false;
        }

        const bool ok = m_port.println() > 0;
        m_updateMs    = millis();
        unlock();

        return ok;
    }

    bool Serial::println(const char* message) {
        if (!lock()) {
            return false;
        }

        const bool ok = m_port.println(message) > 0;
        m_updateMs    = millis();
        unlock();

        return ok;
    }

    bool Serial::printf(const char* format, ...) {
        va_list args;
        va_start(args, format);
        const bool ok = vprintf(format, args);
        va_end(args);
        return ok;
    }

    bool Serial::vprintf(const char* format, va_list args) {
        if (format == nullptr || !lock()) {
            return false;
        }

        const size_t written = m_port.vprintf(format, args);
        m_updateMs           = millis();
        unlock();
        return written > 0;
    }
} // namespace dvc
