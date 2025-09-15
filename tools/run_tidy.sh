#!/usr/bin/env bash
set -euo pipefail

if ! command -v run-clang-tidy >/dev/null 2>&1 && ! command -v clang-tidy >/dev/null 2>&1; then
  echo "clang-tidy or run-clang-tidy not found" >&2
  exit 1
fi

build_dir=${1:-build}

if [[ ! -f "$build_dir/compile_commands.json" ]]; then
  echo "compile_commands.json not found in $build_dir. Configure with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON" >&2
  exit 1
fi

if command -v run-clang-tidy >/dev/null 2>&1; then
  run-clang-tidy -p "$build_dir"
else
  clang-tidy $(jq -r '.[].file' "$build_dir/compile_commands.json") -p "$build_dir"
fi

