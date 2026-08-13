add_library(FastLogLib SHARED
  FastLog_global.h
  fastlog.cpp
  fastlog.h
)

target_include_directories(FastLogLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(FastLogLib PRIVATE nlohmann_json::nlohmann_json)
