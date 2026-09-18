项目构建说明
================

概述
----
本仓库使用 CMake 作为构建系统，推荐配合 Ninja 作为生成器。目标是使用纯 C（C11）构建基于 raylib 的可执行文件，并把构建产物分别放到 build/Release 和 build/Debug 目录。

本文件记录当前构建流程、仓库中与构建相关的文件/目录及其含义、平台依赖以及常见问题与排查办法。

主要目标
--------
- 提供可重复的 Release（已启用性能优化）与 Debug（带调试信息）构建
- 把产物输出到 build/Release 与 build/Debug
- 支持 FreeBSD / OpenBSD / Linux（保证支持），macOS / Windows（尽量支持）
- 为 VS Code 提供编辑器配置（IntelliSense / Tasks / Launch）

代码风格
--------
- C 代码使用 Google Style（.clang-format 中配置），限宽 80 字符。
- 代码注释请使用 ASCII，以保证在终端中可正常显示。
- 保存时自动格式化（VS Code settings.json 中已启用 formatOnSave，使用 clangFormat）。

仓库相关文件与目录
------------------
- CMakeLists.txt
  - 项目根 CMake 配置。
  - 指定 C11，设置按配置（Debug/Release）输出到 build/${CONFIG}。
  - 为 Debug/Release 配置了不同的编译选项（Debug: -g -O0 -DDEBUG；Release: -O3 -march=native -flto -DNDEBUG；Windows/MSVC 有对应的 MSVC 标志分支）。
  - 使用更健壮的 raylib 查找策略：优先使用 find_package(raylib CONFIG)（检测到导入目标 raylib::raylib 则直接链接），若找不到则尝试手动查找头文件与库（find_path/find_library），仍找不到时会给出友好提示并终止配置。
  - 支持通过环境变量 RAYLIB_DIR 或 CMake 变量 -DRAYLIB_DIR=/path/to/raylib 为查找提供提示路径。

- .clang-format
  - 代码格式化配置，基于 Google Style，IndentWidth=2，BreakBeforeBraces=Attach，ColumnLimit=80。

- .gitignore
  - 忽略 build/ 目录和 macOS .DS_Store 文件。

- scripts/
  - build.sh (Unix: macOS / Linux)
    - 脚本会在仓库根下创建 build/Release 与 build/Debug 子目录，分别用 Ninja 生成并构建两套配置。
    - 用法： ./scripts/build.sh
  - build.ps1 (Windows PowerShell)
    - 更灵活的 Windows 构建脚本，支持参数 -Configuration (Release|Debug|Both) 和 -Generator（例如 "Ninja"、"NMake Makefiles"、"Visual Studio 17 2022"）。
    - 内置自动安装 raylib 功能（通过 vcpkg / winget）。
    - 推荐在 Windows 上使用 PowerShell 版本（更强的错误处理与可扩展性）。
    - 用法示例： ./scripts/build.ps1 -Configuration Release
  - build.bat (Windows cmd/batch)
    - 简单的批处理脚本，默认使用 Ninja。提供给不使用 PowerShell 的场景。
    - 同样支持通过 vcpkg 自动安装 raylib。
  - build.ps1.new
    - 空文件，预留用于未来 PowerShell 脚本更新。

- src/
  - main.c — 程序入口，初始化窗口（1024x768，全屏，120 FPS），启动场景管理器。
  - graphsettings.c — 全屏切换相关函数（EnableFullscreen / DisableFullscreen / SetFullscreen）。
  - scene_manager.c — 场景管理器实现，支持场景切换、更新、绘制和卸载。

- src/include/
  - entity.h — Entity 结构体定义（position, has_render, texture）。
  - graphsettings.h — 全屏函数声明。
  - player.h — Player 结构体定义（属性: strength, endurance, constitution, agility, perception; 状态: hunger, thirst; 其他: max_carry_weight, reputation）。
  - scene_manager.h — Scene 结构体和场景管理器 API 声明。

- src/scenes/
  - main_menu_scene.c — 主菜单场景（Enter=开始测试, S=设置, Q=退出）。
  - settings_scene.c — 设置场景（Q=返回主菜单）。
  - testing_scene.c — 测试场景（WASD 移动文字, Q=返回主菜单）。

- assets/
  - 游戏资源目录（当前为空，待填充纹理、音频等资源文件）。

- database/
  - items.csv — 物品数据库，包含 id、name_id、display_name、category、stack_max、texture、description、inspect_text 字段。

- .vscode/
  - c_cpp_properties.json：为 VS Code C/C++ 插件（IntelliSense）提供 includePath，包含 ${env:RAYLIB_DIR}/include、/opt/homebrew/include、/usr/local/include 等常见路径。支持 macOS / Linux / Win32 三种配置。
  - tasks.json：提供 CMake Configure/Build 任务（Release/Debug）以及直接运行 scripts/build.sh 的任务。
  - launch.json：提供 Debug 配置（使用 lldb，preLaunchTask 为 Build Debug）。
  - settings.json：配置 clangFormat 格式化、保存时自动格式化、缩进 2 空格。

- doc/
  - BUILD.md — 本文件，项目构建说明。
  - TipsOfVSCode.md — VS Code 操作技巧（标签页切换、分屏等）。

构建输出
--------
所有构建产物（可执行文件、库）会放在仓库下的 build/<Config> 目录中：
- build/Release/TheLawOfPrey
- build/Debug/TheLawOfPrey

注意：build/ 目录在 .gitignore 中，不纳入版本控制。

平台支持
--------
根据 README，项目平台支持策略如下：
- 保证支持：FreeBSD、OpenBSD、Linux
- 尽量支持：macOS、Windows

平台依赖与安装建议
-----------------
1. macOS (推荐 Homebrew)
   - 安装工具：
     brew install cmake ninja raylib
   - 若是 Apple Silicon（M1/M2），Homebrew 的默认前缀可能是 /opt/homebrew，CMake 脚本和 .vscode 已包含 /opt/homebrew/include 与 /opt/homebrew/lib 的提示。
   - 注意：macOS 为"尽量支持"平台，可能需要额外适配。

2. Ubuntu/Debian（示例）
   - 安装工具（包名随发行版可能不同）：
     sudo apt update
     sudo apt install build-essential cmake ninja-build libraylib-dev
   - 如果系统仓库没有 libraylib-dev，请参照 raylib 官方文档编译并安装 raylib。

3. Fedora / Arch
   - Fedora: sudo dnf install cmake ninja-build raylib raylib-devel
   - Arch: sudo pacman -S cmake ninja raylib

4. FreeBSD / OpenBSD
   - 项目保证支持的平台。可使用 pkg 或 ports 安装 raylib：
     - FreeBSD: pkg install raylib
     - OpenBSD: pkg_add raylib
   - 若包管理器中无 raylib，需从源码编译安装。

5. Windows
   - 建议安装：cmake、ninja、PowerShell（内置）以及 raylib 的 Windows 版本。
   - 如果运行 `scripts\build.ps1` 或 `scripts\build.bat` 时 CMake 提示找不到 raylib，脚本会尝试自动安装 raylib（通过 vcpkg / winget 进行安装）。
   - raylib 也可通过 vcpkg 手动安装（推荐管理第三方依赖）：
     - 安装 vcpkg 并集成： https://github.com/microsoft/vcpkg
     - vcpkg install raylib
   - 或使用预编译的 raylib 库并通过 -DRAYLIB_DIR=... 指定路径。
   - 注意：Windows 为"尽量支持"平台，且因缺少代码签名证书，SmartScreen 可能拦截运行，具体情况待测试。

VS Code 使用说明
----------------
- Intellisense: .vscode/c_cpp_properties.json 已包含常见的 include 路径，支持 macOS / Linux / Win32 三种配置。若编辑器仍报找不到 raylib.h，可以：
  - 在系统环境中设置 RAYLIB_DIR（指向 raylib 安装目录），或
  - 编辑 .vscode/c_cpp_properties.json 将正确的 include 路径加入 includePath。

- 代码格式化：已配置 clangFormat（Google Style, 80 字符限宽），保存时自动格式化。

- 构建任务：Terminal -> Run Task -> 选择 "Build Debug"、"Build Release" 或 "Run scripts/build.sh"。
- 调试：选择 Run and Debug -> 选择 "Launch Debug executable"（会触发 preLaunchTask: Build Debug，然后启动调试）。

RAYLIB 定位策略与常见问题
----------------------
- CMake 首先尝试使用 raylib 的 CMake 包（即安装时提供的 raylibConfig.cmake / raylib-config.cmake），这会创建一个导入目标 raylib::raylib 并自动处理头文件与库的路径。
- 如果 find_package(... CONFIG) 未找到，CMake 会尝试手动通过 find_path/find_library 查找 raylib.h 与 libraylib（或 libraylib.dylib），这时可以通过设置环境变量 RAYLIB_DIR 或 -DRAYLIB_DIR=/path/to/raylib 为查找提供提示路径。
- 若仍然找不到，CMake 配置阶段会报错并给出安装建议（brew、apt 或源码构建）。

常见问题排查
-------------
- 问题：CMake 报找不到 raylib
  - 排查：确认 raylib 已安装在系统路径，或使用 -DRAYLIB_DIR 指定安装前缀；检查 raylib 的 include 和 lib 是否存在（例如 /usr/local/include/raylib.h 与 /usr/local/lib/libraylib.*）。

- 问题：VS Code 中 main.c 对 raylib.h 显示波浪线（找不到头文件）
  - 排查：在终端运行 cmake 并观察 find 时的输出（scripts/build.sh 会显示 CMake 的查找结果）；在 .vscode/c_cpp_properties.json 中增加正确的 include 路径或设置环境变量 RAYLIB_DIR。

- 问题：Release 构建在某些平台上使用 -march=native 或 -flto 导致兼容性或链接问题
  - 说明：CMakeLists.txt 中在非 MSVC 平台为 Release 指定了 -O3 -march=native -flto -DNDEBUG，这提高性能但可能导致跨平台二进制不可移植或部分编译器/链接器不支持 flto。可按需修改或在命令行覆盖 CFLAGS。

- 问题：Windows 上 SmartScreen 拦截运行
  - 说明：因项目缺少代码签名证书，Windows 可能弹出 SmartScreen 警告。可暂时通过"仍要运行"绕过，具体情况待进一步测试。

CI / 自动化建议
---------------
- 在 CI（例如 GitHub Actions）上为 Linux 和 Windows 添加矩阵构建：
  - Linux: 使用 Ubuntu 最新镜像，安装 cmake、ninja、raylib（或在 CI 中构建 raylib）
  - FreeBSD: 可使用 Cirrus CI 或其他支持 FreeBSD 的 CI 服务
  - Windows: 使用 vcpkg 或在 runner 上安装 raylib，或在 CI 中在 Windows 上构建 raylib 子模块

- 可选：将 raylib 作为 FetchContent/ExternalProject 或 git submodule 集成到 CMake 中，以便在目标机器上自动构建 raylib（当系统包不可用时）。该方法方便 CI 一致构建，但会增加构建时间。

项目状态
--------
- 项目正在构建程序基础，游戏内容待定。
- 项目名称为临时设置，未来可能会修改。
- 代码注释当前可能包含中文，未来会统一使用 ASCII。

摘要（快速命令）
----------------
- macOS (使用 Homebrew):
  brew install cmake ninja raylib
  ./scripts/build.sh

- Ubuntu/Debian (示例):
  sudo apt update
  sudo apt install build-essential cmake ninja-build libraylib-dev
  ./scripts/build.sh

- FreeBSD:
  pkg install cmake ninja raylib
  ./scripts/build.sh

- Windows (PowerShell, 推荐):
  # 若需要绕过执行策略：
  powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1 -Configuration Both

- 使用 CMake 手动构建（任意平台）：
  cmake -S . -B build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release
  cmake --build build/Release --config Release
