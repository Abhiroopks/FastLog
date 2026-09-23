# FastLog

A high-performance, asynchronous, thread-safe C++ logging library that outputs structured [JSON Lines](https://jsonlines.org/) logs.

---

## Features

- **Asynchronous & Non-Blocking**: Log messages are queued and dispatched by a dedicated background worker thread.
- **Thread-Safe**: Multiple threads can safely log concurrently without data races or lock contention on I/O.
- **Structured JSONL Output**: Logs are formatted into clean JSONL records for easy parsing and log analysis.
- **Convenient Logging Macros**: Automatically captures source file name, line number, log level, and timestamp.
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
    // 1. Initialize FastLog with the target file path and stdout logging flag
    const std::string logFilePath = "app.log";
    const bool enableStdout = true;
    FastLog::initialize(logFilePath, enableStdout);

    // 2. Use the logging macros throughout your application
    LOG_INFO("Application initialized successfully");
    LOG_DEBUG("Loading configuration parameters");
    LOG_WARNING("Disk space is below 20%");
    LOG_CRITICAL("Database connection dropped, attempting reconnect");
    LOG_FATAL("Unable to restore service, shutting down");

    return 0;
}
```

### 2. Multi-Threaded Logging Example

FastLog is designed to handle high-throughput logging across concurrent threads:

```cpp
#include "fastlog.h"
#include <thread>
#include <vector>

void workerTask(int workerId)
{
    LOG_INFO("Worker " + std::to_string(workerId) + " starting task");
    // Perform work...
    LOG_INFO("Worker " + std::to_string(workerId) + " finished task");
}

int main()
{
    FastLog::initialize("multithread.log", false);

    std::vector<std::thread> workers;
    for (int i = 0; i < 8; ++i) {
        workers.emplace_back(workerTask, i);
    }

    for (auto &t : workers) {
        t.join();
    }

    return 0;
}
```

---

## API Reference

### Initialization & Singleton

- `void FastLog::initialize(std::string fileName, bool stdOut)`
  - Initializes the logger. Must be called once before any logging macros are invoked.
  - `fileName`: File path where the JSON log output will be written.
  - `stdOut`: When set to `true`, messages are printed to `std::cout` in addition to the file.

- `FastLog &FastLog::getInstance()`
  - Returns the singleton instance of `FastLog`. Throws `std::runtime_error` if called before `FastLog::initialize()`.

- `int FastLog::getLogCount()`
  - Returns the total number of log entries written and flushed to the destination log file. Thread-safe (atomic access).

### Logging Macros

The following macros automatically capture the source filename (`__FILE__`) and line number (`__LINE__`):

| Macro | Level | Description |
| :--- | :--- | :--- |
| `LOG_DEBUG(MSG)` | `DEBUG` | Fine-grained diagnostic events |
| `LOG_INFO(MSG)` | `INFO` | Informational messages on application progress |
| `LOG_WARNING(MSG)` | `WARNING` | Potentially harmful situations |
| `LOG_CRITICAL(MSG)` | `CRITICAL` | Severe errors that require immediate attention |
| `LOG_FATAL(MSG)` | `FATAL` | Fatal errors causing premature termination |

---

## JSON Log Output Format

FastLog writes structured JSON entries to the configured log file:

```json
{"timestamp":"23-09-2026 16:22:28","level":"DEBUG","source":"/home/abhi/Projects/FastLog/test/main.cpp","line":143,"msg":"Testing LOG_DEBUG macro execution"}
{"timestamp":"23-09-2026 16:22:28","level":"INFO","source":"/home/abhi/Projects/FastLog/test/main.cpp","line":144,"msg":"Testing LOG_INFO macro execution"}
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

| Test | Throughput | Avg Latency | Total Time |
|------|-----------|-------------|------------|
| Enqueue throughput | 629,078 msgs/sec | 1.59 µs/msg | 32.13 ms |
| File write throughput | 577,079 logs/sec | 1.73 µs/log | 29.59 ms |

