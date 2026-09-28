cmake_minimum_required(VERSION 3.20)
project(FastLog VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(BUILD_TESTING "Build tests" OFF)

include(FetchContent)

FetchContent_Declare(
    LockFreeRingBuffer
    GIT_REPOSITORY  https://github.com/Abhiroopks/LockFreeRingBuffer.git
    GIT_TAG         v1.1.1
    EXCLUDE_FROM_ALL
)

# Prevent external dependencies from building their own tests
set(BUILD_TESTING_SAVED "${BUILD_TESTING}")
set(BUILD_TESTING OFF)
FetchContent_MakeAvailable(LockFreeRingBuffer)
set(BUILD_TESTING "${BUILD_TESTING_SAVED}")

# Add subdirectories
add_subdirectory(src)

# Testing configuration
if(BUILD_TESTING)
    include(CTest)
    add_subdirectory(test)
endif()
