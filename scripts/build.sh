#!/usr/bin/env bash
set -euo pipefail

# Simple convenience script to configure and build both Release and Debug using Ninja.
# Requires cmake and ninja installed, and raylib available on the system.

TOP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "Configuring and building Release..."
cmake -S "${TOP_DIR}" -B "${TOP_DIR}/build/Release" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "${TOP_DIR}/build/Release" --config Release

echo "Configuring and building Debug..."
cmake -S "${TOP_DIR}" -B "${TOP_DIR}/build/Debug" -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build "${TOP_DIR}/build/Debug" --config Debug

echo
echo "Build finished. Binaries:"
echo "  Release: ${TOP_DIR}/build/Release/TheLawOfPrey"
echo "  Debug:   ${TOP_DIR}/build/Debug/TheLawOfPrey"
