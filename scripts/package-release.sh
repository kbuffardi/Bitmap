#!/bin/bash

set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <tag> <output-directory>" >&2
  exit 64
fi

tag="$1"
if [[ ! "$tag" =~ ^v[0-9]+\.[0-9]+(\.[0-9]+)?$ ]]; then
  echo "Release tag must be vMAJOR.MINOR or vMAJOR.MINOR.PATCH." >&2
  exit 64
fi

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
output_dir="$2"
mkdir -p "$output_dir"
output_dir="$(cd "$output_dir" && pwd)"
build_dir="$(mktemp -d "${TMPDIR:-/tmp}/bitmap-release.XXXXXX")"
trap 'rm -rf "$build_dir"' EXIT

cxx="${CXX:-g++}"
compile_flags=(-std=c++11 -O2)
source_archive="$output_dir/bitmap-${tag}-source.zip"
static_library="$output_dir/libbitmap-${tag}-linux-x86_64.a"
example_binary="$output_dir/bitmap-example-${tag}-linux-x86_64"

rm -f "$source_archive" "$static_library" "$example_binary"

(
  cd "$project_root"
  zip -q -j "$source_archive" bitmap.h bitmap.cpp LICENSE README.md
)

expected_archive_contents=$'LICENSE\nREADME.md\nbitmap.cpp\nbitmap.h'
actual_archive_contents="$(zipinfo -1 "$source_archive" | LC_ALL=C sort)"
if [ "$actual_archive_contents" != "$expected_archive_contents" ]; then
  echo "Source archive contains unexpected files." >&2
  exit 1
fi

"$cxx" "${compile_flags[@]}" -c "$project_root/bitmap.cpp" -o "$build_dir/bitmap.o"
ar rcs "$static_library" "$build_dir/bitmap.o"
"$cxx" "${compile_flags[@]}" "$project_root/examples/bitmap_example.cpp" \
  "$static_library" -o "$example_binary"
"$example_binary"

test -s "$source_archive"
test -s "$static_library"
test -x "$example_binary"
