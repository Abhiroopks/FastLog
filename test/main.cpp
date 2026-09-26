#include "fastlog.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <gtest/gtest.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

const std::string LOG_FILE_NAME = "test_output.log";
const bool ENABLE_STDOUT = false;

// Helper to wait until FastLog writes a specific target number of logs to file
bool wait_for_written_log_count(int targetCount, double timeoutSeconds = 15.0)
{
    auto startWait = std::chrono::high_resolution_clock::now();
    while (FastLog::getInstance().getLogCount() < targetCount) {
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
int getStableCount()
{
    int stableCount = FastLog::getInstance().getLogCount();
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        int current = FastLog::getInstance().getLogCount();
        if (current == stableCount) {
            break;
        }
        stableCount = current;
    }

    return stableCount;
}

// Validate uninitialized FastLog::getInstance() throws std::runtime_error
TEST(FastLogTest, test_uninitialized_access)
{
    ASSERT_THROW(FastLog::getInstance(), std::runtime_error)
        << "FastLog::getInstance() must throw std::runtime_error when called before initialization";
}

// Validate initialization and idempotency of subsequent initialize() calls
TEST(FastLogTest, test_initialization_and_reinitialization)
{
    ASSERT_NO_THROW(FastLog::initialize(LOG_FILE_NAME, ENABLE_STDOUT))
        << "FastLog::initialize should succeed without throwing";
    ASSERT_NO_THROW(FastLog::getInstance())
        << "FastLog::getInstance() should succeed after initialization";
    // Verify that re-initialization is safely ignored and does not crash or throw
    ASSERT_NO_THROW(FastLog::initialize("another_log.log", true))
        << "Subsequent calls to FastLog::initialize should be safely ignored";
}

// Validate LogMsg struct member initialization and field correctness
TEST(FastLogTest, test_log_msg_structure)
{
    LogMsg msg1(Severity::INFO, "Test message content", "test_source.cpp", "20-08-2026 12:00:00", 42);
    ASSERT_TRUE(msg1.level == Severity::INFO) << "LogMsg level should match constructor argument";
    ASSERT_TRUE(msg1.msg == "Test message content")
        << "LogMsg msg should match constructor argument";
    ASSERT_TRUE(msg1.source == "test_source.cpp")
        << "LogMsg source should match constructor argument";
    ASSERT_TRUE(msg1.timestamp == "20-08-2026 12:00:00")
        << "LogMsg timestamp should match constructor argument";
    ASSERT_TRUE(msg1.line == 42) << "LogMsg line number should match constructor argument";

    // Edge case: Empty strings and zero line number
    LogMsg msg2(Severity::INFO, "", "", "", 0);
    ASSERT_TRUE(msg2.msg.empty()) << "LogMsg msg should allow empty string";
    ASSERT_TRUE(msg2.source.empty()) << "LogMsg source should allow empty string";
    ASSERT_TRUE(msg2.timestamp.empty()) << "LogMsg timestamp should allow empty string";
    ASSERT_TRUE(msg2.line == 0) << "LogMsg line should allow 0";
}

// Validate all standard severity logging macros and direct logMsg API
TEST(FastLogTest, test_all_severity_macros)
{
    ASSERT_NO_THROW(LOG_DEBUG("Testing LOG_DEBUG macro execution"))
        << "LOG_DEBUG macro should not throw";
    ASSERT_NO_THROW(LOG_INFO("Testing LOG_INFO macro execution"))
        << "LOG_INFO macro should not throw";
    ASSERT_NO_THROW(LOG_WARNING("Testing LOG_WARNING macro execution"))
        << "LOG_WARNING macro should not throw";
    ASSERT_NO_THROW(LOG_CRITICAL("Testing LOG_CRITICAL macro execution"))
        << "LOG_CRITICAL macro should not throw";

    // TODO: Use gtest DEATH_TEST to validate the fatal log.
    // ASSERT_NO_THROW(LOG_FATAL("Testing LOG_FATAL macro execution"), "LOG_FATAL macro should not throw");

    // Direct invocation via FastLog::logMsg
    ASSERT_NO_THROW(FastLog::getInstance().logMsg(Severity::INFO,
                                                  "Direct API invocation message",
                                                  "custom.cpp",
                                                  100))
        << "Direct logMsg method call should not throw";

    FastLog::getInstance().flush();
}

// Validate buffering behavior of logger write-to-disk.
TEST(FastLogTest, test_buffer)
{
    const int stableCount = getStableCount();

    // This should not be enough data to flush the log buffer.
    LOG_INFO("don't flush me");

    const bool success = wait_for_written_log_count(stableCount + 1, 0.1);
    ASSERT_TRUE(!success) << "timed out waiting to flush a single log message.";
}

// Validate flush functionality.
TEST(FastLogTest, test_flush)
{
    const int stableCount = getStableCount();

    // This should not be enough data to flush the log buffer.
    LOG_INFO("Flush me");

    // force flush
    FastLog::getInstance().flush();

    const bool success = wait_for_written_log_count(stableCount + 1);
    ASSERT_TRUE(success) << "timed out waiting to flush a single log message.";
}

// Validate handling of special characters, JSON formatting characters, and large payloads
TEST(FastLogTest, test_special_characters_and_payloads)
{
    ASSERT_NO_THROW(LOG_INFO("JSON quotes: \"val\", backslashes: \\, slashes: /"))
        << "Quotes and backslashes should log cleanly";
    ASSERT_NO_THROW(LOG_INFO("Formatting characters: \n\t\r\b\f"))
        << "Formatting characters should log cleanly";
    ASSERT_NO_THROW(
        LOG_INFO("Embedded JSON: {\"nested\": {\"key\": \"value\", \"array\": [1, 2, 3]}}"))
        << "Embedded JSON string should log cleanly";
    ASSERT_NO_THROW(LOG_INFO("UTF-8 unicode: \u2705 \U0001F680 \u4F60\u597D\u4E16\u754C"))
        << "UTF-8 Unicode string should log cleanly";

    // Large payload (8 KB)
    std::string largeMessage(8192, 'A');
    ASSERT_NO_THROW(LOG_INFO(largeMessage)) << "Large 8KB payload message should log cleanly";

    // Empty message string
    ASSERT_NO_THROW(LOG_INFO("")) << "Empty log message should log cleanly";

    FastLog::getInstance().flush();
}

// Validate concurrent multi-threaded logging safety across multiple worker threads
TEST(FastLogTest, test_concurrent_multithreaded_logging)
{
    const unsigned int numThreads = std::max(4u, std::thread::hardware_concurrency());
    const int logsPerThread = 1000;
    std::vector<std::thread> threads;
    std::atomic<bool> startFlag{false};
    std::atomic<int> completedThreads{0};

    threads.reserve(numThreads);
    for (unsigned int t = 0; t < numThreads; ++t) {
        threads.emplace_back([t, logsPerThread, &startFlag, &completedThreads]() {
            while (!startFlag.load()) {
                std::this_thread::yield();
            }
            for (int i = 0; i < logsPerThread; ++i) {
                switch (i % 5) {
                    case 0: LOG_DEBUG("Thread " + std::to_string(t) + " debug " + std::to_string(i)); break;
                    case 1: LOG_INFO("Thread " + std::to_string(t) + " info " + std::to_string(i)); break;
                    case 2: LOG_WARNING("Thread " + std::to_string(t) + " warn " + std::to_string(i)); break;
                    case 3: LOG_CRITICAL("Thread " + std::to_string(t) + " crit " + std::to_string(i)); break;
                        // case 4: LOG_FATAL("Thread " + std::to_string(t) + " fatal " + std::to_string(i)); break;
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

    FastLog::getInstance().flush();
}

// Validate queue stability under high volume bursts
TEST(FastLogTest, test_high_volume_burst_logging)
{
    const int totalMessages = 25000;
    const unsigned int numThreads = std::max(2u, std::thread::hardware_concurrency());
    const int messagesPerThread = totalMessages / numThreads;
    std::vector<std::thread> threads;

    threads.reserve(numThreads);
    for (unsigned int t = 0; t < numThreads; ++t) {
        threads.emplace_back([t, messagesPerThread]() {
            for (int i = 0; i < messagesPerThread; ++i) {
                LOG_INFO("Burst logging thread " + std::to_string(t) + " item " + std::to_string(i));
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
        LOG_INFO("Burst logging remainder item " + std::to_string(i));
    }

    FastLog::getInstance().flush();
}

// Performance test validating logging throughput and enqueue latency
TEST(FastLogTest, test_performance_throughput)
{
    const int benchmarkLogs = 20000;
    const unsigned int numThreads = std::max(2u, std::thread::hardware_concurrency());
    const int logsPerThread = benchmarkLogs / numThreads;
    std::vector<std::thread> threads;
    std::atomic<bool> startFlag{false};

    threads.reserve(numThreads);
    for (unsigned int t = 0; t < numThreads; ++t) {
        threads.emplace_back([t, logsPerThread, &startFlag]() {
            while (!startFlag.load()) {
                std::this_thread::yield();
            }
            for (int i = 0; i < logsPerThread; ++i) {
                LOG_INFO("Perf benchmark thread " + std::to_string(t) + " message #" + std::to_string(i));
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

    std::cout << "\n      -> Enqueued " << benchmarkLogs << " logs in " << std::fixed
              << std::setprecision(4) << elapsedSeconds << " s " << "(" << std::fixed
              << std::setprecision(0) << throughput << " msgs/sec, " << std::fixed
              << std::setprecision(2) << avgLatencyMicros << " us/msg avg latency) ... ";

    // Performance validations:
    // 1. All 20,000 logs must be enqueued across threads in under 5.0 seconds
    ASSERT_TRUE(elapsedSeconds < 5.0)
        << "Performance threshold failed: benchmark took 5.0 seconds or more";
    // 2. Minimum throughput should exceed 1,000 messages/second
    ASSERT_TRUE(throughput >= 1000.0)
        << "Throughput threshold failed: throughput was below 1,000 msgs/sec";

    FastLog::getInstance().flush();
}

// Performance test validating disk/file write throughput and drain latency using getLogCount()
TEST(FastLogTest, test_file_write_throughput)
{

    const int startLogCount = getStableCount();

    const int testLogs = 5000;
    const unsigned int numThreads = std::max(2u, std::thread::hardware_concurrency());
    const int logsPerThread = testLogs / numThreads;
    const int expectedFinalCount = startLogCount + testLogs;

    std::vector<std::thread> threads;
    std::atomic<bool> startFlag{false};

    threads.reserve(numThreads);
    for (unsigned int t = 0; t < numThreads; ++t) {
        threads.emplace_back([t, logsPerThread, &startFlag]() {
            while (!startFlag.load()) {
                std::this_thread::yield();
            }
            for (int i = 0; i < logsPerThread; ++i) {
                LOG_INFO("File write perf thread " + std::to_string(t) + " msg #" + std::to_string(i));
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
    FastLog::getInstance().flush();

    // Wait until background writer thread has formatted and written all messages to the file
    bool completed = wait_for_written_log_count(expectedFinalCount, 15.0);
    auto end = std::chrono::high_resolution_clock::now();

    ASSERT_TRUE(completed)
        << "Timed out waiting for FastLog background writer to flush all logs to disk";

    int finalLogCount = FastLog::getInstance().getLogCount();
    int actualLogsWritten = finalLogCount - startLogCount;
    ASSERT_TRUE(actualLogsWritten == testLogs)
        << "Log count mismatch: expected " + std::to_string(testLogs) + " logs written, but got "
               + std::to_string(actualLogsWritten);

    double elapsedSeconds = std::chrono::duration<double>(end - start).count();
    double writeThroughput = static_cast<double>(actualLogsWritten) / (elapsedSeconds > 0 ? elapsedSeconds : 0.0001);
    double avgLatencyMicros = (elapsedSeconds * 1e6) / static_cast<double>(actualLogsWritten);

    std::cout << "\n      -> Wrote " << actualLogsWritten << " logs to file in "
              << std::fixed << std::setprecision(4) << elapsedSeconds << " s "
              << "(" << std::fixed << std::setprecision(0) << writeThroughput << " logs/sec, "
              << std::fixed << std::setprecision(2) << avgLatencyMicros << " us/log avg write latency) ... ";

    // Performance assertions against baseline:
    // Baseline: Disk write throughput must exceed 500 logs/second (or under 2000 us/log)
    ASSERT_TRUE(writeThroughput >= 500.0)
        << "File write throughput was below baseline of 500 logs/sec";
    // Maximum allowable time: all 5000 logs written in under 10 seconds
    ASSERT_TRUE(elapsedSeconds < 10.0) << "File writing took longer than the 10.0 second threshold";
}
