#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="${project_dir}/build-macos"
juce_dir="${JUCE_DIR:-/Users/nenofernando/Downloads/JUCE}"

extra_args=()
if [[ "${NF_ENABLE_AAX:-0}" == "1" ]]; then
  : "${JUCE_AAX_SDK_PATH:?Set JUCE_AAX_SDK_PATH to the official AAX SDK path}"
  extra_args+=("-DNF_ENABLE_AAX=ON" "-DJUCE_AAX_SDK_PATH=${JUCE_AAX_SDK_PATH}")
fi

generator_args=()
if [[ -d /Applications/Xcode.app ]]; then
  generator_args=(-G Xcode)
  extra_args+=("-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64")
else
  extra_args+=("-DCMAKE_BUILD_TYPE=Release" "-DCMAKE_OSX_ARCHITECTURES=x86_64")
fi

cmake -S "${project_dir}" -B "${build_dir}" "${generator_args[@]}" \
  -DJUCE_DIR="${juce_dir}" \
  -DNF_BUILD_TESTS=ON \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  "${extra_args[@]}"
cmake --build "${build_dir}" --config Release --parallel
ctest --test-dir "${build_dir}" -C Release --output-on-failure || true

echo "Builds: ${build_dir}/NFVocalHarmonizer_artefacts/Release"
