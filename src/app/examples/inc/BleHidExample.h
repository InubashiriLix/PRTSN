#pragma once

#include "src/alg/inc/alg_matrix.h"
#include "src/cfg/AppConfig.h"
#include "src/dom/NodeInfo.h"
#include "src/dvc/inc/BufferedMouseScanner.h"
#include "src/dvc/inc/NkroKeyboardScanner.h"
#include "src/svc/inc/BleHidNkroKeyboardMouse.h"
#include "src/svc/inc/SerialConsoleService.h"

namespace BleHidExample
{
    namespace Detail
    {
        enum class PinId : uint8_t
        {
            Row0,
            Row1,
            Row2,
            Row3,
            Row4,
            Col0,
            Col1,
            Col2,
            Col3,
        };

        inline constexpr auto Pins = ::prtn::pin::layout(
            ::prtn::pin::bind(PinId::Row0, GPIO_NUM_39, ::prtn::pin::Role::InputPulldown),
            ::prtn::pin::bind(PinId::Row1, GPIO_NUM_38, ::prtn::pin::Role::InputPulldown),
            ::prtn::pin::bind(PinId::Row2, GPIO_NUM_21, ::prtn::pin::Role::InputPulldown),
            ::prtn::pin::bind(PinId::Row3, GPIO_NUM_18, ::prtn::pin::Role::InputPulldown),
            ::prtn::pin::bind(PinId::Row4, GPIO_NUM_17, ::prtn::pin::Role::InputPulldown),
            ::prtn::pin::bind(PinId::Col0, GPIO_NUM_2, ::prtn::pin::Role::Output),
            ::prtn::pin::bind(PinId::Col1, GPIO_NUM_42, ::prtn::pin::Role::Output),
            ::prtn::pin::bind(PinId::Col2, GPIO_NUM_41, ::prtn::pin::Role::Output),
            ::prtn::pin::bind(PinId::Col3, GPIO_NUM_40, ::prtn::pin::Role::Output));

        constexpr size_t      RowCount       = 5;
        constexpr size_t      ColCount       = 4;
        constexpr uint32_t    TaskStackBytes = 4096;
        constexpr UBaseType_t TaskPriority   = 4;

        using Scanner = NkroKeyboardScanner<RowCount, ColCount>;

        constexpr prt_hid::KeyId DefaultKeyMap[RowCount][ColCount] = {
            {prt_hid::KeyId::Escape, prt_hid::KeyId::KeypadDivide, prt_hid::KeyId::KeypadMultiply, prt_hid::KeyId::KeypadMinus},
            {prt_hid::KeyId::Keypad7, prt_hid::KeyId::Keypad8, prt_hid::KeyId::Keypad9, prt_hid::KeyId::KeypadPlus},
            {prt_hid::KeyId::Keypad4, prt_hid::KeyId::Keypad5, prt_hid::KeyId::Keypad6, prt_hid::KeyId::KeypadPlus},
            {prt_hid::KeyId::Keypad1, prt_hid::KeyId::Keypad2, prt_hid::KeyId::Keypad3, prt_hid::KeyId::KeypadEnter},
            {prt_hid::KeyId::Keypad0, prt_hid::KeyId::Keypad0, prt_hid::KeyId::Backspace, prt_hid::KeyId::KeypadEnter},
        };

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

            Matrix<RowCount, ColCount, uint32_t>       keyStateMatrix {};
            Matrix<RowCount, ColCount, prt_hid::KeyId> keyIdMapMatrix {
                &DefaultKeyMap[0][0],
                RowCount* ColCount,
            };
            Matrix<RowCount, ColCount, prt_hid::KeyId> longKeyIdMapMatrix {};

            Scanner::Config scannerConfig {
                .scanIntervalMs     = 1,
                .debounceMs         = 20,
                .longPressMs        = 600,
                .activeLevel        = StdPinLevel::High,
                .rowPins            = {Pins[PinId::Row0], Pins[PinId::Row1], Pins[PinId::Row2], Pins[PinId::Row3], Pins[PinId::Row4]},
                .rowPinMode         = StdPinFunc::InputPulldown,
                .colPins            = {Pins[PinId::Col0], Pins[PinId::Col1], Pins[PinId::Col2], Pins[PinId::Col3]},
                .colPinMode         = StdPinFunc::Output,
                .KeyStateMatrix     = keyStateMatrix,
                .KeyIdMapMatrix     = keyIdMapMatrix,
                .LongKeyIdMapMatrix = longKeyIdMapMatrix,
            };

            Scanner                              scanner {scannerConfig};
            BufferedMouseScanner                 mouseScanner {};
            prt_ble_hid::BleHidNkroKeyboardMouse hid {"PRTN_NKRO_MOUSE"};
            TaskHandle_t                         taskHandle = nullptr;

            Context() {
                longKeyIdMapMatrix.data[0][0] = prt_hid::KeyId::F11;
            }
        };

        inline Context& context() {
            static Context app;
            return app;
        }

        inline void taskEntry(void*) {
            Context& app = context();

            for (;;) {
                const auto scanResult = app.scanner.scan();
                if (scanResult.is_err()) {
                    app.nodeInfo.updateNodeState(ERROR);
                    app.console.errorResult("BLE keyboard scan", scanResult.error());
                    vTaskDelay(pdMS_TO_TICKS(250));
                    continue;
                }

                const auto mouseScanResult = app.mouseScanner.scan();
                if (mouseScanResult.is_err()) {
                    app.nodeInfo.updateNodeState(ERROR);
                    app.console.errorResult("BLE mouse scan", mouseScanResult.error());
                    vTaskDelay(pdMS_TO_TICKS(250));
                    continue;
                }

                if (!app.hid.isReady()) {
                    continue;
                }

                const auto mouseUpdateResult = app.hid.updateMouseState(mouseScanResult.unwrap());
                if (mouseUpdateResult.is_err() &&
                    !mouseUpdateResult.error().is<prt_ble_hid::BleHidNkroKeyboardMouse::Detail::NotReady>()) {
                    app.nodeInfo.updateNodeState(ERROR);
                    app.console.errorResult("BLE mouse update", mouseUpdateResult.error());
                    vTaskDelay(pdMS_TO_TICKS(250));
                    continue;
                }

                const auto updateResult = app.hid.updateKeyboardState(scanResult.unwrap());
                if (updateResult.is_err()) {
                    if (updateResult.error().is<prt_ble_hid::BleHidNkroKeyboardMouse::Detail::NotReady>()) {
                        continue;
                    }
                    app.nodeInfo.updateNodeState(ERROR);
                    app.console.errorResult("BLE keyboard update", updateResult.error());
                    vTaskDelay(pdMS_TO_TICKS(250));
                }
            }
        }

        inline bool startTask() {
            Context& app = context();
            if (app.taskHandle != nullptr) {
                return true;
            }

            const BaseType_t ok = xTaskCreate(
                taskEntry,
                "BLE NKRO keyboard",
                TaskStackBytes,
                nullptr,
                TaskPriority,
                &app.taskHandle);

            if (ok != pdPASS) {
                app.taskHandle = nullptr;
                app.nodeInfo.updateNodeState(ERROR);
                app.console.error("failed to create BLE keyboard task");
                return false;
            }

            return true;
        }
    }

    inline void setup() {
        Detail::Context& app = Detail::context();

        app.console.setup();
        app.console.printBootBanner(app.nodeInfo);

        const auto scannerSetupResult = app.scanner.setup();
        if (scannerSetupResult.is_err()) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.errorResult("BLE keyboard scanner setup", scannerSetupResult.error());
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }

        const auto mouseSetupResult = app.mouseScanner.setup();
        if (mouseSetupResult.is_err()) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.errorResult("BLE mouse scanner setup", mouseSetupResult.error());
            app.scanner.end();
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }

        const auto hidSetupResult = app.hid.setup();
        if (hidSetupResult.is_err()) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.errorResult("BLE HID setup", hidSetupResult.error());
            app.mouseScanner.end();
            app.scanner.end();
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }

        if (!Detail::startTask()) {
            app.mouseScanner.end();
            app.scanner.end();
            app.console.printState(app.nodeInfo.getNodeState());
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
