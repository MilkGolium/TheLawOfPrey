#include "scene_manager.h"

#include <stddef.h>

// 当前场景
static Scene* currentScene;

// 初始化
void SceneManagerInit(void) { currentScene = NULL; }

// 实现 SceneMagaerSwitch() ，用于切换场景
int SceneManagerSwitch(Scene nextScene) {
  // 检查当前是否存在场景，如果存在，将当前场景卸载，清理资源
  if (currentScene != NULL) {
    currentScene->SceneUnload();
    // 将 currentScene 指向新的场景
    currentScene = &nextScene;
    // 顺便初始化新场景
    currentScene->SceneInit();
    return 0;
  } else {
    return 1;
  }
}

// 更新当前场景的数据
void SceneManagerUpdateCurrentScene(void) {
  if (currentScene != NULL) {
    currentScene->SceneUpdate();
  }
}

// 绘制场景
void SceneManagerDraw(void) {
  if (currentScene != NULL) {
    currentScene->SceneDraw();
  }
}

// 清理当前场景的数据
void SceneManagerUnloadCurrentScene(void) {
  if (currentScene != NULL) {
    // 清理场景的数据
    currentScene->SceneUnload();
    // 将当前场景设置为 NULL
    currentScene = NULL;
  }
}

// 获取当前场景的指针
Scene* SceneManagerGetCurrentScene(void) { return currentScene; }
