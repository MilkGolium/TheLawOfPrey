#include "scene_manager.h"
#include "strings.h"
#include "ui.h"

static bool showMessageBox;
static int messageBoxFocus;

void MainSceneInit(void) {
  showMessageBox = false;
  messageBoxFocus = 0;
}

void MainSceneUpdate(void) {}

void MainSceneDraw(void) {
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

void MainSceneUnload(void) {}

Scene MainScene = {.SceneInit = MainSceneInit,
                   .SceneUpdate = MainSceneUpdate,
                   .SceneDraw = MainSceneDraw,
                   .SceneUnload = MainSceneUnload};