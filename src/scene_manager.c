#include "scene_manager.h"

#include <stddef.h>

// 声明变量存储当前场景；此为内部数据，不应向其他源代码文件暴露。
static Scene* sCurrentScene;

// 挂起的切换请求。场景只提请求，真正切换由主循环在本帧所有场景回调之外
// 执行，因此这里不会在回调进行中被取用。
static Scene* sPendingScene;

// 初始化场景管理器（注意不是初始化场景）；此函数应该在进入游戏主循环之前执行。
void InitSceneManager(void) {
  sCurrentScene = NULL;
  sPendingScene = NULL;
}

// 初始化当前场景
void SceneManager_InitCurrentScene(void) {
  if (sCurrentScene != NULL) {
    sCurrentScene->SceneInit();
  }
}

// 记录切换请求。只写请求，不碰当前场景。
void SceneManager_RequestSceneSwitch(Scene* nextScene) {
  sPendingScene = nextScene;
}

// 执行挂起的请求。放在主循环开头，因此新场景的 Init 一定发生在
// 上一帧的 Update 与 Draw 全部返回之后，不会出现回调中途换场景。
void SceneManager_ProcessPendingSwitch(void) {
  if (sPendingScene == NULL) return;
  SceneManager_SwitchCurrentScene(sPendingScene);
  sPendingScene = NULL;
}

// 用于切换“当前场景”的函数，自动为新场景进行初始化
void SceneManager_SwitchCurrentScene(Scene* nextScene) {
  // 检查当前是否存在场景，如果存在，将当前场景卸载，清理资源
  if (sCurrentScene != NULL) {
    sCurrentScene->SceneUnload();
  }
  // 将 currentScene 指向新的场景
  sCurrentScene = nextScene;
  // 顺便初始化新场景
  sCurrentScene->SceneInit();
}

// 更新当前场景的数据
void SceneManager_UpdateCurrentScene(void) {
  if (sCurrentScene != NULL) {
    sCurrentScene->SceneUpdate();
  }
}

// 绘制场景
void SceneManager_DrawCurrentScene(void) {
  if (sCurrentScene != NULL) {
    sCurrentScene->SceneDraw();
  }
}

// 清理当前场景的数据
void SceneManager_UnloadCurrentScene(void) {
  if (sCurrentScene != NULL) {
    // 清理场景的数据
    sCurrentScene->SceneUnload();
    // 将当前场景设置为 NULL
    sCurrentScene = NULL;
  }
}

// 获取当前场景的指针
Scene* SceneManager_GetCurrentScene(void) { return sCurrentScene; }