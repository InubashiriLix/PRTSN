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

namespace SSD1306OledDisplayApp
{
    namespace Detail
    {
        enum class PinId : uint8_t
        {
            I2cSda,
            I2cScl,
        };

        inline constexpr auto Pins = ::prtn::pin::layout(
            ::prtn::pin::bind(PinId::I2cSda, GPIO_NUM_4, ::prtn::pin::Role::I2cSda),
            ::prtn::pin::bind(PinId::I2cScl, GPIO_NUM_5, ::prtn::pin::Role::I2cScl));

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
            IIC                  iic {Pins[PinId::I2cSda], Pins[PinId::I2cScl]};
            SSD1306              oled {iic};
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

        if (!app.iic.setup()) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.error("failed to initialize IIC bus");
            return;
        }

        if (!app.oled.setup()) {
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
