#include "scene_manager.h"

#include <stdlib.h>  // For NULL

static Scene* currentScene = NULL;

void SceneManagerInit(void) { currentScene = NULL; }

void SceneManagerSwitch(Scene scene) {
  // 1. 卸载旧场景
  if (currentScene != NULL) {
    if (currentScene->Unload) {
      currentScene->Unload();
    }
    currentScene->isActive = false;
  }

  // 2. 复制新场景到静态存储
  static Scene sceneStorage;
  sceneStorage = scene;
  currentScene = &sceneStorage;

  // 3. 初始化新场景
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