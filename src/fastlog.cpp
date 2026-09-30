#include "fastlog.h"
#include <chrono>
#include <fstream>
#include <iostream>

// Constructor for LogMsg struct.
LogMsg::LogMsg(const Severity level,
               std::string &&msg,
               std::string &&source,
               std::chrono::local_time<std::chrono::system_clock::duration> &&timestamp,
               const unsigned int line)
    : level(level)
    , msg(msg)
    , source(source)
    , timestamp(timestamp)
    , line(line)
{}

/**
 * @brief FastLog::FastLog Private constructor
 */
FastLog::FastLog()
    : finished(false)
#ifdef TESTING
    , logCount(0)
    , bufferMsgCount(0)
#endif
    , messages(LOG_QUEUE_SIZE)
    , logSize(0)
    , logNum(0)
{
    int dotIdx = FILE_NAME.find('.');
    fileBaseName = FILE_NAME.substr(0, dotIdx);
    fileExt = FILE_NAME.substr(dotIdx + 1);
    outputFile = new std::ofstream(fileBaseName + '-' + std::to_string(logNum) + '.' + fileExt,
                                   std::ofstream::out);
    if (!outputFile->is_open()) {
        std::cout << "Failed to open log file for writing in constructor" << std::endl;
        return;
    }

    writeBuffer.reserve(BUFFER_SIZE);
    writer = std::thread([this] { this->writeLoop(); });
}

FastLog::~FastLog()
{
    finished.store(true);

    if (writer.joinable()) {
        writer.join();
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
 * @param bufferSize the max size, in bytes, of the writeBuffer. Defaults to 16 KB.
 * @param queueSize the max size, in number of log messages, of the queue of log msgs. Defaults to 2^20 ~ 1 million.
 * @param logFileMaxSize the max size, in bytes, of the log file on disk, before rotating to a new one. Defaults to 16 MB.
 */
void FastLog::initialize(std::string fileName,
                         bool stdOut,
                         unsigned int bufferSize,
                         unsigned int queueSize,
                         unsigned int logFileMaxSize)
{
    if (initialized) {
        std::cout << "already initialized FastLog" << std::endl;
        return;
    }

    FILE_NAME = fileName;
    STD_OUT = stdOut;
    BUFFER_SIZE = bufferSize;
    LOG_QUEUE_SIZE = queueSize;
    LOG_FILE_MAX_SIZE = logFileMaxSize;

    initialized = true;
}

// used to log messages to stdout / file
void FastLog::logMsg(Severity level,
                     std::string &&msg,
                     std::string &&source,
                     const unsigned int line)
{
    if (finished.load()) {
        std::cout << "Attempting to log a message after logger deleted.";
        return;
    }

    auto timePoint = std::chrono::current_zone()->to_local(std::chrono::system_clock::now());
    messages.push(LogMsg(level, std::move(msg), std::move(source), std::move(timePoint), line));
}

void FastLog::writeLoop()
{
    LogMsg logMsg;

    while (true) {
        if (finished.load()) {
            break;
        }

        bool success = messages.pop(logMsg);

        // got nothing from queue, restart loop.
        if(!success){
            continue;
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

        writeBuffer.append("{\"timestamp\":\""
                           + std::format("{:%d-%m-%Y %H:%M:%S}", logMsg.timestamp)
                           + "\",\"level\":\"" + sev_map[static_cast<size_t>(logMsg.level)]
                           + "\",\"source\":\"" + logMsg.source + "\",\"line\":"
                           + std::to_string(logMsg.line) + ",\"msg\":\"" + logMsg.msg + "\"}\n");

#ifdef TESTING
        bufferMsgCount++;
#endif
    }
}

void FastLog::flushBuffer()
{
    if (!outputFile->is_open()) {
        std::cout << "Failed to open log file for writing" << std::endl;
        return;
    }
    *outputFile << writeBuffer;
    logSize += writeBuffer.size();
    writeBuffer.clear();

    // rotate log file if needed
    if (logSize >= LOG_FILE_MAX_SIZE) {
        outputFile->close();
        delete outputFile;
        logNum++;
        logSize = 0;

        outputFile = new std::ofstream(fileBaseName + '-' + std::to_string(logNum) + '.' + fileExt,
                                       std::ofstream::out);
    }

#ifdef TESTING
    logCount.fetch_add(bufferMsgCount);
    bufferMsgCount = 0;
#endif
}

void FastLog::flush()
{
    // use a special (line # is -1) LogMsg to force a flush to disk.
    logMsg(Severity::DEBUG, std::move(""), std::move(""), -1);
}

#ifdef TESTING
unsigned int FastLog::getLogCount()
{
    return logCount.load();
}
#endif

std::string FastLog::FILE_NAME = "";
bool FastLog::STD_OUT = false;
bool FastLog::initialized = false;
unsigned int FastLog::BUFFER_SIZE = 0;
unsigned int FastLog::LOG_QUEUE_SIZE = 0;
unsigned int FastLog::LOG_FILE_MAX_SIZE = 0;
