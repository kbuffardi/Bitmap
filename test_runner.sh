#!/bin/bash

set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="$(mktemp -d "${TMPDIR:-/tmp}/bitmap-tests.XXXXXX")"
trap 'rm -rf "$build_dir"' EXIT

cxx="${CXX:-g++}"
compile_flags=(-std=c++11 -Wall -Wextra -Wpedantic)

"$cxx" "${compile_flags[@]}" -c "$project_root/bitmap.cpp" -o "$build_dir/bitmap.o"
"$cxx" "${compile_flags[@]}" "$project_root/tests/bitmap_test.cpp" \
  "$project_root/bitmap.cpp" -o "$build_dir/bitmap_test"
"$build_dir/bitmap_test"
"$cxx" "${compile_flags[@]}" "$project_root/examples/bitmap_example.cpp" \
  "$project_root/bitmap.cpp" -o "$build_dir/bitmap_example"
