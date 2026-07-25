#include "raylib.h"
#include "scene_manager.h"

// 前向声明设置场景的创建函数
extern Scene CreateSettingsScene(void);

static void MainMenuInit(void);
static void MainMenuUpdate(void);
static void MainMenuDraw(void);
static void MainMenuUnload(void);

Scene CreateMainMenuScene(void) {
  return (Scene){.name = "MainMenu",
                 .Init = MainMenuInit,
                 .Update = MainMenuUpdate,
                 .Draw = MainMenuDraw,
                 .Unload = MainMenuUnload};
}

static void MainMenuInit(void) {
  // 初始化菜单资源
}

static void MainMenuUpdate(void) {
  // 按S进入设置
  if (IsKeyPressed(KEY_S)) {
    SceneManagerPush(CreateSettingsScene());
  }
}

static void MainMenuDraw(void) {
  ClearBackground(DARKBLUE);
  DrawText("Main Menu", 340, 200, 40, WHITE);
  DrawText("Press S for Settings", 310, 260, 20, LIGHTGRAY);
  DrawText("Press ESC to Quit", 320, 300, 20, LIGHTGRAY);
}

static void MainMenuUnload(void) {
  // 清理菜单资源
}