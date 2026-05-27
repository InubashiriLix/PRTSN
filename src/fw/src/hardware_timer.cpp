#include "src/fw/inc/hardware_timer.h"

HardwareTimer::HardwareTimer() : HardwareTimer(Config {}) {}

HardwareTimer::HardwareTimer(const Config& config)
    : m_config(config),
      m_status {
          .started      = false,
          .running      = false,
          .resolutionHz = config.resolutionHz,
          .alarmTicks   = config.alarmTicks,
          .autoReload   = config.autoReload,
          .reloadCount  = config.reloadCount,
          .lastError    = Error::OK,
      } {}

HardwareTimer::~HardwareTimer() {
    end();
}

bool HardwareTimer::begin() {
    if (m_timer != nullptr) {
        setError(Error::ALREADY_STARTED);
        return false;
    }

    if (m_config.resolutionHz == 0) {
        setError(Error::INVALID_ARGUMENT);
        return false;
    }

    m_timer = timerBegin(m_config.resolutionHz);
    if (m_timer == nullptr) {
        setError(Error::BEGIN_FAILED);
        return false;
    }

    timerStop(m_timer);
    m_status.started      = true;
    m_status.running      = false;
    m_status.resolutionHz = timerGetFrequency(m_timer);
    m_status.alarmTicks   = m_config.alarmTicks;
    m_status.autoReload   = m_config.autoReload;
    m_status.reloadCount  = m_config.reloadCount;

    if (m_callback != nullptr) {
        timerAttachInterruptArg(m_timer, interruptEntry, this);
    }

    if (m_config.alarmTicks > 0) {
        timerAlarm(m_timer, m_config.alarmTicks, m_config.autoReload, m_config.reloadCount);
    }

    setError(Error::OK);

    if (m_config.autoStart) {
        return start();
    }

    return true;
}

bool HardwareTimer::begin(const Config& config) {
    if (m_timer != nullptr) {
        setError(Error::ALREADY_STARTED);
        return false;
    }

    m_config = config;
    m_status = Status {
        .started      = false,
        .running      = false,
        .resolutionHz = config.resolutionHz,
        .alarmTicks   = config.alarmTicks,
        .autoReload   = config.autoReload,
        .reloadCount  = config.reloadCount,
        .lastError    = Error::OK,
    };

    return begin();
}

void HardwareTimer::end() {
    if (m_timer == nullptr) {
        m_status.started = false;
        m_status.running = false;
        return;
    }

    timerDetachInterrupt(m_timer);
    timerEnd(m_timer);
    m_timer          = nullptr;
    m_status.started = false;
    m_status.running = false;
    setError(Error::OK);
}

bool HardwareTimer::attachInterrupt(Callback callback, void* context) {
    if (callback == nullptr) {
        setError(Error::INVALID_ARGUMENT);
        return false;
    }

    m_callback = callback;
    m_context  = context;

    if (m_timer != nullptr) {
        timerAttachInterruptArg(m_timer, interruptEntry, this);
    }

    setError(Error::OK);
    return true;
}

void HardwareTimer::detachInterrupt() {
    if (m_timer != nullptr) {
        timerDetachInterrupt(m_timer);
    }

    m_callback = nullptr;
    m_context  = nullptr;
}

bool HardwareTimer::setAlarmTicks(uint64_t alarmTicks, bool autoReload, uint64_t reloadCount) {
    if (alarmTicks == 0) {
        setError(Error::INVALID_ARGUMENT);
        return false;
    }

    m_config.alarmTicks  = alarmTicks;
    m_config.autoReload  = autoReload;
    m_config.reloadCount = reloadCount;
    m_status.alarmTicks  = alarmTicks;
    m_status.autoReload  = autoReload;
    m_status.reloadCount = reloadCount;

    if (m_timer != nullptr) {
        timerAlarm(m_timer, alarmTicks, autoReload, reloadCount);
    }

    setError(Error::OK);
    return true;
}

bool HardwareTimer::setPeriodUs(uint64_t periodUs, bool autoReload, uint64_t reloadCount) {
    if (periodUs == 0 || m_config.resolutionHz == 0) {
        setError(Error::INVALID_ARGUMENT);
        return false;
    }

    const uint64_t ticks = (periodUs * static_cast<uint64_t>(m_config.resolutionHz)) / 1000000ULL;
    return setAlarmTicks(ticks, autoReload, reloadCount);
}

bool HardwareTimer::start() {
    if (!ensureStarted()) {
        return false;
    }

    if (m_callback == nullptr) {
        setError(Error::CALLBACK_NOT_SET);
        return false;
    }

    if (m_status.alarmTicks == 0) {
        setError(Error::INVALID_ARGUMENT);
        return false;
    }

    timerStart(m_timer);
    m_status.running = true;
    setError(Error::OK);
    return true;
}

void HardwareTimer::stop() {
    if (m_timer == nullptr) {
        m_status.running = false;
        return;
    }

    timerStop(m_timer);
    m_status.running = false;
    setError(Error::OK);
}

bool HardwareTimer::restart() {
    if (!ensureStarted()) {
        return false;
    }

    timerRestart(m_timer);
    m_status.running = true;
    setError(Error::OK);
    return true;
}

void HardwareTimer::write(uint64_t ticks) {
    if (m_timer != nullptr) {
        timerWrite(m_timer, ticks);
    }
}

uint64_t HardwareTimer::readTicks() const {
    return m_timer != nullptr ? timerRead(m_timer) : 0;
}

uint64_t HardwareTimer::readUs() const {
    return m_timer != nullptr ? timerReadMicros(m_timer) : 0;
}

uint32_t HardwareTimer::getResolutionHz() const {
    return m_status.resolutionHz;
}

bool HardwareTimer::getStarted() const {
    return m_status.started;
}

bool HardwareTimer::getRunning() const {
    return m_status.running;
}

HardwareTimer::Status HardwareTimer::getStatus() const {
    return m_status;
}

HardwareTimer::Error HardwareTimer::getLastError() const {
    return m_status.lastError;
}

void IRAM_ATTR HardwareTimer::interruptEntry(void* context) {
    auto* self = static_cast<HardwareTimer*>(context);
    if (self == nullptr || self->m_callback == nullptr) {
        return;
    }

    self->m_callback(self->m_context);
}

bool HardwareTimer::ensureStarted() {
    if (m_timer != nullptr && m_status.started) {
        return true;
    }

    setError(Error::NOT_STARTED);
    return false;
}

void HardwareTimer::setError(Error error) {
    m_status.lastError = error;
}
