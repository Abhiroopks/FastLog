cmake_minimum_required(VERSION 3.16)
project(FastLog VERSION 1.0 LANGUAGES CXX)


include(FetchContent)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add subdirectories
# The library must be added before the test if the test depends on it
add_subdirectory(src)
add_subdirectory(test)   
