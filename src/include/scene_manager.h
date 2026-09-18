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
// 切换当前场景并自动初始化；需要传入一个场景结构；成功后会返回 1 ，失败返回 0
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
