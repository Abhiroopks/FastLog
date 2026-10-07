#!/bin/bash

cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=ON
cmake --build build --parallel

rm *.log
sudo perf record -g ./build/test/FastLogTest
