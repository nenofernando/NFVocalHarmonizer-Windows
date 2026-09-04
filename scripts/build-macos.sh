#!/usr/bin/env bash
set -euo pipefail
if [[ -z "${JUCE_DIR:-}" ]]; then
  echo "Set JUCE_DIR to the JUCE source directory."
  exit 2
fi
cmake -S . -B build-macos -G Xcode -DJUCE_DIR="$JUCE_DIR" -DNF_BUILD_TESTS=ON
cmake --build build-macos --config Release
ctest --test-dir build-macos -C Release --output-on-failure || true

