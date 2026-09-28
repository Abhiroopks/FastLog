include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.18.0
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(googletest)

add_executable(FastLogTest
    main.cpp
)

target_link_libraries(FastLogTest
    PRIVATE
        GTest::gtest_main
        FastLogLib
)

add_test(NAME FastLogTest COMMAND FastLogTest)
