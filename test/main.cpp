#include "fastlog.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

const std::string LOG_FILE_NAME = "test_output.log";
const bool ENABLE_STDOUT = false;

// Custom exception for test assertion failures
struct TestFailureException : public std::exception
{
    std::string message;
    explicit TestFailureException(std::string msg) : message(std::move(msg)) {}
    const char *what() const noexcept override { return message.c_str(); }
};

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::ostringstream _oss; \
            _oss << "Assertion failed: (" #cond ") - " << (msg) << " [" << __FILE__ << ":" << __LINE__ << "]"; \
            throw TestFailureException(_oss.str()); \
        } \
    } while (0)

#define TEST_ASSERT_THROWS(expr, ExceptionType, msg) \
    do { \
        bool _caught = false; \
        try { \
            expr; \
        } catch (const ExceptionType &) { \
            _caught = true; \
        } catch (const std::exception &_e) { \
            std::ostringstream _oss; \
            _oss << "Expected exception " #ExceptionType " but caught std::exception (" << _e.what() << "): " << (msg) << " [" << __FILE__ << ":" << __LINE__ << "]"; \
            throw TestFailureException(_oss.str()); \
        } catch (...) { \
            std::ostringstream _oss; \
            _oss << "Expected exception " #ExceptionType " but caught unknown exception: " << (msg) << " [" << __FILE__ << ":" << __LINE__ << "]"; \
            throw TestFailureException(_oss.str()); \
        } \
        if (!_caught) { \
            std::ostringstream _oss; \
            _oss << "Expected exception " #ExceptionType " was not thrown: " << (msg) << " [" << __FILE__ << ":" << __LINE__ << "]"; \
            throw TestFailureException(_oss.str()); \
        } \
    } while (0)

#define TEST_ASSERT_NO_THROW(expr, msg) \
    do { \
        try { \
            expr; \
        } catch (const std::exception &_e) { \
            std::ostringstream _oss; \
            _oss << "Unexpected exception (" << _e.what() << "): " << (msg) << " [" << __FILE__ << ":" << __LINE__ << "]"; \
            throw TestFailureException(_oss.str()); \
        } catch (...) { \
            std::ostringstream _oss; \
            _oss << "Unexpected non-standard exception: " << (msg) << " [" << __FILE__ << ":" << __LINE__ << "]"; \
            throw TestFailureException(_oss.str()); \
        } \
    } while (0)

// Test 1: Validate uninitialized FastLog::getInstance() throws std::runtime_error
void test_uninitialized_access()
{
    TEST_ASSERT_THROWS(FastLog::getInstance(), std::runtime_error,
                       "FastLog::getInstance() must throw std::runtime_error when called before initialization");
}

// Test 2: Validate initialization and idempotency of subsequent initialize() calls
void test_initialization_and_reinitialization()
{
    TEST_ASSERT_NO_THROW(FastLog::initialize(LOG_FILE_NAME, ENABLE_STDOUT),
                         "FastLog::initialize should succeed without throwing");
    TEST_ASSERT_NO_THROW(FastLog::getInstance(),
                         "FastLog::getInstance() should succeed after initialization");
    // Verify that re-initialization is safely ignored and does not crash or throw
    TEST_ASSERT_NO_THROW(FastLog::initialize("another_log.log", true),
                         "Subsequent calls to FastLog::initialize should be safely ignored");
}

// Test 3: Validate LogMsg struct member initialization and field correctness
void test_log_msg_structure()
{
    LogMsg msg1("INFO", "Test message content", "test_source.cpp", "20-08-2026 12:00:00", 42);
    TEST_ASSERT(msg1.level == "INFO", "LogMsg level should match constructor argument");
    TEST_ASSERT(msg1.msg == "Test message content", "LogMsg msg should match constructor argument");
    TEST_ASSERT(msg1.source == "test_source.cpp", "LogMsg source should match constructor argument");
    TEST_ASSERT(msg1.timestamp == "20-08-2026 12:00:00", "LogMsg timestamp should match constructor argument");
    TEST_ASSERT(msg1.line == 42, "LogMsg line number should match constructor argument");

    // Edge case: Empty strings and zero line number
    LogMsg msg2("", "", "", "", 0);
    TEST_ASSERT(msg2.level.empty(), "LogMsg level should allow empty string");
    TEST_ASSERT(msg2.msg.empty(), "LogMsg msg should allow empty string");
    TEST_ASSERT(msg2.source.empty(), "LogMsg source should allow empty string");
    TEST_ASSERT(msg2.timestamp.empty(), "LogMsg timestamp should allow empty string");
    TEST_ASSERT(msg2.line == 0, "LogMsg line should allow 0");
}

// Test 4: Validate all standard severity logging macros and direct logMsg API
void test_all_severity_macros()
{
    TEST_ASSERT_NO_THROW(LOG_DEBUG("Testing LOG_DEBUG macro execution"), "LOG_DEBUG macro should not throw");
    TEST_ASSERT_NO_THROW(LOG_INFO("Testing LOG_INFO macro execution"), "LOG_INFO macro should not throw");
    TEST_ASSERT_NO_THROW(LOG_WARNING("Testing LOG_WARNING macro execution"), "LOG_WARNING macro should not throw");
    TEST_ASSERT_NO_THROW(LOG_CRITICAL("Testing LOG_CRITICAL macro execution"), "LOG_CRITICAL macro should not throw");
    TEST_ASSERT_NO_THROW(LOG_FATAL("Testing LOG_FATAL macro execution"), "LOG_FATAL macro should not throw");

    // Direct invocation via FastLog::logMsg
    TEST_ASSERT_NO_THROW(FastLog::getInstance().logMsg("CUSTOM", "Direct API invocation message", "custom.cpp", 100),
                         "Direct logMsg method call should not throw");

    FastLog::getInstance().flush();
}

// Test 5: Validate handling of special characters, JSON formatting characters, and large payloads
void test_special_characters_and_payloads()
{
    TEST_ASSERT_NO_THROW(LOG_INFO("JSON quotes: \"val\", backslashes: \\, slashes: /"),
                         "Quotes and backslashes should log cleanly");
    TEST_ASSERT_NO_THROW(LOG_INFO("Formatting characters: \n\t\r\b\f"),
                         "Formatting characters should log cleanly");
    TEST_ASSERT_NO_THROW(LOG_INFO("Embedded JSON: {\"nested\": {\"key\": \"value\", \"array\": [1, 2, 3]}}"),
                         "Embedded JSON string should log cleanly");
    TEST_ASSERT_NO_THROW(LOG_INFO("UTF-8 unicode: \u2705 \U0001F680 \u4F60\u597D\u4E16\u754C"),
                         "UTF-8 Unicode string should log cleanly");

    // Large payload (8 KB)
    std::string largeMessage(8192, 'A');
    TEST_ASSERT_NO_THROW(LOG_INFO(largeMessage), "Large 8KB payload message should log cleanly");

    // Empty message string
    TEST_ASSERT_NO_THROW(LOG_INFO(""), "Empty log message should log cleanly");

    FastLog::getInstance().flush();
}

// Test 6: Validate concurrent multi-threaded logging safety across multiple worker threads
void test_concurrent_multithreaded_logging()
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
                    case 4: LOG_FATAL("Thread " + std::to_string(t) + " fatal " + std::to_string(i)); break;
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

    TEST_ASSERT(completedThreads.load() == static_cast<int>(numThreads),
                "All worker threads must finish logging successfully without hanging or deadlocking");

    FastLog::getInstance().flush();
}

// Test 7: Validate queue stability under high volume bursts
void test_high_volume_burst_logging()
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

// Test 8: Performance test validating logging throughput and enqueue latency
void test_performance_throughput()
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
    TEST_ASSERT(elapsedSeconds < 5.0, "Performance threshold failed: benchmark took 5.0 seconds or more");
    // 2. Minimum throughput should exceed 1,000 messages/second
    TEST_ASSERT(throughput >= 1000.0, "Throughput threshold failed: throughput was below 1,000 msgs/sec");

    FastLog::getInstance().flush();
}

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

// Test 9: Performance test validating disk/file write throughput and drain latency using getLogCount()
void test_file_write_throughput()
{
    // First ensure any pending log messages from prior tests have been completely written to file
    // by waiting for the log count to stabilize.
    int stableCount = FastLog::getInstance().getLogCount();
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        int current = FastLog::getInstance().getLogCount();
        if (current == stableCount) {
            break;
        }
        stableCount = current;
    }

    const int testLogs = 20000;
    const unsigned int numThreads = std::max(2u, std::thread::hardware_concurrency());
    const int logsPerThread = testLogs / numThreads;
    const int startLogCount = FastLog::getInstance().getLogCount();
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

    TEST_ASSERT(completed, "Timed out waiting for FastLog background writer to flush all logs to disk");

    int finalLogCount = FastLog::getInstance().getLogCount();
    int actualLogsWritten = finalLogCount - startLogCount;
    TEST_ASSERT(actualLogsWritten == testLogs,
                "Log count mismatch: expected " + std::to_string(testLogs)
                    + " logs written, but got " + std::to_string(actualLogsWritten));

    double elapsedSeconds = std::chrono::duration<double>(end - start).count();
    double writeThroughput = static_cast<double>(actualLogsWritten) / (elapsedSeconds > 0 ? elapsedSeconds : 0.0001);
    double avgLatencyMicros = (elapsedSeconds * 1e6) / static_cast<double>(actualLogsWritten);

    std::cout << "\n      -> Wrote " << actualLogsWritten << " logs to file in "
              << std::fixed << std::setprecision(4) << elapsedSeconds << " s "
              << "(" << std::fixed << std::setprecision(0) << writeThroughput << " logs/sec, "
              << std::fixed << std::setprecision(2) << avgLatencyMicros << " us/log avg write latency) ... ";

    // Performance assertions against baseline:
    // Baseline: Disk write throughput must exceed 500 logs/second (or under 2000 us/log)
    TEST_ASSERT(writeThroughput >= 500.0, "File write throughput was below baseline of 500 logs/sec");
    // Maximum allowable time: all 5000 logs written in under 10 seconds
    TEST_ASSERT(elapsedSeconds < 10.0, "File writing took longer than the 10.0 second threshold");
}

// Test runner infrastructure
struct TestResult
{
    std::string name;
    bool passed;
    double durationMs;
    std::string failureDetails;
};

class TestSuite
{
public:
    using TestFn = std::function<void()>;

    void addTest(const std::string &name, TestFn fn)
    {
        tests.push_back({name, fn});
    }

    int runAll()
    {
        std::cout << "========================================================" << std::endl;
        std::cout << "               FastLog Unit Test Suite                  " << std::endl;
        std::cout << "========================================================" << std::endl;

        std::vector<TestResult> results;
        int passedCount = 0;
        int failedCount = 0;
        auto suiteStart = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < tests.size(); ++i) {
            const auto &t = tests[i];
            std::cout << "[" << (i + 1) << "/" << tests.size() << "] " << t.name << " ... " << std::flush;
            auto start = std::chrono::high_resolution_clock::now();
            TestResult res;
            res.name = t.name;
            try {
                t.fn();
                auto end = std::chrono::high_resolution_clock::now();
                res.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
                res.passed = true;
                passedCount++;
                std::cout << "[PASSED] (" << std::fixed << std::setprecision(2) << res.durationMs << " ms)" << std::endl;
            } catch (const TestFailureException &e) {
                auto end = std::chrono::high_resolution_clock::now();
                res.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
                res.passed = false;
                res.failureDetails = e.what();
                failedCount++;
                std::cout << "[FAILED] (" << std::fixed << std::setprecision(2) << res.durationMs << " ms)" << std::endl;
                std::cout << "    Error: " << e.what() << std::endl;
            } catch (const std::exception &e) {
                auto end = std::chrono::high_resolution_clock::now();
                res.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
                res.passed = false;
                res.failureDetails = std::string("Unhandled exception: ") + e.what();
                failedCount++;
                std::cout << "[FAILED] (" << std::fixed << std::setprecision(2) << res.durationMs << " ms)" << std::endl;
                std::cout << "    Unhandled exception: " << e.what() << std::endl;
            } catch (...) {
                auto end = std::chrono::high_resolution_clock::now();
                res.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
                res.passed = false;
                res.failureDetails = "Unhandled unknown exception";
                failedCount++;
                std::cout << "[FAILED] (" << std::fixed << std::setprecision(2) << res.durationMs << " ms)" << std::endl;
                std::cout << "    Unhandled unknown exception" << std::endl;
            }
            results.push_back(res);
        }

        auto suiteEnd = std::chrono::high_resolution_clock::now();
        double totalDurationMs = std::chrono::duration<double, std::milli>(suiteEnd - suiteStart).count();

        std::cout << "\n========================================================" << std::endl;
        std::cout << "                    Test Summary                        " << std::endl;
        std::cout << "========================================================" << std::endl;
        std::cout << " Total Tests : " << tests.size() << std::endl;
        std::cout << " Passed      : " << passedCount << std::endl;
        std::cout << " Failed      : " << failedCount << std::endl;
        std::cout << " Total Time  : " << std::fixed << std::setprecision(2) << totalDurationMs << " ms ("
                  << std::setprecision(2) << (totalDurationMs / 1000.0) << " s)" << std::endl;
        std::cout << "========================================================" << std::endl;

        if (failedCount > 0) {
            std::cout << "\nFailed Test Details:" << std::endl;
            for (const auto &r : results) {
                if (!r.passed) {
                    std::cout << " - " << r.name << ":\n    " << r.failureDetails << std::endl;
                }
            }
            std::cout << "\nResult: FAILED" << std::endl;
            return -1;
        }

        std::cout << "Result: ALL TESTS PASSED" << std::endl;
        return 0;
    }

private:
    struct Entry
    {
        std::string name;
        TestFn fn;
    };
    std::vector<Entry> tests;
};

int main()
{
    TestSuite suite;

    // Unit test cases (executed sequentially)
    // Note: test_uninitialized_access MUST be registered and run first before FastLog::initialize()
    suite.addTest("TestUninitializedAccessThrows", test_uninitialized_access);
    suite.addTest("TestInitializationAndReinitialization", test_initialization_and_reinitialization);
    suite.addTest("TestLogMsgStructure", test_log_msg_structure);
    suite.addTest("TestAllSeverityMacros", test_all_severity_macros);
    suite.addTest("TestSpecialCharactersAndPayloads", test_special_characters_and_payloads);
    suite.addTest("TestConcurrentMultiThreadedLogging", test_concurrent_multithreaded_logging);
    suite.addTest("TestHighVolumeBurstLogging", test_high_volume_burst_logging);
    suite.addTest("TestPerformanceThroughput", test_performance_throughput);
    suite.addTest("TestFileWritePerformanceThroughput", test_file_write_throughput);

    return suite.runAll();
}

