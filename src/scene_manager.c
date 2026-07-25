#include "scene_manager.h"

#include <stddef.h>

static Scene* currentScene = NULL;

void SceneManagerInit(void) { currentScene = NULL; }

void SceneManagerSwitch(Scene scene) {
  if (currentScene != NULL) {
    if (currentScene->Unload) {
      currentScene->Unload();
    }
    currentScene->isActive = false;
  }

  static Scene sceneStorage;
  sceneStorage = scene;
  currentScene = &sceneStorage;

  currentScene->isActive = true;
  if (currentScene->Init) {
    currentScene->Init();
  }
}

void SceneManagerUpdate(void) {
  if (currentScene != NULL && currentScene->isActive && currentScene->Update) {
    currentScene->Update();
  }
}

void SceneManagerDraw(void) {
  if (currentScene != NULL && currentScene->isActive && currentScene->Draw) {
    currentScene->Draw();
  }
}

void SceneManagerClose(void) {
  if (currentScene != NULL) {
    if (currentScene->Unload) {
      currentScene->Unload();
    }
    currentScene = NULL;
  }
}

Scene* SceneManagerGetCurrent(void) { return currentScene; }
