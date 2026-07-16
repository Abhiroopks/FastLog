#include "fastlog.h"
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

/**
 * @brief FastLog::FastLog Private constructor
 * @param fileName the path to file where logs will be saved.
 * @param stdOut whether or not messages should be printed to stdout.
 */
FastLog::FastLog(std::string fileName, bool stdOut)
{
    outputFile = new std::ofstream(fileName, std::ofstream::out);
    if (!outputFile->is_open()) {
        std::cout << "Failed to open log file for writing";
    }
}

FastLog::~FastLog()
{
    outputFile->close();
    delete outputFile;
}

/**
 * @brief FastLog::getInstance Returns the singleton instance.
 * @param fileName the path to file where logs will be saved.
 * @param stdOut whether or not messages should be printed to stdout.
 * @return singleton instance of FastLog.
 */
FastLog &FastLog::getInstance(std::string fileName, bool stdOut)
{
    if (instance == nullptr) {
        instance = new FastLog(fileName, stdOut);
    }

    return *instance;
}

void FastLog::logMsg(const std::string &level, const std::string &msg)
{
    *outputFile << getTimestamp() << " | " << level << " | " << msg;
}

// used to log messages to stdout / file
void FastLog::logInfo(const std::string &msg)
{
    logMsg("INFO", msg);
}
void FastLog::logDebug(const std::string &msg)
{
    logMsg("DEBUG", msg);
}
void FastLog::logError(const std::string &msg)
{
    logMsg("ERROR", msg);
}
void FastLog::logCritical(const std::string &msg)
{
    logMsg("CRITICAL", msg);
}
void FastLog::logFatal(const std::string &msg)
{
    logMsg("FATAL", msg);
}

void FastLog::writeLoop() {}

const std::string FastLog::getTimestamp()
{
    std::time_t now = std::time(nullptr);
    std::tm *tm_info = std::localtime(&now);
    std::ostringstream oss;
    // Format: DD-MM-YYYY HH-MM-SS
    oss << std::put_time(tm_info, "%d-%m-%Y %H-%M-%S");
    return oss.str();
}

FastLog *FastLog::instance = nullptr;
