#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>

#include "HardwareSerial.h"
#include "freertos/idf_additions.h"

#ifdef Serial
#undef Serial
#endif

namespace dvc
{
    class Serial
    {
    public:
        static constexpr uint32_t DefaultBaudrate   = 115200;
        static constexpr uint8_t  DefaultUartNumber = 0;

    private:
        HardwareSerial m_port;
        uint32_t       m_baudrate;
        uint32_t       m_updateMs;

        SemaphoreHandle_t m_mutex = nullptr;
        bool              lock();
        void              unlock();

    public:
        explicit Serial(uint32_t baudrate = DefaultBaudrate, uint8_t uartNumber = DefaultUartNumber);

        bool setup();
        bool setup(unsigned long baud);

        int    available();
        int    read();
        size_t read(uint8_t* buffer, size_t len);

        bool write(uint8_t byte);
        bool write(const uint8_t* data, size_t len);

        bool print(const char* message);
        bool println();
        bool println(const char* message);
        bool printf(const char* format, ...) __attribute__((format(printf, 2, 3)));
        bool vprintf(const char* format, va_list args);
    };
} // namespace dvc
