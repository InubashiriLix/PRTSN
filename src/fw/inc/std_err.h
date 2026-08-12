#pragma once

#include <cstddef>
#include <cstdint>

#include <esp_err.h>

#include "src/fw/inc/Result.h"

/**
 * @brief `esp_err_t` 常见值的类型安全镜像。Type-safe mirror of common ESP-IDF errors.
 *
 * 数值与 ESP-IDF 保持一致，因此可以由 `NativeErr()` 精确保留在 ErrorSet 中。
 * `OK` 仅表示原生成功值，不应列入业务错误集合。
 */
enum class StdError : int32_t
{
    OK   = ESP_OK,
    FAIL = ESP_FAIL,

    NO_MEM           = ESP_ERR_NO_MEM,
    INVALID_ARGS     = ESP_ERR_INVALID_ARG,
    INVALID_ARGUMENT = ESP_ERR_INVALID_ARG,

    INVALID_STATE    = ESP_ERR_INVALID_STATE,    /*!< Invalid state */
    INVALID_SIZE     = ESP_ERR_INVALID_SIZE,     /*!< Invalid size */
    NOT_FOUND        = ESP_ERR_NOT_FOUND,        /*!< Requested resource not found */
    NOT_SUPPORTED    = ESP_ERR_NOT_SUPPORTED,    /*!< Operation or feature not supported */
    TIMEOUT          = ESP_ERR_TIMEOUT,          /*!< Operation timed out */
    INVALID_RESPONSE = ESP_ERR_INVALID_RESPONSE, /*!< Received response was invalid */
    INVALID_CRC      = ESP_ERR_INVALID_CRC,      /*!< CRC or checksum was invalid */
    INVALID_VERSION  = ESP_ERR_INVALID_VERSION,  /*!< Version was invalid */
    INVALID_MAC      = ESP_ERR_INVALID_MAC,      /*!< MAC address was invalid */
    NOT_FINISHED     = ESP_ERR_NOT_FINISHED,     /*!< Operation has not fully completed */
    NOT_ALLOWED      = ESP_ERR_NOT_ALLOWED,      /*!< Operation is not allowed */

    WIFI_BASE    = ESP_ERR_WIFI_BASE,      /*!< Starting number of WiFi error codes */
    MESH_BASE    = ESP_ERR_MESH_BASE,      /*!< Starting number of MESH error codes */
    FLASH_BASE   = ESP_ERR_FLASH_BASE,     /*!< Starting number of flash error codes */
    CRYPTO_BASE  = ESP_ERR_HW_CRYPTO_BASE, /*!< Starting number of HW cryptography module error codes */
    MEMPROT_BASE = ESP_ERR_MEMPROT_BASE,   /*!< Starting number of Memory Protection API error codes */
};

template <std::size_t Depth = error_trace_depth::RUN_FOR_YOUR_LIFE>
/**
 * @brief 包含常见 ESP-IDF 错误的标准集合，可自定义 cause 深度。
 * @code
 * using Errors = StdErrorSet<2>;
 * @endcode
 */
using StdErrorSet = TracedErrorSet<
    Depth,
    StdError::FAIL,
    StdError::NO_MEM,
    StdError::INVALID_ARGUMENT,
    StdError::INVALID_STATE,
    StdError::INVALID_SIZE,
    StdError::NOT_FOUND,
    StdError::NOT_SUPPORTED,
    StdError::TIMEOUT,
    StdError::INVALID_RESPONSE,
    StdError::INVALID_CRC,
    StdError::INVALID_VERSION,
    StdError::INVALID_MAC,
    StdError::NOT_FINISHED,
    StdError::NOT_ALLOWED>;

/// 默认保存 4 层 cause 的标准 ESP-IDF 错误集合。
using StdErrors = StdErrorSet<>;

template <>
struct ErrorNameOverride<StdError::INVALID_ARGUMENT>
{
    inline static constexpr const char* value = "INVALID_ARGUMENT";
};

/// 将 `esp_err_t` 按原数值转换为类型化 `StdError`。
constexpr StdError toStdErr(esp_err_t code) noexcept {
    return static_cast<StdError>(code);
}

/// 使用 ESP-IDF 返回原生错误名称；未知码通常返回 ESP-IDF 的 unknown 文案。
inline const char* toName(esp_err_t code) noexcept {
    return esp_err_to_name(code);
}

/// `StdError` 版本的 ESP-IDF 名称查询。
inline const char* toName(StdError code) noexcept {
    return esp_err_to_name(static_cast<esp_err_t>(code));
}

/// 可重入名称查询；缓冲区语义与 `esp_err_to_name_r()` 完全相同。
inline const char* toNameR(esp_err_t code, char* buf, size_t buflen) noexcept {
    return esp_err_to_name_r(code, buf, buflen);
}

template <auto Fallback>
/**
 * @brief 把 ESP-IDF 返回码延迟转换为 `Result<void,E>`。
 * Convert an ESP-IDF return code into a typed Result.
 *
 * @code
 * using SetupErrors = ErrorSet<
 *     SetupError::DriverFailed,
 *     StdError::NO_MEM,
 *     StdError::INVALID_STATE>;
 * using SetupResult = Result<void, SetupErrors>;
 *
 * SetupResult setup() {
 *     return NativeErr<SetupError::DriverFailed>(esp_driver_install(...));
 * }
 * @endcode
 *
 * `ESP_OK` 变为 `Ok()`；目标集合包含对应 `StdError` 时保留精确错误；否则
 * 顶层使用 `Fallback`，真实原生码和名称自动进入 cause chain。
 */
[[nodiscard]] inline auto NativeErr(esp_err_t code) {
    return NativeErrorProxy<StdError, Fallback> {
        static_cast<StdError>(code),
        StdError::OK,
        "esp_err_t",
        toName(code),
        nullptr,
    };
}

template <auto Fallback>
/**
 * @brief `NativeErr()` 的带静态上下文版本。
 * @code
 * return NativeErr<SetupError::DriverFailed>(
 *     esp_driver_install(...), "installing LED RMT driver failed");
 * @endcode
 */
[[nodiscard]] inline auto NativeErr(esp_err_t code, StaticErrorMessage message) {
    return NativeErrorProxy<StdError, Fallback> {
        static_cast<StdError>(code),
        StdError::OK,
        "esp_err_t",
        toName(code),
        message.get(),
    };
}
