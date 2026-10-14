#ifndef SCENE_MANAGER_H
#define SCENE_MANAGER_H

#include <raylib.h>
#include <stdbool.h>

// 注意：场景结构中提供的函数用于操作场景本身，而场景管理器函数用于管理场景管理器，以及控制场景，这点不要混淆，区分开
// SceneInit() 和 SceneManagerInitd() 。

// 定义场景。场景不允许叠加，所以场景不需要记录是否激活（isActive）或者是否展示；如果需要使用叠加功能，应当使用函数把
// UI 和底图分别打包。
// 此场景结构仅提供函数指针，实际使用需要在每个场景中定义这些函数。
typedef struct {
  // 函数指针

  // 初始化场景
  void (*SceneInit)(void);
  // 更新场景数据
  void (*SceneUpdate)(void);
  // 绘制场景
  void (*SceneDraw)(void);
  // 清理释放资源
  void (*SceneUnload)(void);
} Scene;

// 初始化场景管理器
void InitSceneManager(void);
// 提出切换场景的请求。只记录请求，不立即执行：场景不得在自己的回调里
// 切换自己，切换由主循环在任何场景回调之外统一执行。
void SceneManager_RequestSceneSwitch(Scene* nextScene);
// 执行挂起的切换请求。主循环每帧开头调用一次，没有请求时什么都不做。
// 同一帧内多次请求时后一次覆盖前一次，只有最后一次生效。
void SceneManager_ProcessPendingSwitch(void);
// 切换当前场景并自动初始化。这是底层实现，由 ProcessPendingSwitch 调用，
// 调用点必须已经离开所有场景回调。
void SceneManager_SwitchCurrentScene(Scene*);
// 更新当前场景
void SceneManager_UpdateCurrentScene(void);
// 绘制当前场景
void SceneManager_DrawCurrentScene(void);
// 清理释放资源
void SceneManager_UnloadCurrentScene(void);
// 查询当前场景，返回一个场景结构的指针
Scene* SceneManager_GetCurrentScene(void);

#endif
