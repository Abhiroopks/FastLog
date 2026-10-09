#ifndef FASTLOG_H
#define FASTLOG_H

#include "FastLog_global.h"
#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <lfrb.hpp>
#include <source_location>
#include <string>
#include <thread>

enum class Severity { INFO, DEBUG, WARNING, CRITICAL, FATAL, COUNT };

const std::array<std::string, static_cast<size_t>(Severity::COUNT)> sev_map = {
    "INFO",
    "DEBUG",
    "WARNING",
    "CRITICAL",
    "FATAL",
};

const unsigned int DEFAULT_BUFFER_SIZE = (1 << 14);
const unsigned int DEFAULT_LOG_QUEUE_SIZE = (1 << 20);
const unsigned int DEFAULT_LOG_FILE_SIZE = (1 << 24);
const std::chrono::nanoseconds DEFAULT_BLOCKING_TIME = std::chrono::nanoseconds(1);

// Struct to encapsulate all info for a single log message.
struct LogMsg
{
    Severity level;
    std::string msg;
    std::string source;
    // std::chrono::local_time<std::chrono::system_clock::duration> timestamp;
    time_t timestamp;
    unsigned int line;

    LogMsg() = default;
    LogMsg(const Severity level,
           std::string &&_msg,
           std::string &&_source,
           // std::chrono::local_time<std::chrono::system_clock::duration> &&timestamp,
           time_t timestamp,
           const uint8_t line);
};

class FASTLOG_EXPORT FastLog
{
public:
    FastLog(std::string fileName,
            bool stdOut = false,
            unsigned int bufferSize = DEFAULT_BUFFER_SIZE,
            unsigned int queueSize = DEFAULT_LOG_QUEUE_SIZE,
            unsigned int logFileMaxSize = DEFAULT_LOG_FILE_SIZE,
            bool blocking = false);

    ~FastLog();

    // Prevent copies and moves of FastLog instance
    FastLog(const FastLog &) = delete;
    FastLog &operator=(const FastLog &) = delete;
    FastLog(FastLog &&) = delete;
    FastLog &operator=(FastLog &&) = delete;

    // Severity logging methods capturing source location automatically
    void info(std::string msg, const std::source_location loc = std::source_location::current());
    void debug(std::string msg, const std::source_location loc = std::source_location::current());
    void warning(std::string msg, const std::source_location loc = std::source_location::current());
    void critical(std::string msg, const std::source_location loc = std::source_location::current());
    [[noreturn]] void fatal(std::string msg, const std::source_location loc = std::source_location::current());

    // Explicit source and line overloads
    void info(std::string msg, std::string source, const unsigned int line);
    void debug(std::string msg, std::string source, const unsigned int line);
    void warning(std::string msg, std::string source, const unsigned int line);
    void critical(std::string msg, std::string source, const unsigned int line);
    [[noreturn]] void fatal(std::string msg, std::string source, const unsigned int line);

    // Named log<Level> convenience aliases
    void logInfo(std::string msg, const std::source_location loc = std::source_location::current()) {
        info(std::move(msg), loc);
    }
    void logInfo(std::string msg, std::string source, const unsigned int line) {
        info(std::move(msg), std::move(source), line);
    }

    void logDebug(std::string msg, const std::source_location loc = std::source_location::current()) {
        debug(std::move(msg), loc);
    }
    void logDebug(std::string msg, std::string source, const unsigned int line) {
        debug(std::move(msg), std::move(source), line);
    }

    void logWarning(std::string msg, const std::source_location loc = std::source_location::current()) {
        warning(std::move(msg), loc);
    }
    void logWarning(std::string msg, std::string source, const unsigned int line) {
        warning(std::move(msg), std::move(source), line);
    }

    void logCritical(std::string msg, const std::source_location loc = std::source_location::current()) {
        critical(std::move(msg), loc);
    }
    void logCritical(std::string msg, std::string source, const unsigned int line) {
        critical(std::move(msg), std::move(source), line);
    }

    [[noreturn]] void logFatal(std::string msg, const std::source_location loc = std::source_location::current()) {
        fatal(std::move(msg), loc);
    }
    [[noreturn]] void logFatal(std::string msg, std::string source, const unsigned int line) {
        fatal(std::move(msg), std::move(source), line);
    }

    // Snake_case aliases
    void log_info(std::string msg, const std::source_location loc = std::source_location::current()) {
        info(std::move(msg), loc);
    }
    void log_info(std::string msg, std::string source, const unsigned int line) {
        info(std::move(msg), std::move(source), line);
    }

    void log_debug(std::string msg, const std::source_location loc = std::source_location::current()) {
        debug(std::move(msg), loc);
    }
    void log_debug(std::string msg, std::string source, const unsigned int line) {
        debug(std::move(msg), std::move(source), line);
    }

    void log_warning(std::string msg, const std::source_location loc = std::source_location::current()) {
        warning(std::move(msg), loc);
    }
    void log_warning(std::string msg, std::string source, const unsigned int line) {
        warning(std::move(msg), std::move(source), line);
    }

    void log_critical(std::string msg, const std::source_location loc = std::source_location::current()) {
        critical(std::move(msg), loc);
    }
    void log_critical(std::string msg, std::string source, const unsigned int line) {
        critical(std::move(msg), std::move(source), line);
    }

    [[noreturn]] void log_fatal(std::string msg, const std::source_location loc = std::source_location::current()) {
        fatal(std::move(msg), loc);
    }
    [[noreturn]] void log_fatal(std::string msg, std::string source, const unsigned int line) {
        fatal(std::move(msg), std::move(source), line);
    }

    // used to log messages to stdout / file
    void logMsg(const Severity level,
                std::string msg,
                std::string source,
                const unsigned int line);

#ifdef TESTING
    unsigned int getLogCount();
#endif
    void flush();

private:
    void writeLoop();
    void flushBuffer();

    std::thread writer;
    std::atomic<bool> finished;
    LockFreeRingBuffer<LogMsg> messages;
    std::ofstream *outputFile;
    std::string fileName;
    std::string fileBaseName;
    std::string fileExt;
    bool stdOut;
    unsigned int bufferSize;
    unsigned int logQueueSize;
    unsigned int logFileMaxSize;
    bool blocking;
    unsigned int logSize;
    unsigned int logNum;
    const std::chrono::time_zone *timeZone;

#ifdef TESTING
    std::atomic<int> logCount;
    unsigned int bufferMsgCount;
#endif
    std::string writeBuffer;
};

#endif // FASTLOG_H
