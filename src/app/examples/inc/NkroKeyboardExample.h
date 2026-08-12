#pragma once

#include "src/alg/inc/alg_matrix.h"
#include "src/cfg/AppConfig.h"
#include "src/dom/NodeInfo.h"
#include "src/dvc/inc/NkroKeyboardScanner.h"
#include "src/dvc/inc/Serial.h"
#include "src/svc/inc/NkroKeyboard.h"
#include "src/svc/inc/NkroKeyboardService.h"
#include "src/svc/inc/SerialConsoleService.h"

#include <Arduino.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef Serial
#undef Serial
#endif

namespace NkroKeyboardExample
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
            ::prtn::pin::bind(PinId::Row2, GPIO_NUM_37, ::prtn::pin::Role::InputPulldown),
            ::prtn::pin::bind(PinId::Row3, GPIO_NUM_36, ::prtn::pin::Role::InputPulldown),
            ::prtn::pin::bind(PinId::Row4, GPIO_NUM_35, ::prtn::pin::Role::InputPulldown),
            ::prtn::pin::bind(PinId::Col0, GPIO_NUM_2, ::prtn::pin::Role::Output),
            ::prtn::pin::bind(PinId::Col1, GPIO_NUM_42, ::prtn::pin::Role::Output),
            ::prtn::pin::bind(PinId::Col2, GPIO_NUM_41, ::prtn::pin::Role::Output),
            ::prtn::pin::bind(PinId::Col3, GPIO_NUM_40, ::prtn::pin::Role::Output));

        constexpr size_t      RowCount       = 5;
        constexpr size_t      ColCount       = 4;
        constexpr uint32_t    TaskStackBytes = 4096;
        constexpr UBaseType_t TaskPriority   = 4;

        using Scanner = NkroKeyboardScanner<RowCount, ColCount>;
        using Service = NkroKeyboardService<RowCount, ColCount>;

        // Physical matrix in row-major order: keyMap[row][col].
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
                RowCount * ColCount,
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

            Scanner      scanner {scannerConfig};
            NkroKeyboard keyboard {};

            Service::Config serviceConfig {
                .scanDevice         = scanner,
                .keyboard           = keyboard,
                .longPressMs        = scannerConfig.longPressMs,
                .KeyStateMatrix     = keyStateMatrix,
                .KeyIdMapMatrix     = keyIdMapMatrix,
                .LongKeyIdMapMatrix = longKeyIdMapMatrix,
            };
            Service      service {serviceConfig};
            TaskHandle_t taskHandle = nullptr;

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
#if PRTN_ENABLE_NKRO_DEBUG_LOG
            uint32_t lastHeartbeatMs = 0;

            app.console.info("NKRO scan task entered");
#endif

            for (;;) {
                const auto result       = app.service.update();
                const bool updateFailed = result.is_err();
                if (result.is_err()) {
                    app.nodeInfo.updateNodeState(ERROR);
                    app.console.error("NKRO keyboard update failed: %s", toName(result.unwrap_err()));
                }

#if PRTN_ENABLE_NKRO_DEBUG_LOG
                const auto snapshotResult = app.scanner.snapshot();
                if (snapshotResult.is_err()) {
                    app.console.error("NKRO snapshot failed: %s", toName(snapshotResult.unwrap_err()));
                    vTaskDelay(pdMS_TO_TICKS(250));
                    continue;
                }

                const KeyboardScanFrame frame = snapshotResult.unwrap();
                if (frame.changed) {
                    size_t pressedCount = 0;
                    for (size_t slot = 0; slot < frame.slotCount; ++slot) {
                        if ((frame.pressedBitmap[slot >> 3] & static_cast<uint8_t>(1u << (slot & 0x07))) == 0) {
                            continue;
                        }

                        ++pressedCount;
                        const size_t row = slot / ColCount;
                        const size_t col = slot % ColCount;
                        app.console.info("key active: slot=%u row=%u col=%u duration_ms=%lu short=0x%02X long=0x%02X",
                                         static_cast<unsigned>(slot),
                                         static_cast<unsigned>(row),
                                         static_cast<unsigned>(col),
                                         static_cast<unsigned long>(app.keyStateMatrix.data[row][col]),
                                         static_cast<unsigned>(app.keyIdMapMatrix.data[row][col]),
                                         static_cast<unsigned>(app.longKeyIdMapMatrix.data[row][col]));
                    }
                    app.console.info("scan state changed: pressed=%u timestamp_ms=%lu",
                                     static_cast<unsigned>(pressedCount),
                                     static_cast<unsigned long>(frame.timestemp));
                }

                if (frame.timestemp - lastHeartbeatMs >= 2000) {
                    const auto hidReady = app.keyboard.ready();
                    app.console.info("NKRO heartbeat: scan_alive=yes hid_ready=%s timestamp_ms=%lu",
                                     hidReady.is_ok() && hidReady.unwrap() ? "yes" : "no",
                                     static_cast<unsigned long>(frame.timestemp));
                    lastHeartbeatMs = frame.timestemp;
                }
#endif

                if (updateFailed) {
                    vTaskDelay(pdMS_TO_TICKS(250));
                }
            }
        }
    }

    inline void setup() {
        Detail::Context& app = Detail::context();

        app.console.setup();
        app.console.printBootBanner(app.nodeInfo);
#if PRTN_ENABLE_NKRO_DEBUG_LOG
        app.console.info("NKRO example ready: rows=%u cols=%u slots=%u debounce_ms=%lu long_press_ms=%lu",
                         static_cast<unsigned>(Detail::RowCount),
                         static_cast<unsigned>(Detail::ColCount),
                         static_cast<unsigned>(Detail::RowCount * Detail::ColCount),
                         static_cast<unsigned long>(app.scannerConfig.debounceMs),
                         static_cast<unsigned long>(app.scannerConfig.longPressMs));
        app.console.info("starting keyboard service: scanner setup -> HID setup -> USB begin");
#endif

        const auto setupResult = app.service.setup();
        if (setupResult.is_err()) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.error("failed to setup NKRO keyboard: %s", toName(setupResult.unwrap_err()));
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }
#if PRTN_ENABLE_NKRO_DEBUG_LOG
        app.console.info("keyboard service setup complete");
        app.console.info("creating NKRO scan task");
#endif

        const BaseType_t ok = xTaskCreate(
            Detail::taskEntry,
            "nkro keyboard",
            Detail::TaskStackBytes,
            nullptr,
            Detail::TaskPriority,
            &app.taskHandle);

        if (ok != pdPASS) {
            app.service.end();
            app.nodeInfo.updateNodeState(ERROR);
            app.console.error("failed to create NKRO keyboard task");
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }
#if PRTN_ENABLE_NKRO_DEBUG_LOG
        app.console.info("NKRO scan task created");
#endif

        app.nodeInfo.updateNodeState(RUNNING);
        app.console.printState(app.nodeInfo.getNodeState());
    }

    inline void idle() {
        Detail::context().console.updateCommandResponse();
        delay(AppConfig::Runtime::AppLoopIntervalMs);
    }
}
