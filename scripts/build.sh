#!/usr/bin/env bash
set -euo pipefail

# Configure and build both Release and Debug, always with the Ninja generator.
# Preflight checks live in scripts/preflight.sh: cmake, ninja and a C compiler
# are all required, and nothing is configured until all three are present, so a
# failed check never leaves a half-configured build directory behind.
# raylib is deliberately not checked there: CMakeLists.txt already stops with a
# clear message about it.

TOP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "${TOP_DIR}/scripts/preflight.sh"

PreflightRequireCMake
PreflightRequireNinja
PreflightRequireCompiler

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
