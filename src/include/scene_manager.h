#ifndef SCENE_MANAGER_H
#define SCENE_MANAGER_H

#include <raylib.h>
#include <stdbool.h>

typedef struct {
  const char* name;
  void (*Init)(void);
  void (*Update)(void);
  void (*Draw)(void);
  void (*Unload)(void);
  bool isActive;
} Scene;

void SceneManagerInit(void);
void SceneManagerSwitch(Scene scene);
void SceneManagerUpdate(void);
void SceneManagerDraw(void);
void SceneManagerClose(void);
Scene* SceneManagerGetCurrent(void);

#endif