# test/CMakeLists.txt

add_executable(FastLogTest
    main.cpp
)

# Link the test executable against the library
target_link_libraries(FastLogTest PRIVATE
    FastLogLib
)

add_test(NAME FastLogTest COMMAND FastLogTest)

