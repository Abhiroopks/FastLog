#ifndef FASTLOG_H
#define FASTLOG_H

#include "FastLog_global.h"
#include <queue>
#include <string>
#include <thread>

class FASTLOG_EXPORT FastLog
{
public:
    // used to get reference to the singular fastlog instance.
    static FastLog &getInstance();

    // used to log messages to stdout / file
    void logInfo(std::string &msg);
    void logDebug(std::string &msg);
    void logError(std::string &msg);
    void logCritical(std::string &msg);
    void logFatal(std::string &msg);

private:
    // make private to control number of instances.
    FastLog();

    void writeLoop();

    std::thread writer;
    std::queue<std::string> messages;
};

#endif // FASTLOG_H
