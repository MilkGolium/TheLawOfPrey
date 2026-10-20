#!/usr/bin/env bash
set -euo pipefail

# Configure and build Debug, then run the regression tests through ctest.
# Same preflight checks as build.sh, from the same scripts/preflight.sh.
#
# The theme regression tests create scratch .cfg files under tests/tmp/themes
# and remove them afterwards, so nothing is left behind in assets/.
# The config regression tests do the same under tests/tmp/config.

TOP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "${TOP_DIR}/scripts/preflight.sh"

PreflightRequireCMake
PreflightRequireNinja
PreflightRequireCompiler

echo "Configuring and building Debug for the tests..."
cmake -S "${TOP_DIR}" -B "${TOP_DIR}/build/Debug" -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build "${TOP_DIR}/build/Debug" --config Debug

echo
echo "Running tests..."
ctest --test-dir "${TOP_DIR}/build/Debug" --output-on-failure

echo
echo "Tests finished. Test binaries:"
echo "  Debug: ${TOP_DIR}/build/Debug/theme_tests"
echo "  Debug: ${TOP_DIR}/build/Debug/config_tests"
echo "  Debug: ${TOP_DIR}/build/Debug/ui_tests"
