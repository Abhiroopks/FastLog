# FastLog

A high-performance, asynchronous, thread-safe C++ logging library that outputs structured [JSON Lines](https://jsonlines.org/) logs.

---

## Features

- **Asynchronous & Configurable Blocking**: Log messages are queued and dispatched by a dedicated background worker thread. Supports non-blocking mode (default, low-latency, drops if queue is full) or blocking mode (exponential backoff retry, zero message loss).
- **Thread-Safe**: Multiple threads can safely log concurrently without data races or lock contention on I/O.
- **Structured JSONL Output**: Logs are formatted into clean JSONL records for easy parsing and log analysis.
- **Automatic Log Rotation**: Automatically rolls over to a new log file once a configurable file size threshold (default 16 MB) is reached, using sequential numbering (`<name>-0.log`, `<name>-1.log`, etc.).
- **Automatic Source Location Capture**: Automatically captures source file name, line number, log level, and timestamp using C++20 `std::source_location`.
- **Dual Output Support**: Logs to a specified file and optionally mirrors output to standard output (`stdout`).

---

## Requirements

- **C++20** or higher
- **CMake 3.20** or higher

---

## Including FastLog in CMake Projects

You can integrate FastLog into your CMake project using either **CMake FetchContent** or as a **Git Submodule / Subdirectory**.

### Option 1: Using CMake `FetchContent` (Recommended)

Add the following to your project's `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    FastLog
    GIT_REPOSITORY https://github.com/Abhiroopks/FastLog.git
    GIT_TAG main # Replace with a specific tag, branch, or commit hash
)
FetchContent_MakeAvailable(FastLog)

add_executable(my_app main.cpp)

# Link against the FastLogLib target
target_link_libraries(my_app PRIVATE FastLogLib)
```

### Option 2: Using Git Submodule / `add_subdirectory`

1. Add FastLog as a git submodule:
   ```bash
   git submodule add https://github.com/Abhiroopks/FastLog.git extern/FastLog
   ```

2. Add the subdirectory and link against `FastLogLib` in your `CMakeLists.txt`:
   ```cmake
   add_subdirectory(extern/FastLog)

   add_executable(my_app main.cpp)
   target_link_libraries(my_app PRIVATE FastLogLib)
   ```

---

## Usage in C++ Code

### 1. Quick Start Example

```cpp
#include "fastlog.h"
#include <iostream>

int main()
{
    // 1. Instantiate FastLog with the target file path and stdout logging flag
    // Logs are written to app-0.log and rotated when the file size threshold is reached
    const std::string logFilePath = "app.log";
    const bool enableStdout = true;
    FastLog logger(logFilePath, enableStdout);

    // 2. Use the logging methods on the instance
    logger.info("Application initialized successfully");
    logger.debug("Loading configuration parameters");
    logger.warning("Disk space is below 20%");
    logger.critical("Database connection dropped, attempting reconnect");
    // logger.fatal("Unable to restore service, shutting down"); // Note: terminates process

    return 0;
}
```

### 2. Multi-Threaded Logging Example

FastLog is designed to handle high-throughput logging across concurrent threads:

```cpp
#include "fastlog.h"
#include <thread>
#include <vector>

void workerTask(FastLog &logger, int workerId)
{
    logger.info("Worker " + std::to_string(workerId) + " starting task");
    // Perform work...
    logger.info("Worker " + std::to_string(workerId) + " finished task");
}

int main()
{
    FastLog logger("multithread.log", false);

    std::vector<std::thread> workers;
    for (int i = 0; i < 8; ++i) {
        workers.emplace_back(workerTask, std::ref(logger), i);
    }

    for (auto &t : workers) {
        t.join();
    }

    return 0;
}
```

### 3. Log Rotation, Buffer & Blocking Configuration Example

You can configure the in-memory write buffer, queue capacity, log rotation size threshold, and blocking behavior:

```cpp
#include "fastlog.h"

int main()
{
    const std::string logFilePath = "app.log";
    const bool enableStdout = false;
    const unsigned int bufferSize = 32 * 1024;         // 32 KB write buffer
    const unsigned int queueSize = 500000;             // 500k messages in queue
    const unsigned int maxFileSize = 10 * 1024 * 1024; // 10 MB per file before rotating
    const bool blocking = true;                        // Enable blocking mode (retry on full queue)

    FastLog logger(logFilePath, enableStdout, bufferSize, queueSize, maxFileSize, blocking);

    logger.info("Logging with custom buffer, rotation, and blocking settings");

    return 0;
}
```

---

## Log Rotation

FastLog automatically splits log output across sequentially numbered files to prevent log files from growing unboundedly:

- **Sequential Numbering**: When initialized with a file name such as `"app.log"`, FastLog extracts the base name and extension to format filenames as `<base>-<index>.<ext>` (e.g., `app-0.log`, `app-1.log`, `app-2.log`, etc.).
- **Rotation Threshold**: FastLog tracks the cumulative byte size written to the active log file during buffer flushes. Whenever the file size reaches or exceeds `logFileMaxSize` (default 16 MB), the current file is closed, the sequence counter is incremented, and a new log file is opened.
- **Asynchronous Execution**: Log rotation occurs transparently on the background worker thread during write flushes without blocking worker threads calling logging methods.

---

## Queue Overflow Handling (Blocking vs. Non-Blocking)

FastLog enqueues log messages into an internal lock-free ring buffer (`LockFreeRingBuffer`) before the background worker thread writes them to disk. The `blocking` constructor parameter controls how logging methods behave when the ring buffer is full:

- **Non-Blocking Mode (`blocking = false`, default)**:
  - If the queue is full, logging calls attempt a single push to the ring buffer and immediately return. If the queue cannot accept the item, the message is dropped.
  - Calling threads are never blocked or stalled, prioritizing minimal latency and high throughput.
  - Ideal for performance-critical applications where thread latency is paramount and occasional message drops under extreme load are acceptable.

- **Blocking Mode (`blocking = true`)**:
  - If the queue is full, the calling thread retries pushing the message in a loop using exponential backoff until it succeeds.
  - The retry loop begins by sleeping for `DEFAULT_BLOCKING_TIME` (1 nanosecond) using `std::this_thread::sleep_for(blockingTime)`.
  - On each failed attempt, the sleep duration is doubled (`blockingTime *= 2`) before retrying.
  - Guarantees zero log loss, at the cost of blocking producer threads until the background writer thread drains sufficient space from the queue.
  - Ideal for applications where complete audit trails and log integrity are required without dropping any events.

---

## API Reference

### Constructor & Lifecycle

- `FastLog(std::string fileName, bool stdOut = false, unsigned int bufferSize = DEFAULT_BUFFER_SIZE, unsigned int queueSize = DEFAULT_LOG_QUEUE_SIZE, unsigned int logFileMaxSize = DEFAULT_LOG_FILE_SIZE, bool blocking = false)`
  - Constructs and initializes a `FastLog` instance, starting the background writer thread.
  - `fileName`: Target base file path where logs will be written. FastLog appends a sequential index to the base filename (e.g., `"app.log"` produces `app-0.log`, `app-1.log`, etc.).
  - `stdOut`: When set to `true`, messages are printed to `std::cout` in addition to the file. Defaults to `false`.
  - `bufferSize`: Size (in bytes) of the in-memory write buffer before flushing to disk. Defaults to 16 KB (`DEFAULT_BUFFER_SIZE = 1 << 14`).
  - `queueSize`: Maximum capacity (number of log messages) of the lock-free ring buffer. Defaults to 1,048,576 messages (`DEFAULT_LOG_QUEUE_SIZE = 1 << 20`).
  - `logFileMaxSize`: Maximum size (in bytes) of a log file on disk before rotating to a new indexed file. Defaults to 16 MB (`DEFAULT_LOG_FILE_SIZE = 1 << 24`).
  - `blocking`: Controls queue overflow behavior when the lock-free ring buffer is full. Defaults to `false`.
    - `false` (default): Non-blocking push; drops the message if the queue is full to avoid stalling producer threads.
    - `true`: Blocking push; retries in a loop with exponential backoff (starting at 1 nanosecond sleep and doubling per retry) until the message is enqueued, ensuring zero message loss.

- `~FastLog()`
  - Destructor. Signals the background thread to finish, flushes remaining buffers to disk, and closes the output log file. Copy and move constructors and assignment operators are disabled (`= delete`).

- `void flush()`
  - Enqueues a flush request to immediately flush buffered log messages to disk.

- `unsigned int getLogCount()` *(Available when compiled with `BUILD_TESTING=ON` / `TESTING`)*
  - Returns the total number of log entries written and flushed to the destination log file. Thread-safe (atomic access).

### Logging Methods

`FastLog` instances provide public member methods for each severity level. These methods automatically capture the calling source file name and line number using C++20 `std::source_location`:

| Method | Level | Description |
| :--- | :--- | :--- |
| `void debug(std::string msg, const std::source_location loc = std::source_location::current())` | `DEBUG` | Fine-grained diagnostic events |
| `void info(std::string msg, const std::source_location loc = std::source_location::current())` | `INFO` | Informational messages on application progress |
| `void warning(std::string msg, const std::source_location loc = std::source_location::current())` | `WARNING` | Potentially harmful situations |
| `void critical(std::string msg, const std::source_location loc = std::source_location::current())` | `CRITICAL` | Severe errors that require immediate attention |
| `[[noreturn]] void fatal(std::string msg, const std::source_location loc = std::source_location::current())` | `FATAL` | Fatal errors causing immediate application termination (`std::exit(-1)`) |

#### Overloads & Aliases

- **Explicit Source & Line Overloads**:
  All severity methods provide overloads accepting explicit source file and line number:
  - `void debug(std::string msg, std::string source, const unsigned int line)`
  - `void info(std::string msg, std::string source, const unsigned int line)`
  - `void warning(std::string msg, std::string source, const unsigned int line)`
  - `void critical(std::string msg, std::string source, const unsigned int line)`
  - `[[noreturn]] void fatal(std::string msg, std::string source, const unsigned int line)`

- **Named Convenience Aliases**:
  - CamelCase aliases: `logDebug(...)`, `logInfo(...)`, `logWarning(...)`, `logCritical(...)`, `logFatal(...)`
  - snake_case aliases: `log_debug(...)`, `log_info(...)`, `log_warning(...)`, `log_critical(...)`, `log_fatal(...)`

- **Direct API Logging**:
  - `void logMsg(const Severity level, std::string msg, std::string source, const unsigned int line)`
    Enqueues a structured log message directly with explicit severity, message, source, and line number.

---

## JSON Log Output Format

FastLog writes structured JSON entries to the configured log file:

```json
{"timestamp":"23-09-2026 16:22:28","level":"DEBUG","source":"/home/abhi/Projects/FastLog/test/main.cpp","line":143,"msg":"Testing debug method execution"}
{"timestamp":"23-09-2026 16:22:28","level":"INFO","source":"/home/abhi/Projects/FastLog/test/main.cpp","line":144,"msg":"Testing info method execution"}
```

---

## Building and Running Tests

To build the library and run the included multi-threaded test suite, use the provided convenience bash script: `run_tests.sh`

## Performance

System Info:
```bash
=== CPU ===
CPU(s):                                  8
On-line CPU(s) list:                     0-7
Model name:                              AMD Ryzen 3 5300U with Radeon Graphics
Thread(s) per core:                      2
Core(s) per socket:                      4
Socket(s):                               1
CPU(s) scaling MHz:                      59%
CPU max MHz:                             3900.0000
CPU min MHz:                             412.9420
NUMA node0 CPU(s):                       0-7
=== RAM ===
               total        used        free      shared  buff/cache   available
Mem:           7.1Gi       3.3Gi       1.1Gi        67Mi       3.1Gi       3.8Gi
=== Disk ===
NAME      SIZE TYPE MODEL                ROTA
nvme0n1 476.9G disk UMIS RPJTJ512MGE1QDY    0
=== OS ===
Linux 7.0.0-31-generic
PRETTY_NAME="Linux Mint 22.3"
```

Benchmarked on the hardware described above.

| Test | Throughput |
|------|-----------|
| Enqueue throughput | 814,971 msgs/sec |
| File write throughput | 676,941 logs/sec |

