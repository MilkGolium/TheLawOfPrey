#include <stddef.h>

#include "font.h"
#include "graphsettings.h"
#include "raylib.h"
#include "scene_list.h"
#include "scene_manager.h"
#include "utils.h"

// 注意：此定义只需要进行一次，用于启用 Raygui
// 其它位置无需再定义，直接头文件即可
// raygui.h 在 main.c 中未必会用到，但是必须接在 define 语句后面
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

int main(void) {
  // === 初始化 ===

  // 硬编码提供设置参数
  const int screenWidth = 1920;         // 未来会去掉硬编码，从配置文件中读取
  const int screenHeight = 1080;        // 未来会去掉硬编码，从配置文件中读取
  const bool fullscreenEnabled = true;  // 未来会去掉硬编码，从配置文件中读取
  int targetFPS = 144;                  // 未来会去掉硬编码，从配置文件中读取

  // 初始化场景管理器
  InitSceneManager();
  SceneManager_SwitchCurrentScene(&MainScene);

  InitWindow(screenWidth, screenHeight, "The Law Of Prey");
  SetExitKey(KEY_NULL);  // 禁用 ESC 退出按键
  SetTargetFPS(targetFPS);
  SetFullscreen(fullscreenEnabled);

  // 用于切换开发和发行的 assets 位置。优先检查身边，其次根据项目结构来寻找。
  InitAssetsDirectory();

  // 加载中文字体图集，收集 codepoint 并同步 raygui
  InitGameFont();

  // === 主循环 ===
  while (!WindowShouldClose()) {
    SceneManager_UpdateCurrentScene();

    BeginDrawing();
    ClearBackground(BLUE);
    SceneManager_DrawCurrentScene();
    EndDrawing();
  }

  // === 主循环结束，清理并退出 ===

  // 空场景检测已经包含在函数内，无需再次检测
  SceneManager_UnloadCurrentScene();
  // 释放字体图集（必须在 CloseWindow 之前，此时 GL 上下文尚在）
  UnloadGameFont();
  CloseWindow();
  return 0;
}
