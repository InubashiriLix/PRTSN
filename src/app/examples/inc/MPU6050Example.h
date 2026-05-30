#pragma once

#include "src/cfg/AppConfig.h"
#include "src/dom/NodeInfo.h"
#include "src/dvc/inc/I2C_MPU6050.h"
#include "src/dvc/inc/Serial.h"
#include "src/svc/inc/SerialConsoleService.h"

#include <Arduino.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef Serial
#undef Serial
#endif

namespace MPU6050Example
{
    namespace Detail
    {
        constexpr uint8_t     SdaPin         = IIC::DefaultSdaPin;
        constexpr uint8_t     SclPin         = IIC::DefaultSclPin;
        constexpr uint32_t    IicFrequency   = IIC::DefaultFrequency;
        constexpr uint8_t     MpuAddress     = I2C_MPU6050::MPU6050_ADDR_LOW;
        constexpr TickType_t  TaskPeriodMs   = 50;
        constexpr TickType_t  TaskPeriod     = pdMS_TO_TICKS(TaskPeriodMs);
        constexpr uint32_t    LogIntervalMs  = 500;
        constexpr uint32_t    TaskStackWords = 4096;
        constexpr UBaseType_t TaskPriority   = 4;

        const I2C_MPU6050::Config MpuCfg {
            .accelRange    = I2C_MPU6050::AccelRange::G2,
            .gyroRange     = I2C_MPU6050::GyroRange::DPS250,
            .dlpfCfg       = 3,
            .sampleRateDiv = 9,
            .dataReadyInt  = false,
            .filter {
                .emaCutoffHz   = 2.0f,
                .attitudeAlpha = 0.98f,
                .calibSamples  = 40,
                .calibDelayMs  = 20,
            },
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
            IIC                  iic {SdaPin, SclPin, IicFrequency};
            I2C_MPU6050          mpu {MpuAddress, iic, MpuCfg};

            TaskHandle_t taskHandle = nullptr;
            uint32_t     lastLogMs  = 0;
        };

        inline Context& context() {
            static Context app;
            return app;
        }

        inline void logError(Context& app, const char* op, I2C_MPU6050::Error err) {
            app.console.error(
                "MPU6050 %s failed: code=%s detail=%s iicDetail=%s native=%ld addr=0x%02X bytes=%u",
                op,
                toName(err.code),
                I2C_MPU6050::detailName(err.detail),
                IIC::detailName(err.iic.detail),
                static_cast<long>(err.iic.native),
                static_cast<unsigned>(err.iic.address),
                static_cast<unsigned>(err.iic.bytes));
        }

        inline void logSample(Context& app) {
            const auto accel = app.mpu.getFilteredAccel();
            const auto gyro  = app.mpu.getFilteredGyro();
            const auto temp  = app.mpu.getTemp();
            const auto ori   = app.mpu.getOrientation();

            if (!accel.error || !gyro.error || !temp.error) {
                return;
            }

            app.console.info(
                "MPU6050 tick=%lu accel[g]=%.3f %.3f %.3f gyro[dps]=%.2f %.2f %.2f pitch=%.1f roll=%.1f yaw=%.1f temp=%.2fC",
                static_cast<unsigned long>(app.mpu.lastUpdate()),
                static_cast<double>(accel.vec.vector.x),
                static_cast<double>(accel.vec.vector.y),
                static_cast<double>(accel.vec.vector.z),
                static_cast<double>(gyro.vec.vector.x),
                static_cast<double>(gyro.vec.vector.y),
                static_cast<double>(gyro.vec.vector.z),
                static_cast<double>(ori.pitch),
                static_cast<double>(ori.roll),
                static_cast<double>(ori.yaw),
                static_cast<double>(temp.value));
        }

        inline void taskEntry(void*) {
            Context&   app              = context();
            TickType_t taskLastWakeTime = xTaskGetTickCount();

            for (;;) {
                app.console.updateCommandResponse();

                const auto err   = app.mpu.readRawData();
                const auto nowMs = millis();
                if (nowMs - app.lastLogMs >= LogIntervalMs) {
                    app.lastLogMs = nowMs;

                    if (!err) {
                        logError(app, "readRawData", err);
                    }
                    else if (app.mpu.calibrationState() == I2C_MPU6050::CalibrationState::DONE) {
                        logSample(app);
                    }
                }

                vTaskDelayUntil(&taskLastWakeTime, TaskPeriod);
            }
        }

        inline bool startTask() {
            Context& app = context();
            if (app.taskHandle != nullptr) {
                return true;
            }

            const BaseType_t ok = xTaskCreate(
                taskEntry,
                "mpu6050 example",
                TaskStackWords,
                nullptr,
                TaskPriority,
                &app.taskHandle);

            if (ok != pdPASS) {
                app.taskHandle = nullptr;
                app.nodeInfo.updateNodeState(ERROR);
                app.console.error("failed to create MPU6050 example task");
                return false;
            }

            return true;
        }
    }

    inline void setup() {
        Detail::Context& app = Detail::context();

        app.console.setup();
        app.console.printBootBanner(app.nodeInfo);
        app.console.info("MPU6050 IIC: sda=%u scl=%u freq=%lu addr=0x%02X",
                         static_cast<unsigned>(Detail::SdaPin),
                         static_cast<unsigned>(Detail::SclPin),
                         static_cast<unsigned long>(Detail::IicFrequency),
                         static_cast<unsigned>(Detail::MpuAddress));

        const IIC::Error iicErr = app.iic.setup();
        if (!iicErr) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.error("failed to setup IIC: detail=%s native=%ld",
                              IIC::detailName(iicErr.detail),
                              static_cast<long>(iicErr.native));
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }

        const auto mpuErr = app.mpu.setup();
        if (!mpuErr) {
            app.nodeInfo.updateNodeState(ERROR);
            Detail::logError(app, "setup", mpuErr);
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }

        app.console.info("starting gyro calibration...");
        const auto calibErr = app.mpu.calibrateGyro();
        if (!calibErr) {
            app.nodeInfo.updateNodeState(ERROR);
            app.console.error("gyro calibration failed: %s state=%s",
                              I2C_MPU6050::detailName(calibErr.detail),
                              I2C_MPU6050::calibrationStateName(app.mpu.calibrationState()));
            app.console.printState(app.nodeInfo.getNodeState());
            return;
        }
        app.console.info("gyro calibrated: state=%s", I2C_MPU6050::calibrationStateName(app.mpu.calibrationState()));

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
