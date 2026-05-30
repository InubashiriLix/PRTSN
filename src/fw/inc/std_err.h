#pragma once

#include <cstddef>
#include <cstdint>

#include <esp_err.h>

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

constexpr StdError toStdErr(esp_err_t code) noexcept {
    return static_cast<StdError>(code);
}

inline const char* toName(esp_err_t code) noexcept {
    return esp_err_to_name(code);
}

inline const char* toName(StdError code) noexcept {
    return esp_err_to_name(static_cast<esp_err_t>(code));
}

inline const char* toNameR(esp_err_t code, char* buf, size_t buflen) noexcept {
    return esp_err_to_name_r(code, buf, buflen);
}
