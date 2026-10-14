#include "app.h"
#include "graphsettings.h"
#include "scene_list.h"
#include "scene_manager.h"
#include "strings.h"
#include "ui.h"

// === 临时锚点 ===
//
// 锚点工具尚未做，下面的坐标先手写在这里，等工具定下来再统一调整。
// 布局按 4 的节距：640 不能被 12 整除（640/12 = 53⅓），横向天然节距只有
// 4 ，所以居中类坐标走数学解、不再用 UI_ALIGN12 扭到 12 的倍数——按钮
// x = (640 - 240) / 2 = 200 ，左右余量各 200 ，既是整数也是 4 的倍数。

static const int kTitleSize = 36;
static const int kTitleY = 96;
static const int kMenuSize = 24;
static const int kMenuWidth = 240;
static const int kMenuTopY = 240;

static const char* const kMenuKeys[] = {"menu_start", "menu_settings",
                                        "menu_quit"};
static const int kMenuCount = 3;

// 焦点由场景持有：焦点是按钮唯一的变色来源，界面层不保存跨帧状态。
static int sFocus = 0;

// Ctrl+Q 在标题场景是正常退出，见 README「键位与输入约定」。
// 左右任一 Ctrl 都算。
static bool IsBackKeyPressed(void) {
  return IsKeyPressed(KEY_Q) &&
         (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL));
}

void TitleSceneInit(void) { sFocus = 0; }

void TitleSceneUpdate(void) {
  if (IsKeyPressed(KEY_DOWN)) sFocus = (sFocus + 1) % kMenuCount;
  if (IsKeyPressed(KEY_UP)) sFocus = (sFocus + kMenuCount - 1) % kMenuCount;
  if (IsBackKeyPressed()) AppRequestQuit();
}

void TitleSceneDraw(void) {
  const char* title = GetString("title_text");
  UiSize titleSize = UiMeasureText(title, kTitleSize);
  UiDrawText((GRAPH_DESIGN_WIDTH - titleSize.width) / 2, kTitleY, title,
             kTitleSize, gUiStyle.titleText);

  const int buttonX = (GRAPH_DESIGN_WIDTH - kMenuWidth) / 2;
  const int buttonHeight = UI_BUTTON_HEIGHT(kMenuSize);
  for (int i = 0; i < kMenuCount; i++) {
    const int buttonY = kMenuTopY + i * (buttonHeight + UI_PADDING);
    if (!UiButton(buttonX, buttonY, kMenuWidth, buttonHeight,
                  GetString(kMenuKeys[i]), kMenuSize, sFocus == i))
      continue;
    switch (i) {
      case 0:
        // 游玩界面还没开工，开始游戏先落在测试场景上
        SceneManager_RequestSceneSwitch(&TestScene);
        break;
      case 1:
        SceneManager_RequestSceneSwitch(&SettingsScene);
        break;
      default:
        AppRequestQuit();
        break;
    }
  }
}

void TitleSceneUnload(void) {}

Scene TitleScene = {.SceneInit = TitleSceneInit,
                    .SceneUpdate = TitleSceneUpdate,
                    .SceneDraw = TitleSceneDraw,
                    .SceneUnload = TitleSceneUnload};