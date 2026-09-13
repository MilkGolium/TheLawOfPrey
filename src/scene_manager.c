#include "scene_manager.h"

#include <stddef.h>

// 初始化 currentScene 指针
static Scene* currentScene = NULL;

// 初始化场景管理器
void SceneManagerInit(void) { currentScene = NULL; }

// 切换场景
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
