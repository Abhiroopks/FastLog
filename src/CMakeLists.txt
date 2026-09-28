add_library(FastLogLib SHARED
    FastLog_global.h
    fastlog.h
    fastlog.cpp
)

target_compile_definitions(FastLogLib
    PRIVATE
        FASTLOG_LIBRARY
)

if(BUILD_TESTING)
    target_compile_definitions(FastLogLib PUBLIC TESTING)
endif()

target_include_directories(FastLogLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(FastLogLib PUBLIC LockFreeRingBuffer::LockFreeRingBuffer)

