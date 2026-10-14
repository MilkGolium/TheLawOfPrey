#include "scene_list.h"
#include "scene_manager.h"
#include "strings.h"
#include "ui.h"

// 测试场景。它是 UI 原语与消息框的演示场地，也是「开始游戏」当前的落点：
// 游玩界面出现之前，标题场景的开始游戏按钮切到这里，需要临时验证什么
// 就直接加在这个文件里。定位与键位见 README「游玩闭环」与「键位与输入约定」。

static bool showMessageBox;
static int messageBoxFocus;

// Ctrl+Q 是「返回」：测试场景回标题，若当前有消息框打开则先关框
// （这一次按键只关框，再按才回标题）。ESC 关框的行为保持不变。
static bool IsBackKeyPressed(void) {
  return IsKeyPressed(KEY_Q) &&
         (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL));
}

void TestSceneInit(void) {
  showMessageBox = false;
  messageBoxFocus = 0;
}

void TestSceneUpdate(void) {
  if (!IsBackKeyPressed()) return;
  if (showMessageBox) {
    showMessageBox = false;
    return;
  }
  SceneManager_RequestSceneSwitch(&TitleScene);
}

void TestSceneDraw(void) {
  if (UiButton(24, 24, 120, 36, GetString("button_show_message"), 24,
               !showMessageBox))
    showMessageBox = true;

  if (showMessageBox) {
    const char* buttons[] = {GetString("button_ok"), GetString("button_close")};
    int result = UiMessageBox(GetString("messagebox_title"),
                              GetString("messagebox_body"), buttons, 2, 24,
                              &messageBoxFocus);
    if (result != -2) showMessageBox = false;
  }
}

void TestSceneUnload(void) {}

Scene TestScene = {.SceneInit = TestSceneInit,
                   .SceneUpdate = TestSceneUpdate,
                   .SceneDraw = TestSceneDraw,
                   .SceneUnload = TestSceneUnload};