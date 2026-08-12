#pragma once

#include "src/dom/NodeInfo.h"
#include "src/dvc/inc/Serial.h"
#include "src/fw/inc/Result.h"
#include "freertos/idf_additions.h"

#include <cstdarg>
#include <cstddef>
#include <cstdint>

class SerialConsoleService
{
public:
    enum class OutputLevel : uint8_t
    {
        NONE  = 0,
        ERROR = 1,
        WARN  = 2,
        INFO  = 3,
        DEBUG = 4,
    };

    using OUTPUT_LEVEL   = OutputLevel;
    using Handler        = void (*)(SerialConsoleService& service, const char* args);
    using ContextHandler = void (*)(SerialConsoleService& service, const char* args, void* context);

    struct Command
    {
        const char*    name           = nullptr;
        Handler        handler        = nullptr;
        ContextHandler contextHandler = nullptr;
        void*          context        = nullptr;
        const char*    description    = nullptr;

        bool operator==(const Command& other) const;
    };

    static constexpr size_t MaxCommands       = 32;
    static constexpr size_t MaxCommandNameLen = 24;
    static constexpr size_t ReadBufferLen     = 64;
    static constexpr size_t CommandLineLen    = 128;

private:
    dvc::Serial&      m_serial;
    Command           m_commands[MaxCommands] {};
    size_t            m_commandsCount = 0;
    OutputLevel       m_outputLevel   = OutputLevel::INFO;
    SemaphoreHandle_t m_mutex         = nullptr;

    uint8_t m_readBuffer[ReadBufferLen] {};
    char    m_commandLine[CommandLineLen] {};
    size_t  m_commandLineLen = 0;

public:
    explicit SerialConsoleService(dvc::Serial& serial);

    bool setup();

    bool registerCommand(const Command& command);
    bool registerCommand(const char* name, Handler handler, const char* description);
    bool registerCommand(const char* name, ContextHandler handler, void* context, const char* description);
    void initDefaultCommands();
    bool executeCommand(const char* input);

    void        changeOutputLevel(OutputLevel level);
    OutputLevel outputLevel() const;

    void log(const char* format, ...) __attribute__((format(printf, 2, 3)));
    void info(const char* format, ...) __attribute__((format(printf, 2, 3)));
    void debug(const char* format, ...) __attribute__((format(printf, 2, 3)));
    void warn(const char* format, ...) __attribute__((format(printf, 2, 3)));
    void error(const char* format, ...) __attribute__((format(printf, 2, 3)));
    void err(const char* format, ...) __attribute__((format(printf, 2, 3)));

    /** `va_list` variants for small optional logging adapters in other services. */
    void vinfo(const char* format, va_list args) __attribute__((format(printf, 2, 0)));
    void vdebug(const char* format, va_list args) __attribute__((format(printf, 2, 0)));
    void vwarn(const char* format, va_list args) __attribute__((format(printf, 2, 0)));
    void verror(const char* format, va_list args) __attribute__((format(printf, 2, 0)));

    /**
     * @brief 输出 Result/ErrorSet 的顶层错误和完整 cause chain。
     * Print a typed Result error and its complete cause chain.
     *
     * @code
     * if (result.is_err())
     *     console.errorResult("BLE setup", result.error());
     * @endcode
     */
    template <size_t Depth, auto... Errors>
    void errorResult(const char* operation, const TracedErrorSet<Depth, Errors...>& value) {
        error("%s failed: %s::%s (%ld)%s%s",
              operation != nullptr ? operation : "operation",
              value.domain(),
              value.name(),
              static_cast<long>(value.numeric_code()),
              value.has_message() ? ": " : "",
              value.message());
        value.for_each_cause([this](const ErrorFrame& cause) {
            error("  caused by %s::%s (%ld)%s%s",
                  cause.domain,
                  cause.name,
                  static_cast<long>(cause.numericCode),
                  cause.message != nullptr && cause.message[0] != '\0' ? ": " : "",
                  cause.message != nullptr ? cause.message : "");
        });
    }

    void hexDump(const char* prefix, const uint8_t* data, size_t len);

    void printBootBanner(const NodeInfo& info);
    void printState(NodeState state);
    void printHeartbeat(const NodeInfo& info);

    void updateCommandResponse();

private:
    bool lock();
    void unlock();

    bool        findAndExecuteCommand(const char* name, const char* args);
    void        parseCommand(const char* input, char* name, size_t nameLen, const char*& args) const;
    bool        verifyCommandConfig(const Command& command) const;
    bool        setPrintLevel(OutputLevel level) const;
    void        vlog(OutputLevel level, const char* prefix, const char* format, va_list args);
    const char* stateName(NodeState state) const;

    static void        cmdHelpHandler(SerialConsoleService& service, const char* args);
    static void        cmdLevelHandler(SerialConsoleService& service, const char* args);
    static const char* levelName(OutputLevel level);
    static bool        parseLevel(const char* text, OutputLevel& level);
};
