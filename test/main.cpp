#include "fastlog.h" // Included automatically via target_include_directories
#include <chrono>
#include <iostream>
#include <thread>

const std::string FILE_NAME = "log.log";
const bool STD_OUT = true;

void testLogMsg(const std::string msg)
{
    LOG_INFO(msg);
}

int main()
{
    FastLog::initialize(FILE_NAME, STD_OUT);

    std::vector<std::thread> threads;
    threads.push_back(std::thread([] { testLogMsg("hi"); }));
    threads.push_back(std::thread([] { testLogMsg("work1"); }));
    threads.push_back(std::thread([] { testLogMsg("work2"); }));
    threads.push_back(std::thread([] { testLogMsg("work3"); }));
    threads.push_back(std::thread([] { testLogMsg("bye"); }));

    for (auto &t : threads) {
        t.join();
    }

    return 0;
}
