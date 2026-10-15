# Build preflight checks, sourced by scripts/build.sh and scripts/run_tests.sh.
# No shebang and no executable bit: this file is never run on its own.
# It answers one question -- is the toolchain here -- and when it is not, it
# prints what is missing plus how to install it per distribution, then exits.
# It never installs anything and never runs sudo for you; a failed check always
# exits non-zero. All output is ASCII so it stays readable on a plain console.

# Every CMake feature this project uses predates 3.16, so the floor is set by
# "whatever the distributions ship".
kRequiredCMake="3.16"  # keep in sync with cmake_minimum_required in CMakeLists.txt

PreflightRequireCMake() {
  local required_major required_minor version_line version major minor

  IFS=. read -r required_major required_minor _ <<<"${kRequiredCMake}"

  if ! command -v cmake >/dev/null 2>&1; then
    printf '%s\n' \
      "[preflight] cmake not found in PATH." \
      "" \
      "This project needs ${kRequiredCMake} or newer; distribution packages are often older." \
      "Ways to get a current cmake:" \
      "" \
      "  pip install cmake                  fastest way to a current release" \
      "  Debian/Ubuntu: Kitware's apt repository" \
      "      set it up as described at https://cmake.org/install/ then: sudo apt install cmake" \
      "  snap install cmake --classic" \
      "  macOS:   brew install cmake" \
      "  FreeBSD: pkg install cmake" \
      "" \
      "Run that command yourself. This is a build preflight check; the build has not started."
    exit 1
  fi

  # First line only, shaped like "cmake version 3.16.3"; compare major.minor as
  # integers.
  version_line="$(cmake --version 2>/dev/null || true)"
  version_line="${version_line%%$'\n'*}"
  version="${version_line##* }"
  version="${version%%[!0-9.]*}"
  IFS=. read -r major minor _ <<<"${version}"
  major="${major:-0}"
  minor="${minor:-0}"
  # Force base 10: bash arithmetic reads a leading zero as octal ("08" is an
  # error), and an arithmetic error makes the comparison below false, which
  # would silently pass a malformed version string.
  major="$((10#${major}))"
  minor="$((10#$minor))"

  if (( major < required_major || (major == required_major && minor < required_minor) )); then
    printf '%s\n' \
      "[preflight] cmake is too old: ${kRequiredCMake} or newer is required, found \"${version_line:-no version output}\"." \
      "" \
      "Distribution packages are often older than this project needs." \
      "Ways to get a current cmake:" \
      "" \
      "  pip install cmake                  fastest way to a current release" \
      "  Debian/Ubuntu: Kitware's apt repository" \
      "      set it up as described at https://cmake.org/install/ then: sudo apt install cmake" \
      "  snap install cmake --classic" \
      "  macOS:   brew install cmake" \
      "  FreeBSD: pkg install cmake" \
      "" \
      "Run that command yourself. This is a build preflight check; the build has not started."
    exit 1
  fi
}

PreflightRequireNinja() {
  if ! command -v ninja >/dev/null 2>&1; then
    printf '%s\n' \
      "[preflight] ninja not found in PATH." \
      "" \
      "The build scripts always use the Ninja generator; install it per distribution:" \
      "" \
      "  Debian/Ubuntu: sudo apt install ninja-build" \
      "  Fedora:        sudo dnf install ninja-build" \
      "  Arch:          sudo pacman -S ninja" \
      "  openSUSE:      sudo zypper install ninja" \
      "  macOS:         brew install ninja" \
      "  FreeBSD:       pkg install ninja" \
      "" \
      "Run that command yourself. This is a build preflight check; the build has not started."
    exit 1
  fi
}

PreflightRequireCompiler() {
  if ! command -v cc >/dev/null 2>&1 &&
     ! command -v clang >/dev/null 2>&1 &&
     ! command -v gcc >/dev/null 2>&1; then
    printf '%s\n' \
      "[preflight] no C compiler found (none of cc, clang, gcc)." \
      "" \
      "This project is C11 + raylib and needs at least one C compiler in PATH;" \
      "install one per distribution:" \
      "" \
      "  Debian/Ubuntu: sudo apt install build-essential" \
      "  Fedora:        sudo dnf groupinstall \"Development Tools\"" \
      "  Arch:          sudo pacman -S base-devel" \
      "  macOS:         xcode-select --install" \
      "  FreeBSD:       pkg install llvm" \
      "" \
      "Run that command yourself. This is a build preflight check; the build has not started."
    exit 1
  fi
}
