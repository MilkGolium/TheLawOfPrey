#!/usr/bin/env bash
set -euo pipefail

# 配置并构建 Debug 一份，再用 ctest 跑回归测试，流程与 build.sh 相同。
# 构建前置检查同样来自 scripts/preflight.sh。
#
# 主题回归测试会在 tests/tmp/themes 下造临时 .cfg 文件并在结束后清掉，
# assets/ 下不会留下任何东西。

TOP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "${TOP_DIR}/scripts/preflight.sh"

PreflightRequireCMake
PreflightRequireNinja
PreflightRequireCompiler

echo "正在配置并构建 Debug（供测试使用）..."
cmake -S "${TOP_DIR}" -B "${TOP_DIR}/build/Debug" -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build "${TOP_DIR}/build/Debug" --config Debug

echo
echo "正在运行测试..."
ctest --test-dir "${TOP_DIR}/build/Debug" --output-on-failure

echo
echo "测试结束，测试程序路径："
echo "  Debug: ${TOP_DIR}/build/Debug/theme_tests"
echo "  Debug: ${TOP_DIR}/build/Debug/ui_tests"
