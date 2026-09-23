#ifndef FASTLOG_H
#define FASTLOG_H

#include "FastLog_global.h"
#include <array>
#include <atomic>
// #include <blockingconcurrentqueue.h>
// #include <concurrentqueue.h>
#include <fstream>
#include <lfrb.hpp>
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

#define LOG_INFO(MSG) \
    FastLog::getInstance().logMsg(Severity::INFO, std::move(MSG), std::move(__FILE__), __LINE__)
#define LOG_DEBUG(MSG) \
    FastLog::getInstance().logMsg(Severity::DEBUG, std::move(MSG), std::move(__FILE__), __LINE__)
#define LOG_WARNING(MSG) \
    FastLog::getInstance().logMsg(Severity::WARNING, std::move(MSG), std::move(__FILE__), __LINE__)
#define LOG_CRITICAL(MSG) \
    FastLog::getInstance().logMsg(Severity::CRITICAL, std::move(MSG), std::move(__FILE__), __LINE__)
#define LOG_FATAL(MSG) \
    FastLog::getInstance().logMsg(Severity::FATAL, std::move(MSG), std::move(__FILE__), __LINE__)

const unsigned int DEFAULT_BUFFER_SIZE = (1 << 14);
const unsigned int DEFAULT_LOG_QUEUE_SIZE = (1 << 20);

// Struct to encapsulate all info for a single log message.
struct LogMsg
{
    Severity level;
    std::string msg;
    std::string source;
    std::string timestamp;
    unsigned int line;

    LogMsg() = default;
    LogMsg(const Severity level,
           std::string msg,
           std::string source,
           std::string timestamp,
           const unsigned int line);
};

class FASTLOG_EXPORT FastLog
{
public:
    // Retrieves the singleton object.
    static FastLog &getInstance();
    static void initialize(std::string fileName,
                           bool stdOut,
                           unsigned int bufferSize = DEFAULT_BUFFER_SIZE,
                           unsigned queueSize = DEFAULT_LOG_QUEUE_SIZE);

    // used to log messages to stdout / file
    void logMsg(const Severity level, std::string msg, std::string source, const unsigned int line);

    int getLogCount();

    void flush();

private:

    // make constructor private to enforce singleton pattern.
    FastLog();
    ~FastLog();
    void writeLoop();
    std::string getTimestamp();
    void flushBuffer();

    std::thread writer;
    std::atomic<bool> finished;
    LockFreeRingBuffer<LogMsg> messages;
    std::ofstream *outputFile;
    std::atomic<int> logCount;
    unsigned int bufferMsgCount;
    std::string writeBuffer;

    static bool initialized;
    static std::string FILE_NAME;
    static bool STD_OUT;
    static unsigned int BUFFER_SIZE;
    static unsigned int LOG_QUEUE_SIZE;
};

#endif // FASTLOG_H
