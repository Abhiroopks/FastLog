#!/bin/bash

cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON

cmake --build build --config Release --parallel

./build/test/FastLogTest
