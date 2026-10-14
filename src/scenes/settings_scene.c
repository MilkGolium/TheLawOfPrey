#include "graphsettings.h"
#include "scene_list.h"
#include "scene_manager.h"
#include "strings.h"
#include "ui.h"

// 设置界面当前只做占位：主题切换等设置项留到正式做设置界面的那一批改动里
// 一起落地（README「主题」一节），这里不预置任何可调项。

// 临时锚点，与标题场景同一批手写，等锚点工具定下来再调整。
static const int kTextSize = 24;
static const int kPlaceholderY = 180;
static const int kHintY = 240;

// Ctrl+Q 是「返回」：设置界面回标题，见 README「键位与输入约定」。
// 左右任一 Ctrl 都算。
static bool IsBackKeyPressed(void) {
  return IsKeyPressed(KEY_Q) &&
         (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL));
}

// 文本水平居中后向上取整到 12 的倍数，与标题场景同一条取舍：画布宽 640
// 不是 12 的倍数，严格居中与网格对齐不可兼得，此处按像素网格优先。
static int CenteredX(const char* text, int size) {
  return UI_ALIGN12((GRAPH_DESIGN_WIDTH - UiMeasureText(text, size).width) / 2);
}

void SettingsSceneInit(void) {}

void SettingsSceneUpdate(void) {
  if (IsBackKeyPressed()) SceneManager_RequestSceneSwitch(&TitleScene);
}

void SettingsSceneDraw(void) {
  const char* placeholder = GetString("settings_placeholder");
  UiDrawText(CenteredX(placeholder, kTextSize), kPlaceholderY, placeholder,
             kTextSize, gUiStyle.text);

  const char* hint = GetString("settings_back_hint");
  UiDrawText(CenteredX(hint, kTextSize), kHintY, hint, kTextSize,
             gUiStyle.textDim);
}

void SettingsSceneUnload(void) {}

Scene SettingsScene = {.SceneInit = SettingsSceneInit,
                       .SceneUpdate = SettingsSceneUpdate,
                       .SceneDraw = SettingsSceneDraw,
                       .SceneUnload = SettingsSceneUnload};