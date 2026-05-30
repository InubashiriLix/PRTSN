#pragma once

#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "src/cfg/AppConfig.h"
#include "src/dom/NodeInfo.h"
#include "src/dvc/inc/Serial.h"
#include "src/svc/inc/SerialConsoleService.h"
#include "src/dvc/inc/SSD1306.h"

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdio>

#ifdef Serial
#undef Serial
#endif

namespace SSD1306AnimationApp
{
    namespace Detail
    {
        constexpr TickType_t  TaskPeriodTicks      = pdMS_TO_TICKS(10);
        constexpr uint32_t    TaskStackWords       = 4096;
        constexpr UBaseType_t TaskPriority         = 4;
        constexpr uint8_t     BoxSize              = 12;
        constexpr uint8_t     AnimLeft             = 1;
        constexpr uint8_t     AnimTop              = 16;
        constexpr uint8_t     AnimRight            = SSD1306::Width - 2;
        constexpr uint8_t     AnimBottom           = 55;
        constexpr uint8_t     FpsTextWidth         = 64;
        constexpr uint8_t     ScrollPage           = 7;
        constexpr uint8_t     DirtyPadding         = 1;
        constexpr bool        EnableHardwareScroll = false;

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

            IIC     iic {IIC::DefaultSdaPin, IIC::DefaultSclPin, IIC::DefaultFrequency};
            SSD1306 oled {iic, SSD1306::DefaultAddress};

            TaskHandle_t taskHandle = nullptr;
            uint32_t     frameCount = 0;
            uint32_t     lastFpsMs  = 0;
            uint32_t     fps        = 0;
            int16_t      boxX       = AnimLeft;
            int16_t      boxY       = 22;
            int8_t       boxDx      = 2;
            int8_t       boxDy      = 1;
        };

        inline Context& context() {
            static Context app;
            return app;
        }

        inline void drawRect(SSD1306& oled, uint8_t x, uint8_t y, uint8_t width, uint8_t height, SSD1306::Color color) {
            const uint16_t xEnd = static_cast<uint16_t>(x + width);
            const uint16_t yEnd = static_cast<uint16_t>(y + height);

            for (uint16_t px = x; px < xEnd && px < SSD1306::Width; ++px) {
                oled.setPixel(static_cast<uint8_t>(px), y, color);
                if (height > 1) {
                    oled.setPixel(static_cast<uint8_t>(px), static_cast<uint8_t>(y + height - 1), color);
                }
            }

            for (uint16_t py = y; py < yEnd && py < SSD1306::Height; ++py) {
                oled.setPixel(x, static_cast<uint8_t>(py), color);
                if (width > 1) {
                    oled.setPixel(static_cast<uint8_t>(x + width - 1), static_cast<uint8_t>(py), color);
                }
            }
        }

        inline void drawFilledRect(SSD1306& oled, uint8_t x, uint8_t y, uint8_t width, uint8_t height, SSD1306::Color color) {
            const uint16_t xEnd = static_cast<uint16_t>(x + width);
            const uint16_t yEnd = static_cast<uint16_t>(y + height);

            for (uint16_t py = y; py < yEnd && py < SSD1306::Height; ++py) {
                for (uint16_t px = x; px < xEnd && px < SSD1306::Width; ++px) {
                    oled.setPixel(static_cast<uint8_t>(px), static_cast<uint8_t>(py), color);
                }
            }
        }

        inline void drawFps(Context& app) {
            char fpsText[16] {};
            std::snprintf(fpsText, sizeof(fpsText), "FPS %lu", static_cast<unsigned long>(app.fps));

            drawFilledRect(app.oled, 0, 0, FpsTextWidth, 8, SSD1306::Color::BLACK);
            app.oled.drawText(fpsText, 0, 0);
        }

        inline bool drawInitialFrame(Context& app) {
            app.oled.clear(false);
            drawFps(app);
            drawRect(app.oled,
                     0,
                     static_cast<uint8_t>(AnimTop - 1),
                     SSD1306::Width,
                     static_cast<uint8_t>(AnimBottom - AnimTop + 2),
                     SSD1306::Color::WHITE);
            drawFilledRect(app.oled,
                           static_cast<uint8_t>(app.boxX),
                           static_cast<uint8_t>(app.boxY),
                           BoxSize,
                           BoxSize,
                           SSD1306::Color::WHITE);
            if (EnableHardwareScroll) {
                app.oled.drawText("  HW SCROLL  HW SCROLL  ", SSD1306::TextStyle {0, 56, 1, SSD1306::Color::WHITE});
            }

            if (!app.oled.display()) {
                return false;
            }

            return !EnableHardwareScroll ||
                   app.oled.startHorizontalScroll(
                       SSD1306::ScrollDirection::LEFT,
                       ScrollPage,
                       ScrollPage,
                       SSD1306::ScrollInterval::FRAME_3);
        }

        inline uint8_t rectUnionStart(uint8_t a, uint8_t b) {
            return a < b ? a : b;
        }

        inline uint8_t rectUnionLength(uint8_t start, uint8_t a, uint8_t b, uint8_t size) {
            const uint8_t aEnd = static_cast<uint8_t>(a + size - 1);
            const uint8_t bEnd = static_cast<uint8_t>(b + size - 1);
            const uint8_t end  = aEnd > bEnd ? aEnd : bEnd;
            return static_cast<uint8_t>(end - start + 1);
        }

        inline void drawAnimationFrame(Context& app) {
            drawRect(app.oled,
                     0,
                     static_cast<uint8_t>(AnimTop - 1),
                     SSD1306::Width,
                     static_cast<uint8_t>(AnimBottom - AnimTop + 2),
                     SSD1306::Color::WHITE);
        }

        inline void updateAnimation(Context& app) {
            const int16_t oldX = app.boxX;
            const int16_t oldY = app.boxY;

            app.boxX = static_cast<int16_t>(app.boxX + app.boxDx);
            app.boxY = static_cast<int16_t>(app.boxY + app.boxDy);

            if (app.boxX <= AnimLeft || app.boxX + BoxSize > AnimRight) {
                app.boxDx = static_cast<int8_t>(-app.boxDx);
                app.boxX  = app.boxX <= AnimLeft ? AnimLeft : static_cast<int16_t>(AnimRight - BoxSize + 1);
            }

            if (app.boxY <= AnimTop || app.boxY + BoxSize >= AnimBottom) {
                app.boxDy = static_cast<int8_t>(-app.boxDy);
                app.boxY  = app.boxY <= AnimTop ? AnimTop : static_cast<int16_t>(AnimBottom - BoxSize);
            }

            uint8_t dirtyX      = rectUnionStart(static_cast<uint8_t>(oldX), static_cast<uint8_t>(app.boxX));
            uint8_t dirtyY      = rectUnionStart(static_cast<uint8_t>(oldY), static_cast<uint8_t>(app.boxY));
            uint8_t dirtyWidth  = rectUnionLength(dirtyX, static_cast<uint8_t>(oldX), static_cast<uint8_t>(app.boxX), BoxSize);
            uint8_t dirtyHeight = rectUnionLength(dirtyY, static_cast<uint8_t>(oldY), static_cast<uint8_t>(app.boxY), BoxSize);

            const uint8_t dirtyRight  = dirtyX + dirtyWidth - 1;
            const uint8_t dirtyBottom = dirtyY + dirtyHeight - 1;
            dirtyX                    = dirtyX > DirtyPadding ? static_cast<uint8_t>(dirtyX - DirtyPadding) : 0;
            dirtyY                    = dirtyY > AnimTop ? static_cast<uint8_t>(dirtyY - DirtyPadding) : static_cast<uint8_t>(AnimTop - 1);
            dirtyWidth                = static_cast<uint8_t>((dirtyRight + DirtyPadding >= SSD1306::Width ? SSD1306::Width - 1 : dirtyRight + DirtyPadding) - dirtyX + 1);
            dirtyHeight               = static_cast<uint8_t>((dirtyBottom + DirtyPadding >= AnimBottom ? AnimBottom : dirtyBottom + DirtyPadding) - dirtyY + 1);

            const uint8_t pageTop    = static_cast<uint8_t>((dirtyY / 8) * 8);
            const uint8_t pageBottom = static_cast<uint8_t>(((dirtyY + dirtyHeight - 1) / 8) * 8 + 7);
            dirtyY                   = pageTop;
            dirtyHeight              = static_cast<uint8_t>((pageBottom >= SSD1306::Height ? SSD1306::Height - 1 : pageBottom) - dirtyY + 1);

            drawFilledRect(app.oled, dirtyX, dirtyY, dirtyWidth, dirtyHeight, SSD1306::Color::BLACK);
            drawAnimationFrame(app);
            drawFilledRect(app.oled,
                           static_cast<uint8_t>(app.boxX),
                           static_cast<uint8_t>(app.boxY),
                           BoxSize,
                           BoxSize,
                           SSD1306::Color::WHITE);

            if (!app.oled.displayRegion(dirtyX, dirtyY, dirtyWidth, dirtyHeight)) {
                app.console.error("failed to refresh OLED display");
                return;
            }

            ++app.frameCount;
            const uint32_t nowMs = millis();
            if (nowMs - app.lastFpsMs >= 1000) {
                app.fps        = app.frameCount;
                app.frameCount = 0;
                app.lastFpsMs  = nowMs;

                drawFps(app);
                if (!app.oled.displayRegion(0, 0, FpsTextWidth, 8)) {
                    app.console.error("failed to refresh OLED FPS");
                }
            }
        }

        inline void taskEntry(void*) {
            Context&   app              = context();
            TickType_t taskLastWakeTime = xTaskGetTickCount();

            for (;;) {
                app.console.updateCommandResponse();
                updateAnimation(app);
                vTaskDelayUntil(&taskLastWakeTime, TaskPeriodTicks);
            }
        }

        inline bool startTask() {
            Context& app = context();
            if (app.taskHandle != nullptr) {
                return true;
            }

            const BaseType_t ok = xTaskCreate(
                taskEntry,
                "SDD1306_example_task",
                TaskStackWords,
                nullptr,
                TaskPriority,
                &app.taskHandle);

            if (ok != pdPASS) {
                app.taskHandle = nullptr;
                app.nodeInfo.updateNodeState(ERROR);
                app.console.error("failed to createa sdd1306 test task");
                return false;
            }

            return true;
        }
    }

    inline void setup() {
        Detail::Context& app = Detail::context();

        app.console.setup();
        app.console.printBootBanner(app.nodeInfo);
        // Put complete app-level experiment setup here.

        if (!app.iic.setup()) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.err("Failed to initialize IIC bus");
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }
        if (!app.oled.setup()) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.err("Failed to initialize OLED display");
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }
        if (!Detail::drawInitialFrame(app)) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.err("Failed to draw initial OLED frame");
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }

        // end

        app.lastFpsMs = millis();
        app.nodeInfo.updateNodeState(RUNNING);
        app.console.printState(app.nodeInfo.getNodeState());

        Detail::startTask();
    }

    inline void idle() {
        Detail::context().console.updateCommandResponse();

        // Put complete app-level experiment loop code here.

        delay(AppConfig::Runtime::AppLoopIntervalMs);
    }
}
