#include "scene_manager.h"
#include "ui.h"

static bool showMessageBox;
static int messageBoxFocus;

void MainSceneInit(void) {
  showMessageBox = false;
  messageBoxFocus = 0;
}

void MainSceneUpdate(void) {}

void MainSceneDraw(void) {
  if (UiButton(24, 24, 120, 36, "显示消息", 24, !showMessageBox))
    showMessageBox = true;

  if (showMessageBox) {
    const char* buttons[] = {"好的", "关闭"};
    int result = UiMessageBox("消息框", "你好！这是一条测试消息。", buttons, 2,
                              24, &messageBoxFocus);
    if (result != -2) showMessageBox = false;
  }
}

void MainSceneUnload(void) {}

Scene MainScene = {.SceneInit = MainSceneInit,
                   .SceneUpdate = MainSceneUpdate,
                   .SceneDraw = MainSceneDraw,
                   .SceneUnload = MainSceneUnload};
