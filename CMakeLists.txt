cmake_minimum_required(VERSION 3.20)
project(FastLog VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    LockFreeRingBuffer
    GIT_REPOSITORY  https://github.com/Abhiroopks/LockFreeRingBuffer.git
    GIT_TAG         v1.1.1
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(LockFreeRingBuffer)


# Enable testing
enable_testing()

# Add subdirectories
# The library must be added before the test if the test depends on it
add_subdirectory(src)

option(BUILD_TESTING "Build tests" OFF)

# Testing configuration
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(test)
endif()
