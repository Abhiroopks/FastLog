cmake_minimum_required(VERSION 3.14)

project(FastLog LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(FastLog SHARED
  FastLog_global.h
  fastlog.cpp
  fastlog.h
)

target_compile_definitions(FastLog PRIVATE FASTLOG_LIBRARY)
