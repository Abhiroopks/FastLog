add_library(FastLogLib SHARED
  FastLog_global.h
  fastlog.cpp
  fastlog.h
)

target_include_directories(FastLogLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
