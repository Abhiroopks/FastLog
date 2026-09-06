#include "fastlog.h"
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <source_location>
#include <sstream>

// Constructor for LogMsg struct.
LogMsg::LogMsg(const std::string level,
               const std::string msg,
               const std::string source,
               const std::string timestamp,
               const int line)
    : level(level)
    , msg(msg)
    , source(source)
    , timestamp(timestamp)
    , line(line)
{}

/**
 * @brief FastLog::FastLog Private constructor
 * @param fileName the path to file where logs will be saved.
 * @param stdOut whether or not messages should be printed to stdout.
 */
FastLog::FastLog()
    : finished(false)
    , logCount(0)
{
    outputFile = new std::ofstream(FILE_NAME, std::ofstream::out);
    if (!outputFile->is_open()) {
        std::cout << "Failed to open log file for writing in constructor" << std::endl;
        return;
    }

    writer = std::thread([this] { this->writeLoop(); });

    writeBuffer.reserve(BUFFER_SIZE);
}

FastLog::~FastLog()
{
    finished.store(true);
    cv.notify_all();
    if (writer.joinable()) {
        writer.join();
    }

    if (writeBuffer.size() > 0) {
        flushBuffer();
    }

    outputFile->close();
    delete outputFile;

}

/**
 * @brief FastLog::getInstance Returns the singleton instance.
 * @return singleton instance of FastLog.
 */
FastLog &FastLog::getInstance()
{
    if (!initialized) {
        throw std::runtime_error("FastLog not initialized yet");
    }

    // Created on first call.
    static FastLog instance;
    return instance;
}

/**
 * @brief FastLog::initialize initializes the file name and stdout param.
 * @param fileName the path to file where logs will be saved.
 * @param stdOut whether or not messages should be printed to stdout.
 * @param bufferSize the size, in bytes, of the writeBuffer. Defaults to 16 KB.
 */
void FastLog::initialize(std::string fileName, bool stdOut, unsigned int bufferSize)
{
    if (initialized) {
        std::cout << "already initialized FastLog" << std::endl;
        return;
    }

    FILE_NAME = fileName;
    STD_OUT = stdOut;
    BUFFER_SIZE = bufferSize;

    initialized = true;
}

// used to log messages to stdout / file
void FastLog::logMsg(const std::string &level,
                     const std::string &msg,
                     const std::string &source,
                     const int line)
{
    if (finished.load()) {
        std::cout << "Attempting to log a message after logger deleted.";
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mtx);
        messages.push(LogMsg(level, msg, source, FastLog::getTimestamp(), line));
    }

    cv.notify_one();
}

void FastLog::writeLoop()
{
    LogMsg logMsg;

    while (true) {
        {
            std::unique_lock<std::mutex> lock(mtx);
            // Wait until there is data or a finish signal
            cv.wait(lock, [&]() { return !messages.empty() || finished.load(); });

            if (finished.load() && messages.empty()) {
                break;
            }

            logMsg = std::move(messages.front());
            messages.pop();
        }

        // Check if this a special flush request
        if (logMsg.line == -1) {
            flushBuffer();

            // no need to process this msg, as it's just
            // a flush request.
            continue;
        }

        if (writeBuffer.size() >= BUFFER_SIZE) {
            flushBuffer();
        }

        // Process msg
        if (STD_OUT) {
            std::cout << logMsg.msg << std::endl;
        }

        std::ostringstream oss;
        oss << "{\"timestamp\":\"" << logMsg.timestamp << "\",\"level\":\"" << logMsg.level
            << "\",\"source\":\"" << logMsg.source << "\",\"line\":" << logMsg.line << ",\"msg\":\""
            << logMsg.msg << "\"}\n";
        writeBuffer.append(oss.str());

        bufferMsgCount++;

    }
}

const std::string FastLog::getTimestamp()
{
    std::time_t now = std::time(nullptr);
    std::tm *tm_info = std::localtime(&now);
    std::ostringstream oss;
    // Format: DD-MM-YYYY HH-MM-SS
    oss << std::put_time(tm_info, "%d-%m-%Y %H:%M:%S");
    return oss.str();
}

void FastLog::flushBuffer()
{
    if (!outputFile->is_open()) {
        std::cout << "Failed to open log file for writing" << std::endl;
        return;
    }
    *outputFile << writeBuffer;
    logCount.fetch_add(bufferMsgCount);
    bufferMsgCount = 0;
    writeBuffer.clear();
}

void FastLog::flush()
{
    // use a special LogMsg to force a flush to disk.
    logMsg("", "", "", -1);
}

int FastLog::getLogCount()
{
    return logCount.load();
}

std::string FastLog::FILE_NAME = "";
bool FastLog::STD_OUT = false;
bool FastLog::initialized = false;
unsigned int FastLog::BUFFER_SIZE = 0;
