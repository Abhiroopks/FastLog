#ifndef FASTLOG_H
#define FASTLOG_H

#include "FastLog_global.h"
#include <array>
#include <atomic>
#include <blockingconcurrentqueue.h>
#include <concurrentqueue.h>
#include <fstream>
#include <string>
#include <string_view>
#include <thread>

enum class Severity { INFO, DEBUG, WARNING, CRITICAL, FATAL, COUNT };

const std::array<std::string_view, static_cast<size_t>(Severity::COUNT)> sev_map = {
    "INFO",
    "DEBUG",
    "WARNING",
    "CRITICAL",
    "FATAL",
};

#define LOG_INFO(MSG) FastLog::getInstance().logMsg(Severity::INFO, MSG, __FILE__, __LINE__)
#define LOG_DEBUG(MSG) FastLog::getInstance().logMsg(Severity::DEBUG, MSG, __FILE__, __LINE__)
#define LOG_WARNING(MSG) FastLog::getInstance().logMsg(Severity::WARNING, MSG, __FILE__, __LINE__)
#define LOG_CRITICAL(MSG) FastLog::getInstance().logMsg(Severity::CRITICAL, MSG, __FILE__, __LINE__)
#define LOG_FATAL(MSG) FastLog::getInstance().logMsg(Severity::FATAL, MSG, __FILE__, __LINE__)

const unsigned int DEFAULT_BUFFER_SIZE = (1 << 14);

// Struct to encapsulate all info for a single log message.
struct LogMsg
{
    Severity level;
    std::string_view msg;
    std::string_view source;
    std::string timestamp;
    unsigned int line;

    LogMsg() = default;
    LogMsg(const Severity level,
           const std::string_view msg,
           const std::string_view source,
           const std::string timestamp,
           const unsigned int line);
};

class FASTLOG_EXPORT FastLog
{
public:
    // Retrieves the singleton object.
    static FastLog &getInstance();
    static void initialize(std::string fileName,
                           bool stdOut,
                           unsigned int bufferSize = DEFAULT_BUFFER_SIZE);

    // used to log messages to stdout / file
    void logMsg(const Severity level,
                const std::string_view &msg,
                const std::string_view &source,
                const unsigned int line);

    int getLogCount();

    void flush();

private:

    // make constructor private to enforce singleton pattern.
    FastLog();
    ~FastLog();
    void writeLoop();
    const std::string getTimestamp();
    void flushBuffer();

    std::thread writer;
    std::atomic<bool> finished;
    // std::queue<LogMsg> messages;
    moodycamel::BlockingConcurrentQueue<LogMsg> messages;
    std::ofstream *outputFile;
    std::atomic<int> logCount;
    unsigned int bufferMsgCount;
    std::string writeBuffer;

    static bool initialized;
    static std::string FILE_NAME;
    static bool STD_OUT;
    static unsigned int BUFFER_SIZE;
};

#endif // FASTLOG_H
