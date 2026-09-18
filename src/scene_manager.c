#include "scene_manager.h"

#include <stddef.h>

// 声明变量存储当前场景；此为内部数据，不应向其他源代码文件暴露。
static Scene* currentScene;

// 初始化场景管理器（注意不是初始化场景）；此函数应该在进入游戏主循环之前执行。
void InitSceneManager(void) { currentScene = NULL; }

// 初始化当前场景
void SceneManager_InitCurrentScene(void) {
  if (currentScene != NULL) {
    currentScene->SceneInit();
  }
}

// 用于切换“当前场景”的函数，自动为新场景进行初始化
void SceneManager_SwitchCurrentScene(Scene* nextScene) {
  // 检查当前是否存在场景，如果存在，将当前场景卸载，清理资源
  if (currentScene != NULL) {
    currentScene->SceneUnload();
  }
  // 将 currentScene 指向新的场景
  currentScene = nextScene;
  // 顺便初始化新场景
  currentScene->SceneInit();
}

// 更新当前场景的数据
void SceneManager_UpdateCurrentScene(void) {
  if (currentScene != NULL) {
    currentScene->SceneUpdate();
  }
}

// 绘制场景
void SceneManager_DrawCurrentScene(void) {
  if (currentScene != NULL) {
    currentScene->SceneDraw();
  }
}

// 清理当前场景的数据
void SceneManager_UnloadCurrentScene(void) {
  if (currentScene != NULL) {
    // 清理场景的数据
    currentScene->SceneUnload();
    // 将当前场景设置为 NULL
    currentScene = NULL;
  }
}

// 获取当前场景的指针
Scene* SceneManager_GetCurrentScene(void) { return currentScene; }
