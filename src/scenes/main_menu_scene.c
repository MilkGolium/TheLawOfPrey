#include "entity.h"
#include "raylib.h"
#include "scene_manager.h"

// 前向声明设置场景的创建函数
extern Scene CreateSettingsScene(void);
extern Scene CreateTestingScene(void);

static void TestingPageInit(void);
static void TestingPageUpdate(void);
static void TestingPageDraw(void);
static void TestingPageUnload(void);

Scene CreateMainMenuScene(void) {
  return (Scene){.name = "Testing Scene",
                 .Init = TestingPageInit,
                 .Update = TestingPageUpdate,
                 .Draw = TestingPageDraw,
                 .Unload = TestingPageUnload};
}

static void TestingPageInit(void) {
  // 初始化菜单资源
}

static void TestingPageUpdate(void) {
  if (IsKeyPressed(KEY_ENTER)) {
    SceneManagerPush(CreateTestingScene());
  }
  if (IsKeyPressed(KEY_L)) {
    // 进入加载游戏场景
    // SceneManagerPush(CreateLoadGameScene());
  }
  if (IsKeyPressed(KEY_S)) {
    SceneManagerPush(CreateSettingsScene());
  }
}

static void TestingPageDraw(void) {
  ClearBackground(DARKBLUE);
  DrawText("Main Menu", 340, 200, 40, WHITE);
  DrawText("Start Game (Enter)", 340, 260, 20, LIGHTGRAY);
  DrawText("Load Game (L)", 340, 320, 20, LIGHTGRAY);
  DrawText("Settings (S)", 340, 380, 20, LIGHTGRAY);
  DrawText("Quit (Esc)", 340, 440, 20, LIGHTGRAY);
}

static void TestingPageUnload(void) {
  // 清理菜单资源
}
