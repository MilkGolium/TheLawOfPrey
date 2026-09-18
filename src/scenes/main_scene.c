#include "scene_list.h"
#include "scene_manager.h"

void MainSceneInit(void) { ; }

void MainSceneUpdate(void) { ; }

void MainSceneDraw(void) { ; }

void MainSceneUnload(void) { ; }

Scene MainScene = {.SceneInit = MainSceneInit,
                   .SceneUpdate = MainSceneUpdate,
                   .SceneDraw = MainSceneDraw,
                   .SceneUnload = MainSceneUnload};
