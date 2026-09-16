cmake_minimum_required(VERSION 3.20)
project(FastLog VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    LockFreeRingBuffer
    GIT_REPOSITORY  git@github.com:Abhiroopks/LockFreeRingBuffer.git
    GIT_TAG         v1.0.0
    GIT_PROGRESS    TRUE
)
FetchContent_MakeAvailable(LockFreeRingBuffer)

# FetchContent_Declare(
#     concurrentqueue
#     GIT_REPOSITORY https://github.com/cameron314/concurrentqueue.git
#     GIT_TAG        v1.0.5
# )
# 
# FetchContent_MakeAvailable(concurrentqueue)

# Enable testing
enable_testing()

# Add subdirectories
# The library must be added before the test if the test depends on it
add_subdirectory(src)
add_subdirectory(test)
