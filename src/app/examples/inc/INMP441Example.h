#pragma once

#include "src/cfg/AppConfig.h"
#include "src/dom/NodeInfo.h"
#include "src/dvc/inc/INMP441.h"
#include "src/dvc/inc/Serial.h"
#include "src/svc/inc/SerialConsoleService.h"

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef Serial
#undef Serial
#endif

namespace INMP441Example
{
    namespace Detail
    {
        enum class PinId : uint8_t
        {
            Bck,
            Ws,
            DataIn,
        };

        inline constexpr auto Pins = ::prtn::pin::layout(
            ::prtn::pin::bind(PinId::Bck, GPIO_NUM_6, ::prtn::pin::Role::I2sBclk),
            ::prtn::pin::bind(PinId::Ws, GPIO_NUM_7, ::prtn::pin::Role::I2sWs),
            ::prtn::pin::bind(PinId::DataIn, GPIO_NUM_10, ::prtn::pin::Role::I2sDataIn));

        constexpr uint32_t          SampleRate = 16000;
        constexpr i2s_channel_fmt_t Channel    = I2S_CHANNEL_FMT_ONLY_LEFT; // INMP441 L/R tied to GND.

        constexpr size_t      SampleCount      = 256;
        constexpr TickType_t  ReadTimeoutTicks = pdMS_TO_TICKS(100);
        constexpr uint32_t    LogIntervalMs    = 500;
        constexpr uint32_t    TaskStackWords   = 1024 * 4;
        constexpr UBaseType_t TaskPriority     = 4;

        struct Context
        {
            NodeInfo nodeInfo {
                AppConfig::Identity::ProjectName,
                AppConfig::Identity::ProjectFullName,
                AppConfig::Identity::BoardName,
                AppConfig::Identity::VersionString,
                AppConfig::Identity::NodeName,
                AppConfig::Identity::NodeId,
                BOOTING,
            };

            dvc::Serial          serial {AppConfig::Hardware::SerialBaudrate};
            SerialConsoleService console {serial};
            INMP441              mic {Pins[PinId::Bck], Pins[PinId::Ws], Pins[PinId::DataIn], SampleRate, Channel};

            int32_t      samples[SampleCount] {};
            TaskHandle_t taskHandle = nullptr;
            uint32_t     lastLogMs  = 0;
        };

        inline Context& context() {
            static Context app;
            return app;
        }

        inline int32_t sample24FromRaw32(int32_t raw) {
            return raw >> 8;
        }

        inline uint32_t abs32(int32_t value) {
            if (value >= 0) {
                return static_cast<uint32_t>(value);
            }
            return static_cast<uint32_t>(-(value + 1)) + 1;
        }

        inline void logStats(Context& app, size_t samplesRead) {
            if (samplesRead == 0) {
                app.console.warn("INMP441 read returned 0 samples");
                return;
            }

            int32_t  minSample = sample24FromRaw32(app.samples[0]);
            int32_t  maxSample = minSample;
            uint32_t peak      = abs32(minSample);
            uint64_t absSum    = 0;

            for (size_t i = 0; i < samplesRead; ++i) {
                const int32_t  sample = sample24FromRaw32(app.samples[i]);
                const uint32_t mag    = abs32(sample);

                if (sample < minSample) {
                    minSample = sample;
                }
                if (sample > maxSample) {
                    maxSample = sample;
                }
                if (mag > peak) {
                    peak = mag;
                }
                absSum += mag;
            }

            const uint32_t meanAbs = static_cast<uint32_t>(absSum / samplesRead);
            const auto&    stats   = app.mic.stats();

            app.console.info("INMP441 samples=%u peak=%lu meanAbs=%lu min=%ld max=%ld healthy=%u readOk=%lu readErr=%lu rxDone=%lu rxOvf=%lu dmaErr=%lu",
                             static_cast<unsigned>(samplesRead),
                             static_cast<unsigned long>(peak),
                             static_cast<unsigned long>(meanAbs),
                             static_cast<long>(minSample),
                             static_cast<long>(maxSample),
                             static_cast<unsigned>(app.mic.healthy()),
                             static_cast<unsigned long>(stats.readOk),
                             static_cast<unsigned long>(stats.readError),
                             static_cast<unsigned long>(stats.rxDone),
                             static_cast<unsigned long>(stats.rxOverflow),
                             static_cast<unsigned long>(stats.dmaError));
        }

        inline void taskEntry(void*) {
            Context& app = context();

            for (;;) {
                app.console.updateCommandResponse();

                size_t     samplesRead = 0;
                const auto err         = app.mic.readRaw(app.samples, SampleCount, samplesRead, ReadTimeoutTicks);

                const uint32_t nowMs = millis();
                if (nowMs - app.lastLogMs >= LogIntervalMs) {
                    app.lastLogMs = nowMs;

                    if (err != INMP441::Err::OK) {
                        const auto& stats = app.mic.stats();
                        app.console.error("INMP441 read failed: %ld healthy=%u readOk=%lu readErr=%lu rxDone=%lu rxOvf=%lu dmaErr=%lu",
                                          static_cast<long>(err),
                                          static_cast<unsigned>(app.mic.healthy()),
                                          static_cast<unsigned long>(stats.readOk),
                                          static_cast<unsigned long>(stats.readError),
                                          static_cast<unsigned long>(stats.rxDone),
                                          static_cast<unsigned long>(stats.rxOverflow),
                                          static_cast<unsigned long>(stats.dmaError));
                    }
                    else {
                        logStats(app, samplesRead);
                    }
                }

                taskYIELD();
            }
        }

        inline bool startTask() {
            Context& app = context();
            if (app.taskHandle != nullptr) {
                return true;
            }

            const BaseType_t ok = xTaskCreate(
                taskEntry,
                "inmp441 example",
                TaskStackWords,
                nullptr,
                TaskPriority,
                &app.taskHandle);

            if (ok != pdPASS) {
                app.taskHandle = nullptr;
                app.nodeInfo.updateNodeState(ERROR);
                app.console.error("failed to create INMP441 example task");
                return false;
            }

            return true;
        }
    }

    inline void setup() {
        Detail::Context& app = Detail::context();

        app.console.setup();
        app.console.printBootBanner(app.nodeInfo);
        app.console.info("INMP441 pins: bck=%d ws=%d data=%d sampleRate=%lu channel=left",
                         Detail::Pins[Detail::PinId::Bck],
                         Detail::Pins[Detail::PinId::Ws],
                         Detail::Pins[Detail::PinId::DataIn],
                         static_cast<unsigned long>(Detail::SampleRate));

        const auto err = app.mic.setup();
        if (err != INMP441::Err::OK) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.error("failed to setup INMP441: %ld", static_cast<long>(err));
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }

        app.nodeInfo.updateNodeState(RUNNING);
        app.console.printState(app.nodeInfo.getNodeState());

        if (!Detail::startTask()) {
            app.console.printState(app.nodeInfo.getNodeState());
        }
    }

    inline void idle() {
        Detail::context().console.updateCommandResponse();
        delay(AppConfig::Runtime::AppLoopIntervalMs);
    }
}
