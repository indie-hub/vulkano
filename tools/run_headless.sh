#!/usr/bin/env bash
set -euo pipefail

BUILD_TYPE=${1:-Debug}

APP="./app/bin/${BUILD_TYPE}/vulkano_app"
if [ ! -x "$APP" ]; then
  echo "App binary not found at $APP. Build first: cmake -S . -B build -DCMAKE_BUILD_TYPE=${BUILD_TYPE} && cmake --build build --config ${BUILD_TYPE}" >&2
  exit 1
fi

echo "Running headless for 500 ms (${BUILD_TYPE})..."
HEADLESS_RUN_MS=500 "$APP"

echo "Done."

