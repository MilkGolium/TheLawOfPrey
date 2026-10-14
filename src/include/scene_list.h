#ifndef SCENE_LIST_H
#define SCENE_LIST_H

#include "scene_manager.h"

// 场景清单。场景本体在 src/scenes/ 下，文件名与 --scene 的取值一致。
extern Scene TitleScene;
extern Scene SettingsScene;
extern Scene TestScene;
// 游玩界面还没开工，本体随「游玩场景骨架」那一批改动落地。这里先留出
// 声明占位：SceneListFindByName 还不接受 game 取值。
extern Scene GameScene;

// 按名字查场景，取值为 title / settings / test。找不到时返回 NULL，
// 由调用方报错，不静默回落到标题场景。
Scene* SceneListFindByName(const char* name);

#endif  // SCENE_LIST_H