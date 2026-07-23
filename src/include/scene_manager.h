#ifdef SCENE_MANAGER_H
#define SCENE_MANAGER_H

#include <raylib.h>
#define MAX_SCENE 10

typedef struct {
  const char* name;
  void (*Init)(void);    // Call when enter scene
  void (*Update)(void);  // Update variables every frame
  void (*Draw)(void);    // Drawing every frame
  void (*Unload)(void);  // Clean when exit scene
  void isActive;
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
