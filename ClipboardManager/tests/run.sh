#!/bin/sh
# Builds and runs the platform independent unit tests on any machine with g++.
set -e
cd "$(dirname "$0")"
g++ -std=c++17 -Wall -Wextra -O1 core_test.cpp ../src/core.cpp -o /tmp/cm_core_test
/tmp/cm_core_test
