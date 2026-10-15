# 构建前置检查库，被 scripts/build.sh 与 scripts/run_tests.sh 用 source 引入。
# 没有 shebang，也没有可执行位：它不单独执行。
# 只负责把「工具没就位」这件事说清楚——缺什么、按发行版怎么装、然后退出。
# 绝不自动安装任何东西，也绝不代跑 sudo；缺依赖一律以非零码退出。

# 本项目用到的 cmake 功能在 3.16 之前就已全部存在，版本要求只按「发行版自带的够用」定档。
kRequiredCMake="3.16"  # 与 CMakeLists.txt 的 cmake_minimum_required 保持一致

PreflightRequireCMake() {
  local required_major required_minor version_line version major minor

  IFS=. read -r required_major required_minor _ <<<"${kRequiredCMake}"

  if ! command -v cmake >/dev/null 2>&1; then
    printf '%s\n' \
      "[前置检查] 未找到 cmake 命令。" \
      "" \
      "发行版仓库里的 cmake 往往太旧，达不到本项目要求的最低版本（${kRequiredCMake}）。可以这样装：" \
      "" \
      "  pip install cmake                最快拿到新版" \
      "  Debian/Ubuntu 的 Kitware 官方 apt 源" \
      "      先按 https://cmake.org/install/ 配好源，再执行 sudo apt install cmake" \
      "  snap install cmake --classic" \
      "  macOS：brew install cmake" \
      "  FreeBSD：pkg install cmake" \
      "" \
      "以上命令请自行执行。这是构建前置检查，构建尚未开始。"
    exit 1
  fi

  # 只看第一行，形如「cmake version 3.16.3」；取 major.minor 与最低要求做整数比较。
  version_line="$(cmake --version 2>/dev/null || true)"
  version_line="${version_line%%$'\n'*}"
  version="${version_line##* }"
  version="${version%%[!0-9.]*}"
  IFS=. read -r major minor _ <<<"${version}"
  major="${major:-0}"
  minor="${minor:-0}"
  # 一律按十进制读：bash 算术会把带前导零的数字当八进制（"08" 直接报错），
  # 算术出错会让下面的比较判假，也就是把畸形的版本串静默判成「合格」。
  major="$((10#${major}))"
  minor="$((10#$minor))"

  if (( major < required_major || (major == required_major && minor < required_minor) )); then
    printf '%s\n' \
      "[前置检查] cmake 版本不达标：需要 ${kRequiredCMake} 或更高，实际读到「${version_line:-（空输出）}」。" \
      "" \
      "发行版仓库里的 cmake 往往太旧，达不到本项目要求的最低版本。可以这样装：" \
      "" \
      "  pip install cmake                最快拿到新版" \
      "  Debian/Ubuntu 的 Kitware 官方 apt 源" \
      "      先按 https://cmake.org/install/ 配好源，再执行 sudo apt install cmake" \
      "  snap install cmake --classic" \
      "  macOS：brew install cmake" \
      "  FreeBSD：pkg install cmake" \
      "" \
      "以上命令请自行执行。这是构建前置检查，构建尚未开始。"
    exit 1
  fi
}

PreflightRequireNinja() {
  if ! command -v ninja >/dev/null 2>&1; then
    printf '%s\n' \
      "[前置检查] 未找到 ninja 命令。" \
      "" \
      "本项目的构建脚本固定使用 Ninja 生成器，请按发行版安装：" \
      "" \
      "  Debian/Ubuntu：sudo apt install ninja-build" \
      "  Fedora：sudo dnf install ninja-build" \
      "  Arch：sudo pacman -S ninja" \
      "  openSUSE：sudo zypper install ninja" \
      "  macOS：brew install ninja" \
      "  FreeBSD：pkg install ninja" \
      "" \
      "以上命令请自行执行。这是构建前置检查，构建尚未开始。"
    exit 1
  fi
}

PreflightRequireCompiler() {
  if ! command -v cc >/dev/null 2>&1 &&
     ! command -v clang >/dev/null 2>&1 &&
     ! command -v gcc >/dev/null 2>&1; then
    printf '%s\n' \
      "[前置检查] 未找到 C 编译器（cc、clang、gcc 都没有）。" \
      "" \
      "本项目是 C11 + raylib，至少要有一个 C 编译器在 PATH 里，请按发行版安装：" \
      "" \
      "  Debian/Ubuntu：sudo apt install build-essential" \
      "  Fedora：sudo dnf groupinstall \"Development Tools\"" \
      "  Arch：sudo pacman -S base-devel" \
      "  macOS：xcode-select --install" \
      "  FreeBSD：pkg install llvm" \
      "" \
      "以上命令请自行执行。这是构建前置检查，构建尚未开始。"
    exit 1
  fi
}
