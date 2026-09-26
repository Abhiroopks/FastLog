# test/CMakeLists.txt

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.18.0
)

FetchContent_MakeAvailable(googletest)

add_executable(FastLogTest
    main.cpp
)

# Link the test executable against the dependencies.
target_link_libraries(FastLogTest
    PRIVATE
        GTest::gtest_main
        FastLogLib
)

add_test(NAME FastLogTest COMMAND FastLogTest)

include(GoogleTest)
gtest_discover_tests(FastLogTest)
