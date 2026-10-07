#include "fastlog.h"
#include <chrono>
#include <fstream>
#include <iostream>

// Constructor for LogMsg struct.
LogMsg::LogMsg(const Severity level,
               std::string &&_msg,
               std::string &&_source,
               std::chrono::local_time<std::chrono::system_clock::duration> &&timestamp,
               const unsigned int line)
    : level(level)
    , msg(std::move(_msg))
    , source(std::move(_source))
    , timestamp(timestamp)
    , line(line)
{}

/**
 * @brief FastLog::FastLog Public constructor
 */
FastLog::FastLog(std::string fileName,
                 bool stdOut,
                 unsigned int bufferSize,
                 unsigned int queueSize,
                 unsigned int logFileMaxSize,
                 bool blocking)
    : finished(false)
    , messages(queueSize)
    , outputFile(nullptr)
    , fileName(std::move(fileName))
    , stdOut(stdOut)
    , bufferSize(bufferSize)
    , logQueueSize(queueSize)
    , logFileMaxSize(logFileMaxSize)
    , blocking(blocking)
    , logSize(0)
    , logNum(0)
#ifdef TESTING
    , logCount(0)
    , bufferMsgCount(0)
#endif
{
    size_t dotIdx = this->fileName.rfind('.');
    if (dotIdx != std::string::npos) {
        fileBaseName = this->fileName.substr(0, dotIdx);
        fileExt = this->fileName.substr(dotIdx + 1);
    } else {
        fileBaseName = this->fileName;
        fileExt = "log";
    }

    outputFile = new std::ofstream(fileBaseName + '-' + std::to_string(logNum) + '.' + fileExt,
                                   std::ofstream::out);
    if (!outputFile->is_open()) {
        std::cout << "Failed to open log file for writing in constructor" << std::endl;
        return;
    }

    writeBuffer.reserve(this->bufferSize);
    writer = std::thread([this] { this->writeLoop(); });
}

FastLog::~FastLog()
{
    finished.store(true);

    if (writer.joinable()) {
        writer.join();
    }

    if (outputFile) {
        if (outputFile->is_open()) {
            flushBuffer();
            outputFile->close();
        }
        delete outputFile;
        outputFile = nullptr;
    }
}

// used to log messages to stdout / file
void FastLog::logMsg(const Severity level,
                     std::string msg,
                     std::string source,
                     const unsigned int line)
{
    if (finished.load()) {
        std::cout << "Attempting to log a message after logger deleted." << std::endl;
        return;
    }

    auto timePoint = std::chrono::current_zone()->to_local(std::chrono::system_clock::now());

    if (!blocking) {
        messages.push(LogMsg(level, std::move(msg), std::move(source), std::move(timePoint), line));
    } else {
        bool succ = false;
        std::chrono::nanoseconds blockingTime = DEFAULT_BLOCKING_TIME;
        while (!succ) {
            succ = messages.push(
                LogMsg(level, std::move(msg), std::move(source), std::move(timePoint), line));

            std::this_thread::sleep_for(blockingTime);
            blockingTime *= 2;
        }
    }
}

void FastLog::info(std::string msg, const std::source_location loc)
{
    logMsg(Severity::INFO, std::move(msg), std::string(loc.file_name()), loc.line());
}

void FastLog::debug(std::string msg, const std::source_location loc)
{
    logMsg(Severity::DEBUG, std::move(msg), std::string(loc.file_name()), loc.line());
}

void FastLog::warning(std::string msg, const std::source_location loc)
{
    logMsg(Severity::WARNING, std::move(msg), std::string(loc.file_name()), loc.line());
}

void FastLog::critical(std::string msg, const std::source_location loc)
{
    logMsg(Severity::CRITICAL, std::move(msg), std::string(loc.file_name()), loc.line());
}

[[noreturn]] void FastLog::fatal(std::string msg, const std::source_location loc)
{
    logMsg(Severity::FATAL, std::move(msg), std::string(loc.file_name()), loc.line());
    std::exit(-1);
}

void FastLog::info(std::string msg, std::string source, const unsigned int line)
{
    logMsg(Severity::INFO, std::move(msg), std::move(source), line);
}

void FastLog::debug(std::string msg, std::string source, const unsigned int line)
{
    logMsg(Severity::DEBUG, std::move(msg), std::move(source), line);
}

void FastLog::warning(std::string msg, std::string source, const unsigned int line)
{
    logMsg(Severity::WARNING, std::move(msg), std::move(source), line);
}

void FastLog::critical(std::string msg, std::string source, const unsigned int line)
{
    logMsg(Severity::CRITICAL, std::move(msg), std::move(source), line);
}

[[noreturn]] void FastLog::fatal(std::string msg, std::string source, const unsigned int line)
{
    logMsg(Severity::FATAL, std::move(msg), std::move(source), line);
    std::exit(-1);
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

        if (writeBuffer.size() >= bufferSize) {
            flushBuffer();
        }

        // Process msg
        if (stdOut) {
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
    if (!outputFile || !outputFile->is_open()) {
        std::cout << "Failed to open log file for writing" << std::endl;
        return;
    }
    *outputFile << writeBuffer;
    logSize += writeBuffer.size();
    writeBuffer.clear();

    // rotate log file if needed
    if (logSize >= logFileMaxSize) {
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
    logMsg(Severity::DEBUG, "", "", -1);
}

#ifdef TESTING
unsigned int FastLog::getLogCount()
{
    return logCount.load();
}
#endif
