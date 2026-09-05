#ifndef FASTLOG_H
#define FASTLOG_H

#include "FastLog_global.h"
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#define LOG_INFO(MSG) FastLog::getInstance().logMsg("INFO", MSG, __FILE__, __LINE__)
#define LOG_DEBUG(MSG) FastLog::getInstance().logMsg("DEBUG", MSG, __FILE__, __LINE__)
#define LOG_WARNING(MSG) FastLog::getInstance().logMsg("WARNING", MSG, __FILE__, __LINE__)
#define LOG_CRITICAL(MSG) FastLog::getInstance().logMsg("CRITICAL", MSG, __FILE__, __LINE__)
#define LOG_FATAL(MSG) FastLog::getInstance().logMsg("FATAL", MSG, __FILE__, __LINE__)

const unsigned int DEFAULT_BUFFER_SIZE = (1 << 14);

// Struct to encapsulate all info for a single log message.
struct LogMsg
{
    std::string level;
    std::string msg;
    std::string source;
    std::string timestamp;
    int line;

    LogMsg() = default;
    LogMsg(const std::string level,
           const std::string msg,
           const std::string source,
           const std::string timestamp,
           const int line);
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
    void logMsg(const std::string &level,
                const std::string &msg,
                const std::string &source,
                const int line);

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
    std::mutex mtx;
    std::atomic<bool> finished;
    std::condition_variable cv;
    std::queue<LogMsg> messages;
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
