#include "fastlog.h" // Included automatically via target_include_directories
#include <chrono>
#include <iostream>
#include <thread>

const std::string FILE_NAME = "log.log";
const bool STD_OUT = false;

void testLogMsg(const int id)
{
    LOG_INFO("msg " + std::to_string(id));
}

int main()
{
    FastLog::initialize(FILE_NAME, STD_OUT);
    std::vector<std::thread> threads;
    unsigned int tasks = 100000;
    unsigned int cores = std::thread::hardware_concurrency();
    unsigned int blocks = cores - 1;

    std::cout << "Running " << tasks << " tasks in groups of " << cores - 1 << std::endl;

    for (auto i = 0; i < tasks / blocks; i++) {
        for (auto j = 0; j < blocks; j++) {
            auto thread_id = i * blocks + j;
            threads.push_back(std::thread([thread_id] { testLogMsg(thread_id); }));
        }

        for (auto &t : threads) {
            t.join();
        }

        threads.clear();
    }

    // do remainder tasks
    auto remaining = tasks % blocks;

    for (auto i = tasks - remaining; i < tasks; i++) {
        threads.push_back(std::thread([i] { testLogMsg(i); }));
    }

    for (auto &t : threads) {
        t.join();
    }

    return 0;
}
