#include "raylib.h"
#include "scene_manager.h"

extern Scene CreateMainMenuScene(void);

static void SettingsInit(void);
static void SettingsUpdate(void);
static void SettingsDraw(void);
static void SettingsUnload(void);

static const char *kQuote =
    "我们度过的每个平凡的日常，也许就是连续发生的奇迹。";
static Font QuoteFont;

Scene CreateSettingsScene(void) {
  return (Scene){.name = "Settings",
                 .Init = SettingsInit,
                 .Update = SettingsUpdate,
                 .Draw = SettingsDraw,
                 .Unload = SettingsUnload};
}

static void SettingsInit(void) {
  int codepointCount = 0;
  int *codepoints = LoadCodepoints(kQuote, &codepointCount);
  // 16px ark-pixel is an incomplete subset; 12px build covers all CJK.
  QuoteFont = LoadFontEx(
      "assets/fonts/ttf/ark_pixel/12px-proportional/"
      "ark-pixel-12px-proportional-zh_cn.ttf",
      36, codepoints, codepointCount);
  UnloadCodepoints(codepoints);
}

static void SettingsUpdate(void) {
  if (IsKeyPressed(KEY_ESCAPE)) {
    SceneManagerSwitch(CreateMainMenuScene());
  }
}

static void SettingsDraw(void) {
  ClearBackground(DARKGRAY);
  DrawText("Settings", 350, 280, 40, WHITE);

  const float fontSize = 36.0f;
  const float spacing = 1.0f;
  const Vector2 textSize = MeasureTextEx(QuoteFont, kQuote, fontSize, spacing);
  const Vector2 textPos = {(GetScreenWidth() - textSize.x) / 2.0f,
                           (GetScreenHeight() - fontSize) / 2.0f};
  DrawTextEx(QuoteFont, kQuote, textPos, fontSize, spacing, WHITE);
}

static void SettingsUnload(void) { UnloadFont(QuoteFont); }
