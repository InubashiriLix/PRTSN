#pragma once

#include "src/cfg/AppConfig.h"
#include "src/dom/NodeInfo.h"
#include "src/dvc/inc/SSD1306.h"
#include "src/dvc/inc/Serial.h"
#include "src/svc/inc/SerialConsoleService.h"

#include <Arduino.h>

#ifdef Serial
#undef Serial
#endif

namespace OledDisplayApp
{
    namespace Detail
    {
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
            SSD1306              oled {IIC::DefaultSdaPin, IIC::DefaultSclPin};
        };

        inline Context& context() {
            static Context app;
            return app;
        }
    }

    inline void setup() {
        Detail::Context& app = Detail::context();

        app.console.setup();
        app.console.printBootBanner(app.nodeInfo);

        if (!app.oled.begin()) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.error("failed to initialize OLED display");
            return;
        }

        if (!app.oled.displayText("1234567890123", SSD1306::TextStyle {
                                                      0,
                                                      0,
                                                      2,
                                                      SSD1306::Color::WHITE,
                                                      SSD1306::Color::BLACK,
                                                      false,
                                                  }) ||
            !app.oled.displayText("PRTNS", SSD1306::TextStyle {
                                               0,
                                               16,
                                               3,
                                               SSD1306::Color::WHITE,
                                               SSD1306::Color::BLACK,
                                               true,
                                           })) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.error("failed to update OLED display");
            return;
        }

        app.nodeInfo.updateNodeState(RUNNING);
        app.console.printState(app.nodeInfo.getNodeState());
    }

    inline void idle() {
        Detail::context().console.updateCommandResponse();
        delay(AppConfig::Runtime::AppLoopIntervalMs);
    }
}
