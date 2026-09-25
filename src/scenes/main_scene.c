#include <raylib.h>

#include "raygui.h"
#include "scene_list.h"
#include "scene_manager.h"

bool showMessageBox;

void MainSceneInit(void) { showMessageBox = false; }

void MainSceneUpdate(void) { ; }

void MainSceneDraw(void) {
  if (GuiButton((Rectangle){24, 24, 250, 40}, "#191#Show Message"))
    showMessageBox = true;

  if (showMessageBox) {
    int btnActive = -1;
    GuiMessageBox((Rectangle){85, 70, 400, 100}, "#191#Message Box",
                  "Hi! This is a message!", "Nice;Cool", &btnActive);

    if (btnActive >= 0) showMessageBox = false;
  }
}

void MainSceneUnload(void) { ; }

Scene MainScene = {.SceneInit = MainSceneInit,
                   .SceneUpdate = MainSceneUpdate,
                   .SceneDraw = MainSceneDraw,
                   .SceneUnload = MainSceneUnload};
