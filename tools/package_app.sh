#!/usr/bin/env bash
set -euo pipefail

# Package the built app and runtime assets into ./app/<Config>
# Usage: tools/package_app.sh [Debug|Release]

CONFIG=${1:-Release}
ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
BUILD_DIR="${ROOT_DIR}/build"
BIN_DIR="${BUILD_DIR}/bin/${CONFIG}"
OUT_DIR="${ROOT_DIR}/app/${CONFIG}"

if [[ ! -x "${BIN_DIR}/vulkan_app_example" && ! -x "${BIN_DIR}/vulkan_app_example.exe" ]]; then
  echo "Binary not found in ${BIN_DIR}. Build first: cmake -S . -B build -DCMAKE_BUILD_TYPE=${CONFIG} && cmake --build build --config ${CONFIG}" >&2
  exit 1
fi

echo "Staging to ${OUT_DIR}"
rm -rf "${OUT_DIR}"
mkdir -p "${OUT_DIR}/shaders"

# Copy binary
if [[ -x "${BIN_DIR}/vulkan_app_example.exe" ]]; then
  cp -v "${BIN_DIR}/vulkan_app_example.exe" "${OUT_DIR}/"
else
  cp -v "${BIN_DIR}/vulkan_app_example" "${OUT_DIR}/"
fi

# Copy shaders (prefer compiled SPIR-V if present)
if compgen -G "${BUILD_DIR}/shaders/*.spv" > /dev/null; then
  cp -v ${BUILD_DIR}/shaders/*.spv "${OUT_DIR}/shaders/" || true
else
  cp -v ${ROOT_DIR}/shaders/* "${OUT_DIR}/shaders/" || true
fi

# Copy assets if present
if [[ -d "${ROOT_DIR}/assets" ]]; then
  rsync -av --exclude ".gitkeep" "${ROOT_DIR}/assets/" "${OUT_DIR}/assets/" || true
fi

echo "Packaged application in ${OUT_DIR}"

