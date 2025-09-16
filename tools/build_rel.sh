#!/usr/bin/env bash
set -euo pipefail

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=ON
cmake --build build -j
ctest --test-dir build --output-on-failure || true

echo "Run headless sanity: HEADLESS_RUN_MS=500 ./app/bin/RelWithDebInfo/vulkano_app"

