#pragma once

#include "src/cfg/AppConfig.h"
#include "src/dom/NodeInfo.h"
#include "src/dvc/inc/ST7735.h"
#include "src/dvc/inc/Serial.h"
#include "src/svc/inc/SerialConsoleService.h"

#include <Arduino.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdio>

#ifdef Serial
#undef Serial
#endif

namespace ST7735FpsTextExample
{
    namespace Detail
    {
        enum class PinId : uint8_t
        {
            Reset,
            Sck,
            Mosi,
            Cs,
            Dc,
            Backlight,
        };

        inline constexpr auto Pins = ::prtn::pin::layout(
            ::prtn::pin::bind(PinId::Reset, GPIO_NUM_1, ::prtn::pin::Role::Output),
            ::prtn::pin::bind(PinId::Sck, ::prtn::b::spi::sck, ::prtn::pin::Role::SpiSck),
            ::prtn::pin::bind(PinId::Mosi, ::prtn::b::spi::mosi, ::prtn::pin::Role::SpiMosi),
            ::prtn::pin::bind(PinId::Cs, ::prtn::b::spi::cs, ::prtn::pin::Role::SpiCs),
            ::prtn::pin::bind(PinId::Dc, GPIO_NUM_0, ::prtn::pin::Role::Output),
            ::prtn::pin::bind(PinId::Backlight, GPIO_NUM_10, ::prtn::pin::Role::Output));

        constexpr uint32_t SpiClockHz     = 20 * 1000 * 1000;
        constexpr size_t   DeviceIndex    = 0;
        constexpr size_t   DmaBufferBytes = 3200;
        constexpr auto     LcdOrientation = ST7735::Orientation::Landscape;
        constexpr uint16_t Width          = ST7735::LandscapeWidth;
        constexpr uint16_t Height         = ST7735::LandscapeHeight;

        constexpr uint32_t    LogPeriodMs    = 1000;
        constexpr uint32_t    TaskStackBytes = 8192;
        constexpr UBaseType_t TaskPriority   = 4;

        inline SPI::BusConfig makeBusConfig() {
            SPI::BusConfig config {};
            config.host                = SPI2_HOST;
            config.bus.mosi_io_num     = Pins[PinId::Mosi];
            config.bus.miso_io_num     = -1;
            config.bus.sclk_io_num     = Pins[PinId::Sck];
            config.bus.quadwp_io_num   = -1;
            config.bus.quadhd_io_num   = -1;
            config.bus.max_transfer_sz = DmaBufferBytes;
            config.bus.flags           = SPICOMMON_BUSFLAG_MASTER;
            config.bus.intr_flags      = 0;
            return config;
        }

        inline SPI::DeviceConfig makeDeviceConfig() {
            SPI::DeviceConfig config {};
            config.index                 = DeviceIndex;
            config.device.clock_speed_hz = SpiClockHz;
            config.device.mode           = 0;
            config.device.spics_io_num   = Pins[PinId::Cs];
            config.device.queue_size     = 4;
            return config;
        }

        inline ST7735::Config makeLcdConfig() {
            ST7735::Config config       = ST7735::makeConfig(LcdOrientation);
            config.dmaBufferBytes       = DmaBufferBytes;
            config.resetPin             = Pins[PinId::Reset];
            config.dcPin                = Pins[PinId::Dc];
            config.backlightPin         = Pins[PinId::Backlight];
            config.useResetPin          = true;
            config.useBacklightPin      = true;
            config.autoDmaBuffer        = true;
            config.useOrientationPreset = true;
            config.invertColors         = true;
            return config;
        }

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
            SPI                  spi {makeBusConfig()};
            ST7735               lcd {spi, DeviceIndex, makeLcdConfig()};
            uint8_t              frameBufferData[Width * Height * 2] {};
            ST7735::FrameBuffer  frameBuffer {
                .data   = frameBufferData,
                .width  = Width,
                .height = Height,
                .stride = Width * 2,
            };

            TaskHandle_t taskHandle   = nullptr;
            uint32_t     frame        = 0;
            uint32_t     lastLogMs    = 0;
            uint32_t     framesDrawn  = 0;
            uint32_t     lastFrameUs  = 0;
            uint32_t     totalFrameUs = 0;
            uint32_t     fps          = 0;
        };

        inline Context& context() {
            static Context app;
            return app;
        }

        inline uint16_t wave(uint32_t frame, uint16_t period, uint16_t amplitude) {
            const uint16_t phase = static_cast<uint16_t>(frame % period);
            const uint16_t half  = static_cast<uint16_t>(period / 2);
            if (phase < half) {
                return static_cast<uint16_t>(phase * amplitude / half);
            }

            return static_cast<uint16_t>((period - phase) * amplitude / half);
        }

        inline uint16_t wheel(uint8_t pos) {
            if (pos < 85) {
                return ST7735::rgb565(static_cast<uint8_t>(255 - pos * 3), static_cast<uint8_t>(pos * 3), 32);
            }
            if (pos < 170) {
                pos = static_cast<uint8_t>(pos - 85);
                return ST7735::rgb565(32, static_cast<uint8_t>(255 - pos * 3), static_cast<uint8_t>(pos * 3));
            }

            pos = static_cast<uint8_t>(pos - 170);
            return ST7735::rgb565(static_cast<uint8_t>(pos * 3), 32, static_cast<uint8_t>(255 - pos * 3));
        }

        template <size_t Depth, auto... Errors>
        inline void logError(Context& app, const char* op, const TracedErrorSet<Depth, Errors...>& error) {
            app.console.error("%s failed: %s::%s (%ld)%s%s",
                              op,
                              error.domain(),
                              error.name(),
                              static_cast<long>(error.numeric_code()),
                              error.has_message() ? ": " : "",
                              error.message());
            error.for_each_cause([&app](const ErrorFrame& cause) {
                app.console.error("  caused by %s::%s (%ld)%s%s",
                                  cause.domain,
                                  cause.name,
                                  static_cast<long>(cause.numericCode),
                                  cause.message != nullptr ? ": " : "",
                                  cause.message != nullptr ? cause.message : "");
            });
        }

        inline bool drawScene(Context& app) {
            const uint32_t startUs = micros();
            const uint32_t frame   = app.frame++;

            const auto clearResult = ST7735::clearFrame(app.frameBuffer, ST7735::rgb565(2, 4, 7));
            if (clearResult.is_err()) {
                logError(app, "ST7735 clearFrame", clearResult.error());
                return false;
            }

            for (uint8_t i = 0; i < 8; ++i) {
                const uint16_t y         = static_cast<uint16_t>(24 + i * 6);
                const uint16_t width     = static_cast<uint16_t>(20 + wave(frame + i * 9, 70, 120));
                const auto     barResult = ST7735::fillFrameRect(app.frameBuffer, 0, y, width, 5, wheel(static_cast<uint8_t>(frame * 3 + i * 24)));
                if (barResult.is_err()) {
                    logError(app, "ST7735 fillFrameRect/bar", barResult.error());
                    return false;
                }
            }

            const uint16_t boxX      = wave(frame, 64, Width - 18);
            const uint16_t boxY      = static_cast<uint16_t>(42 + wave(frame + 17, 50, 20));
            const auto     boxResult = ST7735::fillFrameRect(app.frameBuffer, boxX, boxY, 18, 18, wheel(static_cast<uint8_t>(frame * 5)));
            if (boxResult.is_err()) {
                logError(app, "ST7735 fillFrameRect/box", boxResult.error());
                return false;
            }

            char fpsText[16] {};
            std::snprintf(fpsText, sizeof(fpsText), "%lu FPS", static_cast<unsigned long>(app.fps));
            const auto fpsTextResult = ST7735::drawFrameText(app.frameBuffer, fpsText, ST7735::TextStyle {
                                                                                           .x          = 2,
                                                                                           .y          = 2,
                                                                                           .scale      = 2,
                                                                                           .color      = ST7735::YELLOW,
                                                                                           .background = ST7735::BLACK,
                                                                                           .wrap       = false,
                                                                                       });
            if (fpsTextResult.is_err()) {
                logError(app, "ST7735 drawFrameText/fps", fpsTextResult.error());
                return false;
            }

            const auto titleResult = ST7735::drawFrameText(app.frameBuffer, "ANIM + FONT", ST7735::TextStyle {
                                                                                               .x           = 2,
                                                                                               .y           = 68,
                                                                                               .scale       = 1,
                                                                                               .color       = ST7735::WHITE,
                                                                                               .transparent = true,
                                                                                               .wrap        = false,
                                                                                           });
            if (titleResult.is_err()) {
                logError(app, "ST7735 drawFrameText/title", titleResult.error());
                return false;
            }

            const auto drawResult = app.lcd.drawFrameBuffer(app.frameBuffer);
            if (drawResult.is_err()) {
                logError(app, "ST7735 drawFrameBuffer", drawResult.error());
                return false;
            }

            const uint32_t endUs = micros();
            app.lastFrameUs      = endUs - startUs;
            app.totalFrameUs += app.lastFrameUs;
            ++app.framesDrawn;
            return true;
        }

        inline void logStats(Context& app, uint32_t nowMs) {
            if (nowMs - app.lastLogMs < LogPeriodMs) {
                return;
            }

            const uint32_t elapsedMs = nowMs - app.lastLogMs;
            app.fps                  = elapsedMs > 0 ? app.framesDrawn * 1000U / elapsedMs : 0;
            const uint32_t avgUs     = app.framesDrawn > 0 ? app.totalFrameUs / app.framesDrawn : 0;

            app.console.info("ST7735 fps-test fps=%lu avg=%luus last=%luus frame=%lu dma=%u clock=%lu",
                             static_cast<unsigned long>(app.fps),
                             static_cast<unsigned long>(avgUs),
                             static_cast<unsigned long>(app.lastFrameUs),
                             static_cast<unsigned long>(app.frame),
                             static_cast<unsigned>(app.lcd.dmaBufferBytes()),
                             static_cast<unsigned long>(SpiClockHz));

            app.lastLogMs    = nowMs;
            app.framesDrawn  = 0;
            app.totalFrameUs = 0;
        }

        inline void taskEntry(void*) {
            Context& app = context();

            for (;;) {
                app.console.updateCommandResponse();

                if (!drawScene(app)) {
                    app.nodeInfo.updateNodeState(ERROR);
                    app.console.printState(app.nodeInfo.getNodeState());
                    vTaskDelay(pdMS_TO_TICKS(250));
                    continue;
                }

                logStats(app, millis());
            }
        }

        inline bool startTask() {
            Context& app = context();
            if (app.taskHandle != nullptr) {
                return true;
            }

            const BaseType_t ok = xTaskCreate(
                taskEntry,
                "st7735 fps text",
                TaskStackBytes,
                nullptr,
                TaskPriority,
                &app.taskHandle);

            if (ok != pdPASS) {
                app.taskHandle = nullptr;
                app.nodeInfo.updateNodeState(ERROR);
                app.console.error("failed to create ST7735 FPS text task");
                return false;
            }

            return true;
        }
    }

    inline void setup() {
        Detail::Context& app = Detail::context();

        app.console.setup();
        app.console.printBootBanner(app.nodeInfo);
        app.console.info("ST7735 FPS test pins: scl/sck=%d sda/mosi=%d cs=%d res=%d dc=%d bl=%d clock=%lu",
                         Detail::Pins[Detail::PinId::Sck],
                         Detail::Pins[Detail::PinId::Mosi],
                         Detail::Pins[Detail::PinId::Cs],
                         Detail::Pins[Detail::PinId::Reset],
                         Detail::Pins[Detail::PinId::Dc],
                         Detail::Pins[Detail::PinId::Backlight],
                         static_cast<unsigned long>(Detail::SpiClockHz));
        app.console.info("ST7735 FPS test: orientation=%s size=%ux%u dma=%u text=5x7",
                         ST7735::orientationName(Detail::LcdOrientation),
                         static_cast<unsigned>(Detail::Width),
                         static_cast<unsigned>(Detail::Height),
                         static_cast<unsigned>(Detail::DmaBufferBytes));

        const auto busResult = app.spi.setupBus();
        if (busResult.is_err()) {
            app.nodeInfo.updateNodeState(ERROR);
            Detail::logError(app, "SPI setupBus", busResult.error());
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }

        const auto deviceResult = app.spi.addDevice(Detail::makeDeviceConfig());
        if (deviceResult.is_err()) {
            app.nodeInfo.updateNodeState(ERROR);
            Detail::logError(app, "SPI addDevice", deviceResult.error());
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }

        const auto lcdResult = app.lcd.setup();
        if (lcdResult.is_err()) {
            app.nodeInfo.updateNodeState(ERROR);
            Detail::logError(app, "ST7735 setup", lcdResult.error());
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
