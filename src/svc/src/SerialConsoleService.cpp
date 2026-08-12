#include "src/svc/inc/SerialConsoleService.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <Arduino.h>
#include <cstring>

#ifdef Serial
#undef Serial
#endif

SerialConsoleService::SerialConsoleService(dvc::Serial& serial)
    : m_serial(serial),
      m_mutex(xSemaphoreCreateMutex()) {}

bool SerialConsoleService::Command::operator==(const SerialConsoleService::Command& other) const {
    if (name == nullptr || other.name == nullptr) {
        return name == other.name;
    }

    return std::strcmp(name, other.name) == 0;
}

bool SerialConsoleService::setup() {
    initDefaultCommands();
    return m_serial.setup();
}

bool SerialConsoleService::registerCommand(const Command& command) {
    if (!verifyCommandConfig(command) || m_commandsCount >= MaxCommands) {
        return false;
    }

    for (size_t i = 0; i < m_commandsCount; ++i) {
        if (m_commands[i] == command) {
            return false;
        }
    }

    m_commands[m_commandsCount++] = command;
    return true;
}

bool SerialConsoleService::registerCommand(const char* name, Handler handler, const char* description) {
    return registerCommand(Command {
        .name        = name,
        .handler     = handler,
        .description = description,
    });
}

bool SerialConsoleService::registerCommand(const char*    name,
                                           ContextHandler handler,
                                           void*          context,
                                           const char*    description) {
    return registerCommand(Command {
        .name           = name,
        .contextHandler = handler,
        .context        = context,
        .description    = description,
    });
}

void SerialConsoleService::initDefaultCommands() {
    registerCommand("help", cmdHelpHandler, "show registered commands");
    registerCommand("level", cmdLevelHandler, "set log level: none/error/warn/info/debug");
}

bool SerialConsoleService::executeCommand(const char* input) {
    char        name[MaxCommandNameLen] {};
    const char* args = nullptr;
    parseCommand(input, name, sizeof(name), args);

    if (name[0] == '\0') {
        return false;
    }

    return findAndExecuteCommand(name, args);
}

void SerialConsoleService::changeOutputLevel(OutputLevel level) {
    m_outputLevel = level;
}

SerialConsoleService::OutputLevel SerialConsoleService::outputLevel() const {
    return m_outputLevel;
}

void SerialConsoleService::log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vlog(OutputLevel::INFO, "log", format, args);
    va_end(args);
}

void SerialConsoleService::info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vlog(OutputLevel::INFO, "info", format, args);
    va_end(args);
}

void SerialConsoleService::debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vlog(OutputLevel::DEBUG, "debug", format, args);
    va_end(args);
}

void SerialConsoleService::warn(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vlog(OutputLevel::WARN, "warn", format, args);
    va_end(args);
}

void SerialConsoleService::error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vlog(OutputLevel::ERROR, "error", format, args);
    va_end(args);
}

void SerialConsoleService::err(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vlog(OutputLevel::ERROR, "error", format, args);
    va_end(args);
}

void SerialConsoleService::vinfo(const char* format, va_list args) {
    vlog(OutputLevel::INFO, "info", format, args);
}

void SerialConsoleService::vdebug(const char* format, va_list args) {
    vlog(OutputLevel::DEBUG, "debug", format, args);
}

void SerialConsoleService::vwarn(const char* format, va_list args) {
    vlog(OutputLevel::WARN, "warn", format, args);
}

void SerialConsoleService::verror(const char* format, va_list args) {
    vlog(OutputLevel::ERROR, "error", format, args);
}

void SerialConsoleService::hexDump(const char* prefix, const uint8_t* data, size_t len) {
    if (prefix == nullptr || data == nullptr || !setPrintLevel(OutputLevel::INFO) || !lock()) {
        return;
    }

    m_serial.printf("[log] %s:", prefix);
    for (size_t i = 0; i < len; ++i) {
        m_serial.printf(" %02X", data[i]);
    }
    m_serial.println();
    unlock();
}

void SerialConsoleService::printBootBanner(const NodeInfo& info) {
    if (!lock()) {
        return;
    }

    m_serial.println();
    m_serial.println("== PRTN boot ==");
    m_serial.printf("Project : %s (%s)\n", info.projectName, info.fullName);
    m_serial.printf("Board   : %s\n", info.boardName);
    m_serial.printf("Version : %s\n", info.version);
    m_serial.printf("Node    : %s (%s)\n", info.nodeName, info.nodeId);
    unlock();
}

void SerialConsoleService::printState(NodeState state) {
    if (!setPrintLevel(OutputLevel::INFO) || !lock()) {
        return;
    }

    m_serial.printf("[state] %s\n", stateName(state));
    unlock();
}

void SerialConsoleService::printHeartbeat(const NodeInfo& info) {
    if (!setPrintLevel(OutputLevel::INFO)) {
        return;
    }

    if (!lock()) {
        return;
    }

    m_serial.printf("[heartbeat] node=%s uptime=%lu ms\n", info.nodeId, millis());
    unlock();
}

void SerialConsoleService::updateCommandResponse() {
    const size_t bytesRead = m_serial.read(m_readBuffer, sizeof(m_readBuffer));
    for (size_t i = 0; i < bytesRead; ++i) {
        const uint8_t byte = m_readBuffer[i];

        if (byte == '\r' || byte == '\n') {
            if (m_commandLineLen > 0) {
                m_commandLine[m_commandLineLen] = '\0';
                executeCommand(m_commandLine);
                m_commandLineLen = 0;
                m_commandLine[0] = '\0';
            }
            continue;
        }

        if (byte == '\b' || byte == 0x7F) {
            if (m_commandLineLen > 0) {
                --m_commandLineLen;
                m_commandLine[m_commandLineLen] = '\0';
            }
            continue;
        }

        if (byte < 0x20 || byte > 0x7E) {
            continue;
        }

        if (m_commandLineLen + 1 >= sizeof(m_commandLine)) {
            m_commandLineLen = 0;
            m_commandLine[0] = '\0';
            warn("command line too long");
            continue;
        }

        m_commandLine[m_commandLineLen++] = static_cast<char>(byte);
        m_commandLine[m_commandLineLen]   = '\0';
    }
}

bool SerialConsoleService::lock() {
    return m_mutex != nullptr && xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE;
}

void SerialConsoleService::unlock() {
    if (m_mutex != nullptr) {
        xSemaphoreGive(m_mutex);
    }
}

bool SerialConsoleService::findAndExecuteCommand(const char* name, const char* args) {
    for (size_t i = 0; i < m_commandsCount; ++i) {
        const Command& command = m_commands[i];
        if (std::strcmp(command.name, name) == 0) {
            if (command.handler != nullptr) {
                command.handler(*this, args == nullptr ? "" : args);
            }
            else {
                command.contextHandler(*this, args == nullptr ? "" : args, command.context);
            }
            return true;
        }
    }

    warn("unknown command: %s", name);
    return false;
}

void SerialConsoleService::parseCommand(const char* input, char* name, size_t nameLen, const char*& args) const {
    args = "";
    if (input == nullptr || name == nullptr || nameLen == 0) {
        return;
    }

    while (*input == ' ' || *input == '\t') {
        ++input;
    }

    size_t nameIndex = 0;
    while (*input != '\0' && *input != ' ' && *input != '\t') {
        if (nameIndex + 1 < nameLen) {
            name[nameIndex++] = *input;
        }
        ++input;
    }
    name[nameIndex] = '\0';

    while (*input == ' ' || *input == '\t') {
        ++input;
    }
    args = input;
}

bool SerialConsoleService::verifyCommandConfig(const Command& command) const {
    return command.name != nullptr &&
           command.name[0] != '\0' &&
           (command.handler != nullptr || command.contextHandler != nullptr) &&
           command.description != nullptr;
}

bool SerialConsoleService::setPrintLevel(OutputLevel level) const {
    return m_outputLevel != OutputLevel::NONE &&
           static_cast<uint8_t>(level) <= static_cast<uint8_t>(m_outputLevel);
}

void SerialConsoleService::vlog(OutputLevel level, const char* prefix, const char* format, va_list args) {
    if (format == nullptr || !setPrintLevel(level) || !lock()) {
        return;
    }

    m_serial.printf("[%s] ", prefix);
    m_serial.vprintf(format, args);
    m_serial.println();
    unlock();
}

const char* SerialConsoleService::stateName(NodeState state) const {
    switch (state) {
        case BOOTING:
            return "Booting";
        case RUNNING:
            return "Running";
        case ERROR:
            return "Error";
    }

    return "Unknown";
}

void SerialConsoleService::cmdHelpHandler(SerialConsoleService& service, const char*) {
    service.info("available commands:");
    for (size_t i = 0; i < service.m_commandsCount; ++i) {
        service.info("  %s - %s", service.m_commands[i].name, service.m_commands[i].description);
    }
}

void SerialConsoleService::cmdLevelHandler(SerialConsoleService& service, const char* args) {
    OutputLevel level = OutputLevel::INFO;
    if (!parseLevel(args, level)) {
        service.info("current log level: %s", levelName(service.outputLevel()));
        service.info("usage: level none|error|warn|info|debug");
        return;
    }

    service.changeOutputLevel(level);
    service.info("log level: %s", levelName(level));
}

const char* SerialConsoleService::levelName(OutputLevel level) {
    switch (level) {
        case OutputLevel::NONE:
            return "none";
        case OutputLevel::ERROR:
            return "error";
        case OutputLevel::WARN:
            return "warn";
        case OutputLevel::INFO:
            return "info";
        case OutputLevel::DEBUG:
            return "debug";
    }

    return "unknown";
}

bool SerialConsoleService::parseLevel(const char* text, OutputLevel& level) {
    if (text == nullptr) {
        return false;
    }

    while (*text == ' ' || *text == '\t') {
        ++text;
    }

    if (std::strcmp(text, "none") == 0) {
        level = OutputLevel::NONE;
        return true;
    }
    if (std::strcmp(text, "error") == 0 || std::strcmp(text, "err") == 0) {
        level = OutputLevel::ERROR;
        return true;
    }
    if (std::strcmp(text, "warn") == 0 || std::strcmp(text, "warning") == 0) {
        level = OutputLevel::WARN;
        return true;
    }
    if (std::strcmp(text, "info") == 0 || std::strcmp(text, "log") == 0) {
        level = OutputLevel::INFO;
        return true;
    }
    if (std::strcmp(text, "debug") == 0) {
        level = OutputLevel::DEBUG;
        return true;
    }

    return false;
}
