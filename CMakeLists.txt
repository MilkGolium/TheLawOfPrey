cmake_minimum_required(VERSION 3.16)
# 定 3.16 是因为它覆盖 Ubuntu 20.04 / Debian 11 / RHEL 9 这一档发行版自带的 cmake，而本项目只用到 3.16 之前就有的功能；定更高会让发行版自带的 cmake 在还没走到「缺依赖」就先 FATAL。
project(TheLawOfPrey VERSION 0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Default to Release when not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif()

# Put all build outputs into <source-root>/build/<Config> (e.g. build/Release, build/Debug)
set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/build/${CMAKE_BUILD_TYPE}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIR})

# Target wiring shared by the game and the regression tests, so the two
# cannot drift apart: same include path, same PROJECT_ROOT_DIR, same flags.
# All libraries in this project are static, so ARCHIVE output placement is
# irrelevant, but it is set above for consistency.
add_library(raylib_deps INTERFACE)

# Bodies only, deliberately without src/main.c: the tests bring their own
# entry point.
set(GAME_SOURCES
  src/config.c
  src/font.c
  src/graphsettings.c
  src/lang.c
  src/scene_manager.c
  src/strings.c
  src/theme.c
  src/ui.c
  src/utils.c
)

# Project executable
# 场景、场景清单与命令行只服务本体：它们彼此引用，回归测试用不到，
# 因此不进 GAME_SOURCES，否则测试目标会被拖上全套场景。
add_executable(${PROJECT_NAME}
  src/main.c
  src/app.c
  src/cli.c
  src/scene_list.c
  src/scenes/settings_scene.c
  src/scenes/test_scene.c
  src/scenes/title_scene.c
  ${GAME_SOURCES}
)

# Linux / FreeBSD 上 raylib 的静态库会调用 libm（powf、cosf 之类），不显式
# 链接就会在链接期报 undefined reference；macOS 把 libm 并进了系统库，不链接
# 也能过——所以这个缺陷只在 Linux / FreeBSD 上暴露。用 check_library_exists
# 探测，而不是按平台硬写：libm 并进 libc 的平台（musl、新版 glibc）不需要它。
include(CheckLibraryExists)
check_library_exists(m powf "" HAVE_LIBM)

function(configure_project_target target)
  target_include_directories(${target} PRIVATE
    src/include
  )

  # Use CMake to automatically escape string literals
  target_compile_definitions(${target} PRIVATE
      PROJECT_ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
  )

  target_link_libraries(${target} PRIVATE raylib_deps)

  # Compiler flags per-configuration
  if(MSVC)
    target_compile_options(${target} PRIVATE
      $<$<CONFIG:Debug>:/Zi /Od>
      $<$<CONFIG:Release>:/O2 /Ot /GL>
    )
  else()
    target_compile_options(${target} PRIVATE
      $<$<CONFIG:Debug>:-g -O0 -DDEBUG>
      $<$<CONFIG:Release>:-O3 -march=native -flto -DNDEBUG>
    )
  endif()
endfunction()

configure_project_target(${PROJECT_NAME})

# === 回归测试 ===
#
# 自写断言，不引入第三方测试框架。测试与本体共用上面的编译选项，
# 因此双配置都能零警告构建。
enable_testing()

# 主题解析器。THEME_DIR 指向临时目录：测试在自己的目录下造 .cfg，
# 不会往 assets/ 下留任何文件，游戏本体那边仍然只认 assets/themes。
add_executable(theme_tests
  tests/theme_tests.c
  tests/test_util.c
  ${GAME_SOURCES}
)
target_include_directories(theme_tests PRIVATE tests)
target_compile_definitions(theme_tests PRIVATE
  THEME_DIR="tests/tmp/themes"
)
configure_project_target(theme_tests)
add_test(NAME theme_tests COMMAND theme_tests)

# 配置文件解析器。配置文件的位置由接口给出，测试在自己的临时目录里
# 造文件，不会往 assets/ 下留任何东西，游戏本体那边仍然只认平台默认位置。
add_executable(config_tests
  tests/config_tests.c
  tests/test_util.c
  ${GAME_SOURCES}
)
target_include_directories(config_tests PRIVATE tests)
configure_project_target(config_tests)
add_test(NAME config_tests COMMAND config_tests)

# 界面度量与居中。需要一个隐藏窗口把内容画进设计画布再回读像素。
add_executable(ui_tests
  tests/ui_tests.c
  tests/test_util.c
  ${GAME_SOURCES}
)
target_include_directories(ui_tests PRIVATE tests)
configure_project_target(ui_tests)
add_test(NAME ui_tests COMMAND ui_tests)

# Try to find raylib (more robust handling and helpful messages)
# Allow user to hint raylib location with environment variable RAYLIB_DIR or CMake variable RAYLIB_DIR
set(RAYLIB_HINTS $ENV{RAYLIB_DIR} ${RAYLIB_DIR} ${CMAKE_SOURCE_DIR}/external/raylib /usr/local /usr)
# Prefer config/package provided by raylib (provides imported target raylib::raylib)
find_package(raylib CONFIG QUIET HINTS ${RAYLIB_HINTS})

if(TARGET raylib::raylib)
  target_link_libraries(raylib_deps INTERFACE raylib::raylib)
else()
  # Try to find headers and library manually as a fallback
  find_path(RAYLIB_INCLUDE_DIR NAMES raylib.h HINTS ${RAYLIB_HINTS} PATH_SUFFIXES include)
  find_library(RAYLIB_LIBRARY NAMES raylib HINTS ${RAYLIB_HINTS} PATH_SUFFIXES lib lib64)
  if(RAYLIB_INCLUDE_DIR AND RAYLIB_LIBRARY)
    message(STATUS "Found raylib (include: ${RAYLIB_INCLUDE_DIR}, lib: ${RAYLIB_LIBRARY})")
    target_include_directories(raylib_deps INTERFACE ${RAYLIB_INCLUDE_DIR})
    target_link_libraries(raylib_deps INTERFACE ${RAYLIB_LIBRARY})
  else()
    message(FATAL_ERROR "Could not find raylib (this project needs 6.0 or newer).\n\nOptions:\n - Build from source (works everywhere, takes a minute):\n   git clone https://github.com/raysan5/raylib && cmake -S raylib -B raylib/build -G Ninja -DBUILD_EXAMPLES=OFF && cmake --build raylib/build && sudo cmake --install raylib/build\n - Package manager, where a current package exists:\n   Arch:      sudo pacman -S raylib\n   openSUSE:  sudo zypper install raylib-devel\n   Fedora:    sudo dnf install raylib-devel\n   FreeBSD:   pkg install raylib\n   macOS:     brew install raylib\n   NOTE: Debian 13 and Ubuntu ship no raylib package at all, and most distributions\n         that do package it are older than 6.0. Build from source or use vcpkg there.\n - Then point CMake at it if it went somewhere unusual:\n   -DRAYLIB_DIR=/path/to/raylib, or the RAYLIB_DIR environment variable.\n\nExamples:\n cmake -S . -B build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release -DRAYLIB_DIR=/usr/local\n")
  endif()
endif()

# raylib 的静态包在 Linux / FreeBSD 上不携带它自己的链接依赖（X11 / GL /
# dl / pthread / m）。两个后果，都必须在这里处理：
#   其一，缺了它们会在链接期报一堆 undefined reference（首当其冲是 powf 与
#   XInternAtom）；其二，静态库对**顺序**敏感——-lm 若排在 libraylib.a 之前，
#   链接器不会回头解析归档里的 powf。所以这些库挂在 raylib_deps 的 INTERFACE
#   上，保证每个使用者拿到的顺序都是 raylib 在前、平台库在后。
# 逐个 find_library 探测而不是硬写 -lX11：缺哪个就让链接器报哪个，别猜。
if(UNIX AND NOT APPLE)
  find_package(OpenGL REQUIRED)
  find_package(Threads REQUIRED)
  set(raylib_platform_libs OpenGL::GL Threads::Threads ${CMAKE_DL_LIBS})
  foreach(lib_name IN ITEMS X11 Xrandr Xinerama Xcursor Xi rt m)
    find_library(${lib_name}_library ${lib_name})
    if(${lib_name}_library)
      list(APPEND raylib_platform_libs ${${lib_name}_library})
    endif()
  endforeach()
  target_link_libraries(raylib_deps INTERFACE ${raylib_platform_libs})
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Output directory: ${OUTPUT_DIR}")
