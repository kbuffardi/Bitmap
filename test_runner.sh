#!/bin/bash

set -eu

test_binary="${TMPDIR:-/tmp}/bitmap-tests"
g++ -std=c++11 -Wall -Wextra -pedantic bitmap.cpp tests/bitmap_tests.cpp -o "$test_binary"
"$test_binary"
