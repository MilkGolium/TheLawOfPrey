#!/usr/bin/env bash
set -euo pipefail

# Configure and build the Debug configuration, then run the regression tests
# through ctest. Requires cmake and ninja installed, and raylib available on
# the system, exactly like build.sh.
#
# The theme regression tests write scratch .cfg files under tests/tmp/themes
# and clean them up afterwards, so nothing is left behind in assets/.

TOP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "Configuring and building Debug for tests..."
cmake -S "${TOP_DIR}" -B "${TOP_DIR}/build/Debug" -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build "${TOP_DIR}/build/Debug" --config Debug

echo
echo "Running tests..."
ctest --test-dir "${TOP_DIR}/build/Debug" --output-on-failure

echo
echo "Tests finished. Test binaries:"
echo "  Debug: ${TOP_DIR}/build/Debug/theme_tests"
echo "  Debug: ${TOP_DIR}/build/Debug/ui_tests"