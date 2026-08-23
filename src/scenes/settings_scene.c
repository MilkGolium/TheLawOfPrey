// 设置界面

#include "raylib.h"
#include "scene_manager.h"

#define TEXT_BTN_TITLE "设置 Settings"
// 宏拼接字符串
#define ALL_SETTINGS_TEXT TEXT_BTN_TITLE

extern Scene CreateMainMenuScene(void);

static void SettingsInit(void);
static void SettingsUpdate(void);
static void SettingsDraw(void);
static void SettingsUnload(void);

static Font UIFont;

Scene CreateSettingsScene(void) {
  return (Scene){.name = "Settings",
                 .Init = SettingsInit,
                 .Update = SettingsUpdate,
                 .Draw = SettingsDraw,
                 .Unload = SettingsUnload};
}

static void SettingsInit(void) {
  // codepoint 计数（码点，在 Unicode 标准中一般写作 U+xxxx，U+十六进制数值）
  int codepointCount = 0;
  // 此指针指向一个存储着所有需要加载的文字的码点
  int* codepoints = LoadCodepoints(ALL_SETTINGS_TEXT, &codepointCount);
  // Zpix 像素字体是 12px 的，所以 fontSize 传入 12
  // 码点数组 codepoints 被传入以后， LoadFontEx
  // 会使用循环来逐个加载进入纹理，texture 无需单独声明，它被包含在前面声明的
  // Font 类型的变量里面。
  UIFont =
      LoadFontEx("assets/fonts/ttf/zpix.ttf", 12, codepoints, codepointCount);
  SetTextureFilter(UIFont.texture, TEXTURE_FILTER_POINT);
  UnloadCodepoints(codepoints);
}

static void SettingsUpdate(void) {
  // 按 ESC 回到主场景
  if (IsKeyPressed(KEY_ESCAPE)) {
    SceneManagerSwitch(CreateMainMenuScene());
  }
}

static void SettingsDraw(void) {
  ClearBackground(DARKGRAY);

  const float fontSize = 24.0f;
  const float spacing = 1.0f;
  // 计算文本宽高
  const Vector2 textSize =
      MeasureTextEx(UIFont, ALL_SETTINGS_TEXT, fontSize, spacing);
  // 根据刚刚算出的文本宽高，将文本放在屏幕中央
  const Vector2 TitleTextPos = {(GetScreenWidth() - textSize.x) / 2.0f,
                                (GetScreenHeight() - fontSize) / 2.0f};
  DrawTextEx(UIFont, "设置 Settings", TitleTextPos, fontSize, spacing, WHITE);
}

static void SettingsUnload(void) { UnloadFont(UIFont); }
