#!/usr/bin/env bash
set -euo pipefail

# 配置并构建 Release 与 Debug 两份，统一使用 Ninja 生成器。
# 构建前置检查在 scripts/preflight.sh：cmake、ninja、C 编译器缺一不可，
# 三项检查全齐后才开始配置，构建过程中不会留下半配置目录。
# raylib 不在这里检查，CMakeLists.txt 已有明确的 FATAL_ERROR 提示。

TOP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "${TOP_DIR}/scripts/preflight.sh"

PreflightRequireCMake
PreflightRequireNinja
PreflightRequireCompiler

echo "正在配置并构建 Release..."
cmake -S "${TOP_DIR}" -B "${TOP_DIR}/build/Release" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "${TOP_DIR}/build/Release" --config Release

echo "正在配置并构建 Debug..."
cmake -S "${TOP_DIR}" -B "${TOP_DIR}/build/Debug" -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build "${TOP_DIR}/build/Debug" --config Debug

echo
echo "构建完成，产物路径："
echo "  Release: ${TOP_DIR}/build/Release/TheLawOfPrey"
echo "  Debug:   ${TOP_DIR}/build/Debug/TheLawOfPrey"
