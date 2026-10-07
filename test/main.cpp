#include "fastlog.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>
#include <iomanip>
#include <iostream>
#include <optional>
#include <regex>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

const std::string LOG_FILE_EXTENSION = ".log";
const std::string LOG_FILE_BASE_NAME = "test_output";
const bool ENABLE_STDOUT = false;

// Helper to wait until FastLog writes a specific target number of logs to file
bool wait_for_written_log_count(FastLog &logger, int targetCount, double timeoutSeconds = 15.0)
{
    auto startWait = std::chrono::high_resolution_clock::now();
    while (logger.getLogCount() < targetCount) {
        auto now = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(now - startWait).count();
        if (elapsed > timeoutSeconds) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    return true;
}

// waits for logCount in FastLog to stabilize and returns the count.
int getStableCount(FastLog &logger)
{
    int stableCount = logger.getLogCount();
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        int current = logger.getLogCount();
        if (current == stableCount) {
            break;
        }
        stableCount = current;
    }

    return stableCount;
}

// Validate that FastLog cannot be instantiated without constructor parameters (no uninitialized access)
TEST(FastLogTest, test_uninitialized_access)
{
    static_assert(!std::is_default_constructible_v<FastLog>,
                  "FastLog must require constructor arguments and not be default constructible");
    ASSERT_FALSE(std::is_default_constructible_v<FastLog>);
}

// Validate constructor initialization and creation of multiple FastLog instances
TEST(FastLogTest, test_initialization_and_reinitialization)
{
    ASSERT_NO_THROW({
        FastLog logger(LOG_FILE_BASE_NAME + LOG_FILE_EXTENSION, ENABLE_STDOUT);
    }) << "FastLog constructor should succeed without throwing";

    ASSERT_NO_THROW({
        FastLog anotherLogger("another_log.log", true);
    }) << "Creating another FastLog instance should succeed";
}

TEST(FastLogTest, test_fatal)
{
    FastLog logger(LOG_FILE_BASE_NAME + LOG_FILE_EXTENSION, ENABLE_STDOUT);
    ASSERT_DEATH(logger.fatal("Fatal exception, test will terminate."), "");
}

// Validate all standard severity logging methods and direct logMsg API
TEST(FastLogTest, test_all_severity_macros)
{
    FastLog logger(LOG_FILE_BASE_NAME + LOG_FILE_EXTENSION, ENABLE_STDOUT);
    ASSERT_NO_THROW(logger.debug("Testing debug method execution"))
        << "debug method should not throw";
    ASSERT_NO_THROW(logger.info("Testing info method execution"))
        << "info method should not throw";
    ASSERT_NO_THROW(logger.warning("Testing warning method execution"))
        << "warning method should not throw";
    ASSERT_NO_THROW(logger.critical("Testing critical method execution"))
        << "critical method should not throw";

    // Also validate named log<Level> aliases
    ASSERT_NO_THROW(logger.logDebug("Testing logDebug alias execution"));
    ASSERT_NO_THROW(logger.logInfo("Testing logInfo alias execution"));
    ASSERT_NO_THROW(logger.logWarning("Testing logWarning alias execution"));
    ASSERT_NO_THROW(logger.logCritical("Testing logCritical alias execution"));

    // Direct invocation via FastLog::logMsg
    ASSERT_NO_THROW(logger.logMsg(Severity::INFO,
                                  "Direct API invocation message",
                                  "custom.cpp",
                                  100))
        << "Direct logMsg method call should not throw";

    logger.flush();
}

// Validate buffering behavior of logger write-to-disk.
TEST(FastLogTest, test_buffer)
{
    FastLog logger(LOG_FILE_BASE_NAME + LOG_FILE_EXTENSION, ENABLE_STDOUT);
    const int stableCount = getStableCount(logger);

    // This should not be enough data to flush the log buffer.
    logger.info("don't flush me");

    const bool success = wait_for_written_log_count(logger, stableCount + 1, 0.1);
    ASSERT_TRUE(!success) << "timed out waiting to flush a single log message.";
}

// Validate flush functionality.
TEST(FastLogTest, test_flush)
{
    FastLog logger(LOG_FILE_BASE_NAME + LOG_FILE_EXTENSION, ENABLE_STDOUT);
    const int stableCount = getStableCount(logger);

    // This should not be enough data to flush the log buffer.
    logger.info("Flush me");

    // force flush
    logger.flush();

    const bool success = wait_for_written_log_count(logger, stableCount + 1);
    ASSERT_TRUE(success) << "timed out waiting to flush a single log message.";
}

// Validate handling of special characters, JSON formatting characters, and large payloads
TEST(FastLogTest, test_special_characters_and_payloads)
{
    FastLog logger(LOG_FILE_BASE_NAME + LOG_FILE_EXTENSION, ENABLE_STDOUT);
    ASSERT_NO_THROW(logger.info("JSON quotes: \"val\", backslashes: \\, slashes: /"))
        << "Quotes and backslashes should log cleanly";
    ASSERT_NO_THROW(logger.info("Formatting characters: \n\t\r\b\f"))
        << "Formatting characters should log cleanly";
    ASSERT_NO_THROW(
        logger.info("Embedded JSON: {\"nested\": {\"key\": \"value\", \"array\": [1, 2, 3]}}"))
        << "Embedded JSON string should log cleanly";
    ASSERT_NO_THROW(logger.info("UTF-8 unicode: \u2705 \U0001F680 \u4F60\u597D\u4E16\u754C"))
        << "UTF-8 Unicode string should log cleanly";

    // Large payload (8 KB)
    std::string largeMessage(8192, 'A');
    ASSERT_NO_THROW(logger.info(largeMessage)) << "Large 8KB payload message should log cleanly";

    // Empty message string
    ASSERT_NO_THROW(logger.info("")) << "Empty log message should log cleanly";

    logger.flush();
}

// Validate concurrent multi-threaded logging safety across multiple worker threads
TEST(FastLogTest, test_concurrent_multithreaded_logging)
{
    FastLog logger(LOG_FILE_BASE_NAME + LOG_FILE_EXTENSION, ENABLE_STDOUT);
    const unsigned int numThreads = std::max(4u, std::thread::hardware_concurrency());
    const int logsPerThread = 1000;
    std::vector<std::thread> threads;
    std::atomic<bool> startFlag{false};
    std::atomic<int> completedThreads{0};

    threads.reserve(numThreads);
    for (unsigned int t = 0; t < numThreads; ++t) {
        threads.emplace_back([t, logsPerThread, &startFlag, &completedThreads, &logger]() {
            while (!startFlag.load()) {
                std::this_thread::yield();
            }
            for (int i = 0; i < logsPerThread; ++i) {
                switch (i % 5) {
                    case 0: logger.debug("Thread " + std::to_string(t) + " debug " + std::to_string(i)); break;
                    case 1: logger.info("Thread " + std::to_string(t) + " info " + std::to_string(i)); break;
                    case 2: logger.warning("Thread " + std::to_string(t) + " warn " + std::to_string(i)); break;
                    case 3: logger.critical("Thread " + std::to_string(t) + " crit " + std::to_string(i)); break;
                }
            }
            completedThreads.fetch_add(1);
        });
    }

    startFlag.store(true);
    for (auto &t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    ASSERT_TRUE(completedThreads.load() == static_cast<int>(numThreads))
        << "All worker threads must finish logging successfully without hanging or deadlocking";

    logger.flush();
}

// Validate queue stability under high volume bursts
TEST(FastLogTest, test_high_volume_burst_logging)
{
    FastLog logger(LOG_FILE_BASE_NAME + LOG_FILE_EXTENSION, ENABLE_STDOUT);
    const int totalMessages = 25000;
    const unsigned int numThreads = std::max(2u, std::thread::hardware_concurrency());
    const int messagesPerThread = totalMessages / numThreads;
    std::vector<std::thread> threads;

    threads.reserve(numThreads);
    for (unsigned int t = 0; t < numThreads; ++t) {
        threads.emplace_back([t, messagesPerThread, &logger]() {
            for (int i = 0; i < messagesPerThread; ++i) {
                logger.info("Burst logging thread " + std::to_string(t) + " item " + std::to_string(i));
            }
        });
    }

    for (auto &t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    int remainder = totalMessages % numThreads;
    for (int i = 0; i < remainder; ++i) {
        logger.info("Burst logging remainder item " + std::to_string(i));
    }

    logger.flush();
}

// Performance test validating logging throughput and enqueue latency
TEST(FastLogTest, test_performance_throughput)
{
    FastLog logger(LOG_FILE_BASE_NAME + LOG_FILE_EXTENSION, ENABLE_STDOUT);
    const int benchmarkLogs = 20000;
    const unsigned int numThreads = std::max(2u, std::thread::hardware_concurrency());
    const int logsPerThread = benchmarkLogs / numThreads;
    std::vector<std::thread> threads;
    std::atomic<bool> startFlag{false};

    threads.reserve(numThreads);
    for (unsigned int t = 0; t < numThreads; ++t) {
        threads.emplace_back([t, logsPerThread, &startFlag, &logger]() {
            while (!startFlag.load()) {
                std::this_thread::yield();
            }
            for (int i = 0; i < logsPerThread; ++i) {
                logger.info("Perf benchmark thread " + std::to_string(t) + " message #" + std::to_string(i));
            }
        });
    }

    auto start = std::chrono::high_resolution_clock::now();
    startFlag.store(true);

    for (auto &t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsedSeconds = std::chrono::duration<double>(end - start).count();
    double throughput = static_cast<double>(benchmarkLogs)
                        / (elapsedSeconds > 0 ? elapsedSeconds : 0.0001);
    double avgLatencyMicros = (elapsedSeconds * 1e6) / static_cast<double>(benchmarkLogs);

    std::cout << "\n   -> Enqueued " << benchmarkLogs << " logs in " << std::fixed
              << std::setprecision(4) << elapsedSeconds << " s " << "(" << std::fixed
              << std::setprecision(0) << throughput << " msgs/sec, " << std::fixed
              << std::setprecision(2) << avgLatencyMicros << " us/msg avg latency) ...\n\n";

    // Performance validations:
    // 1. All 20,000 logs must be enqueued across threads in under 5.0 seconds
    ASSERT_TRUE(elapsedSeconds < 5.0)
        << "Performance threshold failed: benchmark took 5.0 seconds or more";
    // 2. Minimum throughput should exceed 1,000 messages/second
    ASSERT_TRUE(throughput >= 1000.0)
        << "Throughput threshold failed: throughput was below 1,000 msgs/sec";

    logger.flush();
}

// Performance test validating disk/file write throughput and drain latency using getLogCount()
TEST(FastLogTest, test_file_write_throughput)
{
    FastLog logger(LOG_FILE_BASE_NAME + LOG_FILE_EXTENSION, ENABLE_STDOUT);
    const int startLogCount = getStableCount(logger);

    const int testLogs = 5000;
    const unsigned int numThreads = std::max(2u, std::thread::hardware_concurrency());
    const int logsPerThread = testLogs / numThreads;
    const int expectedFinalCount = startLogCount + testLogs;

    std::vector<std::thread> threads;
    std::atomic<bool> startFlag{false};

    threads.reserve(numThreads);
    for (unsigned int t = 0; t < numThreads; ++t) {
        threads.emplace_back([t, logsPerThread, &startFlag, &logger]() {
            while (!startFlag.load()) {
                std::this_thread::yield();
            }
            for (int i = 0; i < logsPerThread; ++i) {
                logger.info("File write perf thread " + std::to_string(t) + " msg #" + std::to_string(i));
            }
        });
    }

    auto start = std::chrono::high_resolution_clock::now();
    startFlag.store(true);

    for (auto &t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    // flush the logger.
    logger.flush();

    // Wait until background writer thread has formatted and written all messages to the file
    bool completed = wait_for_written_log_count(logger, expectedFinalCount, 15.0);
    auto end = std::chrono::high_resolution_clock::now();

    ASSERT_TRUE(completed)
        << "Timed out waiting for FastLog background writer to flush all logs to disk";

    int finalLogCount = logger.getLogCount();
    int actualLogsWritten = finalLogCount - startLogCount;
    ASSERT_TRUE(actualLogsWritten == testLogs)
        << "Log count mismatch: expected " + std::to_string(testLogs) + " logs written, but got "
               + std::to_string(actualLogsWritten);

    double elapsedSeconds = std::chrono::duration<double>(end - start).count();
    double writeThroughput = static_cast<double>(actualLogsWritten) / (elapsedSeconds > 0 ? elapsedSeconds : 0.0001);
    double avgLatencyMicros = (elapsedSeconds * 1e6) / static_cast<double>(actualLogsWritten);

    std::cout << "\n   -> Wrote " << actualLogsWritten << " logs to file in " << std::fixed
              << std::setprecision(4) << elapsedSeconds << " s " << "(" << std::fixed
              << std::setprecision(0) << writeThroughput << " logs/sec, " << std::fixed
              << std::setprecision(2) << avgLatencyMicros << " us/log avg write latency) ...\n\n";

    // Performance assertions against baseline:
    // Baseline: Disk write throughput must exceed 500 logs/second (or under 2000 us/log)
    ASSERT_TRUE(writeThroughput >= 500.0)
        << "File write throughput was below baseline of 500 logs/sec";
    // Maximum allowable time: all 5000 logs written in under 10 seconds
    ASSERT_TRUE(elapsedSeconds < 10.0) << "File writing took longer than the 10.0 second threshold";
}

std::optional<unsigned int> maxLogNumber(const std::string &baseName)
{
    // Get the curr dir
    std::filesystem::path dir = std::filesystem::current_path();

    std::string pattern = baseName + R"(-(\d+)\.log)";
    std::regex re(pattern);

    long maxNum = -1;

    for (const auto &entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file())
            continue;

        std::string filename = entry.path().filename().string();

        std::smatch match;
        if (std::regex_match(filename, match, re)) {
            long num = std::stol(match[1].str());
            if (num > maxNum)
                maxNum = num;
        }
    }

    return maxNum >= 0 ? std::optional<long>(maxNum) : std::nullopt;
}

// Ensure a new log file is created once the set max size is reached.
TEST(FastLogTest, test_file_rotation)
{
    const unsigned int maxFileSize = (1 << 22); // 4 MB threshold to trigger rotation
    FastLog logger(LOG_FILE_BASE_NAME + LOG_FILE_EXTENSION,
                   ENABLE_STDOUT,
                   DEFAULT_BUFFER_SIZE,
                   DEFAULT_LOG_QUEUE_SIZE,
                   maxFileSize);

    std::optional<unsigned int> maxLogNumBefore = maxLogNumber(LOG_FILE_BASE_NAME);

    ASSERT_TRUE(maxLogNumBefore != std::nullopt);

    // this msg size should be enough to force a file rotation.
    unsigned int msgSize = (1 << 23); // 8 MB message exceeds 4 MB maxFileSize
    logger.info(std::string(msgSize, 'a'));

    // The next log msg should go into a new file.
    logger.info("new file");
    logger.flush();

    // sleep for some time to allow background thread to process.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::optional<unsigned int> maxLogNumAfter = maxLogNumber(LOG_FILE_BASE_NAME);
    ASSERT_TRUE(maxLogNumAfter != std::nullopt);

    ASSERT_TRUE(maxLogNumAfter > maxLogNumBefore);
}

// Ensure blocking behavior takes a minimum amount of time to complete
// in the case of a full queue.
TEST(FastLogTest, test_blocking)
{
    // set queue size to 1, which is VERY SMALL.
    unsigned int logQueueSize = 1;
    unsigned int numMessages = 10;

    FastLog logger(LOG_FILE_BASE_NAME + LOG_FILE_EXTENSION,
                   ENABLE_STDOUT,
                   DEFAULT_BUFFER_SIZE,
                   logQueueSize,
                   DEFAULT_LOG_FILE_SIZE,
                   true);

    auto start = std::chrono::high_resolution_clock::now();
    for (auto i = 0; i < numMessages; i++) {
        logger.info("test blocking");
    }
    auto end = std::chrono::high_resolution_clock::now();

    std::cout << "\n   -> Enqueuing " << numMessages << " msgs with queue size == " << logQueueSize << " took "
              << std::chrono::duration_cast<std::chrono::microseconds>(end - start) << "\n\n";
    ASSERT_TRUE((end - start) >= DEFAULT_BLOCKING_TIME);
}
