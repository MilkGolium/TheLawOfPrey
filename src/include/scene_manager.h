#ifndef SCENE_MANAGER_H
#define SCENE_MANAGER_H

#include <raylib.h>
#include <stdbool.h>

#define MAX_SCENES 10

typedef struct {
  const char* name;
  void (*Init)(void);    // Call when entering scene
  void (*Update)(void);  // Updating variables every frame
  void (*Draw)(void);    // Drawing every frame
  void (*Unload)(void);  // Clean when exiting the scene
  bool isActive;
} Scene;

void SceneManagerInit(void);
void SceneManagerPush(Scene scene);
void SceneManagerPop(void);
void SceneManagerSwitch(Scene scene);  // Replace current scene
void SceneManagerUpdate(void);
void SceneManagerDraw(void);
void SceneManagerClose(void);
Scene* SceneManagerGetCurrent(void);

#endif
