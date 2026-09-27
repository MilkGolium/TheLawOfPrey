#include <raylib.h>

#include "raygui.h"
#include "scene_list.h"
#include "scene_manager.h"

bool showMessageBox;

void MainSceneInit(void) { showMessageBox = false; }

void MainSceneUpdate(void) { ; }

void MainSceneDraw(void) {
  if (GuiButton((Rectangle){24, 24, 250, 40}, "#191#显示消息"))
    showMessageBox = true;

  if (showMessageBox) {
    int btnActive = -1;
    GuiMessageBox((Rectangle){85, 70, 400, 100}, "#191#消息框",
                  "你好！这是一条测试消息。", "好的;关闭", &btnActive);

    if (btnActive >= 0) showMessageBox = false;
  }
}

void MainSceneUnload(void) { ; }

Scene MainScene = {.SceneInit = MainSceneInit,
                   .SceneUpdate = MainSceneUpdate,
                   .SceneDraw = MainSceneDraw,
                   .SceneUnload = MainSceneUnload};
