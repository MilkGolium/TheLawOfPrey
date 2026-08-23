// 主场景

#include "raylib.h"
#include "scene_manager.h"

extern Scene CreateSettingsScene(void);
extern Scene CreateTestingScene(void);
// extern Scene CreateLoadGameScene(void);

static void MainMenuInit(void);
static void MainMenuUpdate(void);
static void MainMenuDraw(void);
static void MainMenuUnload(void);

static Texture2D bgTexture;

// 辅助函数
static inline float float_max(float a, float b) { return (a > b) ? a : b; }

Scene CreateMainMenuScene(void) {
  return (Scene){.name = "Main Menu",
                 .Init = MainMenuInit,
                 .Update = MainMenuUpdate,
                 .Draw = MainMenuDraw,
                 .Unload = MainMenuUnload};
}

static void MainMenuInit(void) {
  bgTexture = LoadTexture("assets/img/main_scene/main_scene_bg.png");
  SetTextureFilter(bgTexture, TEXTURE_FILTER_BILINEAR);
}

static void MainMenuUpdate(void) {
  if (IsKeyPressed(KEY_ENTER)) {
    SceneManagerSwitch(CreateTestingScene());
  }
  if (IsKeyPressed(KEY_S)) {
    SceneManagerSwitch(CreateSettingsScene());
  }
  if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Q)) {
    SceneManagerClose();
  }
}

static void MainMenuDraw(void) {
  ClearBackground(DARKBLUE);

  // 2. 动态计算铺满屏幕的绘制矩形（Cover 模式，按比例裁剪多余部分）
  float screenWidth = (float)GetScreenWidth();
  float screenHeight = (float)GetScreenHeight();

  // 源矩形（整张大图）
  Rectangle srcRect = {0.0f, 0.0f, (float)bgTexture.width,
                       (float)bgTexture.height};

  // 计算缩放比例，取能覆盖全屏的最大比例
  float scale =
      float_max(screenWidth / bgTexture.width, screenHeight / bgTexture.height);

  // 目标矩形（居中绘制）
  float destWidth = bgTexture.width * scale;
  float destHeight = bgTexture.height * scale;
  Rectangle destRect = {(screenWidth - destWidth) / 2.0f,
                        (screenHeight - destHeight) / 2.0f, destWidth,
                        destHeight};

  // 绘制背景图
  DrawTexturePro(bgTexture, srcRect, destRect, (Vector2){0, 0}, 0.0f, WHITE);

  DrawText("Main Menu", 65, 50, 60, WHITE);
  DrawText("Start Game [Enter]", 137, 186, 20, LIGHTGRAY);
  DrawText("Settings [S]", 137, 266, 20, LIGHTGRAY);
  DrawText("Quit [Ctrl-Q]", 137, 350, 20, LIGHTGRAY);
}

static void MainMenuUnload(void) {
  // 3. 必须在 Unload 中卸载纹理，防止显存泄漏
  UnloadTexture(bgTexture);
}
