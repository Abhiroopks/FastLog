#ifndef FASTLOG_H
#define FASTLOG_H

#include "FastLog_global.h"
#include <fstream>
#include <memory>
#include <queue>
#include <string>
#include <thread>

class FASTLOG_EXPORT FastLog
{
public:
    // Retrieves the singleton object.
    static FastLog &getInstance(std::string fileName, bool stdOut);

    // used to log messages to stdout / file
    void logInfo(const std::string &msg);
    void logDebug(const std::string &msg);
    void logError(const std::string &msg);
    void logCritical(const std::string &msg);
    void logFatal(const std::string &msg);

private:
    static FastLog *instance;

    // make constructor private to enforce singleton pattern.
    FastLog(std::string fileName, bool stdOut);
    ~FastLog();
    void writeLoop();

    std::thread writer;
    std::queue<std::string> messages;
    std::ofstream *outputFile;

    const std::string getTimestamp();

    void logMsg(const std::string &level, const std::string &msg);
};

#endif // FASTLOG_H
