#!/usr/bin/env bash
set -euo pipefail

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format not found in PATH" >&2
  exit 1
fi

git ls-files '*.hpp' '*.h' '*.cpp' '*.cc' | xargs -r clang-format -i

echo "Formatting complete."

