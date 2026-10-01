#include <stdbool.h>
#include <stddef.h>

#include "font.h"
#include "graphsettings.h"
#include "raylib.h"
#include "scene_list.h"
#include "scene_manager.h"
#include "theme.h"
#include "utils.h"

int main(void) {
  // === 初始化 ===

  // 硬编码提供设置参数
  const bool fullscreenEnabled = true;  // 未来会去掉硬编码，从配置文件中读取
  int targetFPS = 144;                  // 未来会去掉硬编码，从配置文件中读取

  // 高 DPI：让帧缓冲等于物理像素，避免系统把整个窗口平滑放大成模糊。
  // 必须在 InitWindow 之前设置。
  SetConfigFlags(FLAG_WINDOW_HIGHDPI);

  // 初始化场景管理器
  InitSceneManager();
  SceneManager_SwitchCurrentScene(&MainScene);

  InitWindow(GRAPH_DESIGN_WIDTH, GRAPH_DESIGN_HEIGHT, "The Law Of Prey");
  SetExitKey(KEY_NULL);  // 禁用 ESC 退出按键
  SetTargetFPS(targetFPS);
  SetFullscreen(fullscreenEnabled);

  // 用于切换开发和发行的 assets 位置。优先检查身边，其次根据项目结构来寻找。
  InitAssetsDirectory();

  // 加载中文字体图集，收集 codepoint
  InitGameFont();

  // 字体度量就绪后再套配色，主题只允许改动颜色
  InitTheme();

  // 设计画布：场景画在 640*480 的纹理里，再整数倍放大到窗口
  InitCanvas();

  // === 主循环 ===
  while (!WindowShouldClose()) {
    SceneManager_UpdateCurrentScene();

    // 场景一律画在设计画布内，坐标即设计分辨率坐标
    BeginCanvas();
    ClearBackground(YELLOW);
    SceneManager_DrawCurrentScene();
    EndCanvas();

    // 画布整数倍放大到窗口，未覆盖的部分留作黑边
    BeginDrawing();
    ClearBackground(BLACK);
    DrawCanvas();
    EndDrawing();
  }

  // === 主循环结束，清理并退出 ===

  // 空场景检测已经包含在函数内，无需再次检测
  SceneManager_UnloadCurrentScene();
  // 释放字体图集（必须在 CloseWindow 之前，此时 GL 上下文尚在）
  UnloadGameFont();
  // 释放设计画布，同样必须在 CloseWindow 之前
  UnloadCanvas();
  CloseWindow();
  return 0;
}
