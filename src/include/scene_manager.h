#ifndef SCENE_MANAGER_H
#define SCENE_MANAGER_H

#include <raylib.h>
#include <stdbool.h>

typedef struct {
  void(*SceneManagerInit);
  void(*SceneManagerUpdate);
  void(*SceneManagerDraw);
  void(*SceneManagerUnload);
} Scene;

void SceneManagerInit(void);
void SceneManagerSwitch(Scene scene);
void SceneManagerUpdate(void);
void SceneManagerDraw(void);
void SceneManagerClose(void);
Scene* SceneManagerGetCurrent(void);

#endif
