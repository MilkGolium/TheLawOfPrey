#include <stddef.h>

#include "graphsettings.h"
#include "raylib.h"
#include "scene_list.h"
#include "scene_manager.h"
#include "utils.h"

extern Scene CreateMainMenuScene(void);

int main(void) {
  const int screenWidth = 1920;
  const int screenHeight = 1080;
  const bool fullscreenEnabled = true;  // 未来会去掉硬编码，从配置文件中读取
  int targetFPS = 144;                  // 未来会去掉硬编码，从配置文件中读取

  InitWindow(screenWidth, screenHeight, "The Law Of Prey");
  SetExitKey(KEY_NULL);  // 禁用 ESC 退出按键
  SetTargetFPS(targetFPS);
  SetFullscreen(fullscreenEnabled);

  // 用于切换开发和发行的 assets 位置。优先检查身边，其次根据项目结构来寻找。
  InitAssetsDirectory();

  // 主循环
  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(BLUE);
    MainScene.SceneDraw();
    EndDrawing();
  }

  CloseWindow();
  return 0;
}
