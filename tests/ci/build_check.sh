#!/usr/bin/env bash
set -e
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
# Confirm binary exits non-zero (avoid set -e triggering inside the if)
if ./build/mslang; then
  echo "ERROR: mslang should exit non-zero" >&2
  exit 1
fi
cmake -B build_rel -DCMAKE_BUILD_TYPE=Release
cmake --build build_rel
echo "BUILD OK"
