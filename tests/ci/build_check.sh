#!/usr/bin/env bash
set -e
cmake -S . -B build -G "Ninja Multi-Config"
cmake --build build --config Debug
cmake --build build --config Release
# exit-non-zero check is covered by ctest test_mslang_exits_nonzero (WILL_FAIL)
ctest --test-dir build -C Debug --output-on-failure
echo "BUILD OK"
