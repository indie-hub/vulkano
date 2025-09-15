#!/usr/bin/env bash
set -euo pipefail

shopt -s globstar

files=(
  include/**/*.hpp
  src/**/*.cpp
  tests/**/*.cpp
)

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format not found" >&2
  exit 1
fi

clang-format -i "${files[@]}"

