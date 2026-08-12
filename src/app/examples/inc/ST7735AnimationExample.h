#pragma once

#include "src/app/assets/boot-anim.h"
#include "src/cfg/AppConfig.h"
#include "src/dom/NodeInfo.h"
#include "src/dvc/inc/ST7735.h"
#include "src/dvc/inc/Serial.h"
#include "src/svc/inc/SerialConsoleService.h"

#include <Arduino.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef Serial
#undef Serial
#endif

namespace ST7735AnimationExample
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
        constexpr auto     LcdOrientation = ST7735::Orientation::Portrait;

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

        inline ST7735::AnimationView makeAnimation() {
            return ST7735::AnimationView {
                .frames           = LcdAnimation::Frames,
                .width            = LcdAnimation::Width,
                .height           = LcdAnimation::Height,
                .frameCount       = LcdAnimation::FrameCount,
                .frameStride      = LcdAnimation::FrameBytes,
                .frameDurationsMs = LcdAnimation::DurationsMs,
            };
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

            dvc::Serial             serial {AppConfig::Hardware::SerialBaudrate};
            SerialConsoleService    console {serial};
            SPI                     spi {makeBusConfig()};
            ST7735                  lcd {spi, DeviceIndex, makeLcdConfig()};
            ST7735::AnimationView   animation {makeAnimation()};
            ST7735::AnimationPlayer player {.fallbackFps = LcdAnimation::Fps};
            uint8_t                 frameBufferData[LcdAnimation::Width * LcdAnimation::Height * 2] {};
            ST7735::FrameBuffer     frameBuffer {
                .data   = frameBufferData,
                .width  = LcdAnimation::Width,
                .height = LcdAnimation::Height,
                .stride = LcdAnimation::Width * 2,
            };

            TaskHandle_t taskHandle   = nullptr;
            uint32_t     lastLogMs    = 0;
            uint32_t     framesDrawn  = 0;
            uint32_t     lastFrameUs  = 0;
            uint32_t     totalFrameUs = 0;
        };

        inline Context& context() {
            static Context app;
            return app;
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

        inline void waitForFrame(Context& app) {
            int64_t nowUs = esp_timer_get_time();
            if (app.player.nextFrameUs == 0) {
                app.player.nextFrameUs = nowUs;
            }

            const int64_t waitUs = app.player.nextFrameUs - nowUs;
            if (waitUs > 0) {
                if (waitUs > 2000) {
                    vTaskDelay(pdMS_TO_TICKS(static_cast<uint32_t>(waitUs / 1000)));
                }
                while (esp_timer_get_time() < app.player.nextFrameUs) {
                    taskYIELD();
                }
            }
        }

        inline void advanceFrame(Context& app, uint16_t frameIndex) {
            uint16_t durationMs = app.player.fallbackFps > 0 ? static_cast<uint16_t>(1000U / app.player.fallbackFps) : 1;
            if (app.animation.frameDurationsMs != nullptr) {
                durationMs = app.animation.frameDurationsMs[frameIndex];
            }
            if (durationMs == 0) {
                durationMs = 1;
            }

            const int64_t intervalUs = static_cast<int64_t>(durationMs) * 1000LL;
            app.player.frameIndex    = static_cast<uint16_t>((app.player.frameIndex + 1U) % app.animation.frameCount);
            app.player.nextFrameUs += intervalUs;

            const int64_t nowUs = esp_timer_get_time();
            if (app.player.nextFrameUs < nowUs - intervalUs) {
                app.player.nextFrameUs = nowUs + intervalUs;
            }
        }

        inline bool drawFrame(Context& app) {
            const uint32_t startUs = micros();

            waitForFrame(app);
            const uint16_t frameIndex = static_cast<uint16_t>(app.player.frameIndex % app.animation.frameCount);

            const auto copyResult = ST7735::copyAnimationFrame(app.frameBuffer, app.animation, frameIndex);
            if (copyResult.is_err()) {
                logError(app, "ST7735 copyAnimationFrame", copyResult.error());
                return false;
            }

            const auto textResult = ST7735::drawFrameText(app.frameBuffer, "ST7735 DMA", ST7735::TextStyle {
                                                                                             .x          = 2,
                                                                                             .y          = 2,
                                                                                             .scale      = 1,
                                                                                             .color      = ST7735::WHITE,
                                                                                             .background = ST7735::BLACK,
                                                                                             .wrap       = false,
                                                                                         });
            if (textResult.is_err()) {
                logError(app, "ST7735 drawFrameText", textResult.error());
                return false;
            }

            const auto drawResult = app.lcd.drawFrameBuffer(app.frameBuffer);
            if (drawResult.is_err()) {
                logError(app, "ST7735 drawFrameBuffer", drawResult.error());
                return false;
            }

            advanceFrame(app, frameIndex);

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
            const uint32_t fps       = elapsedMs > 0 ? app.framesDrawn * 1000U / elapsedMs : 0;
            const uint32_t avgUs     = app.framesDrawn > 0 ? app.totalFrameUs / app.framesDrawn : 0;

            app.console.info(
                "ST7735 GIF fps=%lu gifFps=%u avg=%luus last=%luus frame=%u/%u bytes/frame=%u dma=%u clock=%lu",
                static_cast<unsigned long>(fps),
                static_cast<unsigned>(LcdAnimation::Fps),
                static_cast<unsigned long>(avgUs),
                static_cast<unsigned long>(app.lastFrameUs),
                static_cast<unsigned>(app.player.frameIndex),
                static_cast<unsigned>(app.animation.frameCount),
                static_cast<unsigned>(app.animation.frameStride),
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

                if (!drawFrame(app)) {
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
                "st7735 animation",
                TaskStackBytes,
                nullptr,
                TaskPriority,
                &app.taskHandle);

            if (ok != pdPASS) {
                app.taskHandle = nullptr;
                app.nodeInfo.updateNodeState(ERROR);
                app.console.error("failed to create ST7735 animation task");
                return false;
            }

            return true;
        }
    }

    inline void setup() {
        Detail::Context& app = Detail::context();

        app.console.setup();
        app.console.printBootBanner(app.nodeInfo);
        app.console.info("ST7735 SPI pins: scl/sck=%d sda/mosi=%d cs=%d res=%d dc=%d bl=%d clock=%lu",
                         Detail::Pins[Detail::PinId::Sck],
                         Detail::Pins[Detail::PinId::Mosi],
                         Detail::Pins[Detail::PinId::Cs],
                         Detail::Pins[Detail::PinId::Reset],
                         Detail::Pins[Detail::PinId::Dc],
                         Detail::Pins[Detail::PinId::Backlight],
                         static_cast<unsigned long>(Detail::SpiClockHz));
        app.console.info("ST7735 GIF: orientation=%s size=%ux%u frames=%u fps=%u frameBytes=%u",
                         ST7735::orientationName(Detail::LcdOrientation),
                         static_cast<unsigned>(app.animation.width),
                         static_cast<unsigned>(app.animation.height),
                         static_cast<unsigned>(app.animation.frameCount),
                         static_cast<unsigned>(LcdAnimation::Fps),
                         static_cast<unsigned>(app.animation.frameStride));

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
