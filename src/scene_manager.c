#include "scene_manager.h"

#include <stdio.h>
#include <string.h>

static Scene sceneStack[MAX_SCENES];
static int stackCount = 0;

void SceneManagerInit(void) { stackCount = 0; }
void SceneManagerPush(Scene scene) {
  if (stackCount >= MAX_SCENES) {
    printf("Error: Scene stack is full!");
    return;
  }

  // Pause the scene fitst if there have a current scene (not unload)
  // Initialized new scene
  sceneStack[stackCount] = scene;
  sceneStack[stackCount].isActive = true;
  if (scene.Init) scene.Init();
  stackCount++;
}

void SceneManagerPop(void) {
  if (stackCount <= 0) return;
  // Unload the current scene
  int top = stackCount - 1;
  if (sceneStack[top].Unload) sceneStack[top].Unload();
  sceneStack[top].isActive = false;
  stackCount--;

  // Optional: Restore the last scene (This assumes the scene is always running,
  // but is being overwritten)
}

void SceneManagerSwitch(Scene scene) {
  // Pop out the current scene and push the new scene onto the stack.
  SceneManagerPop();
  SceneManagerPush(scene);
}

void SceneManagerUpdate(void) {
  if (stackCount > 0) {
    Scene* current = &sceneStack[stackCount - 1];
    if (current->isActive && current->Update) {
      current->Update();
    }
  }
}

void SceneManagerDraw(void) {
  // Render from the buttom of the stack upwards. (Optional: For transparent
  // overlay)
  for (int i = 0; i < stackCount; i++) {
    if (sceneStack[i].isActive && sceneStack[i].Draw) sceneStack[i].Draw();
  }
}

void SceneManagerClose(void) {
  while (stackCount > 0) SceneManagerPop();
}

Scene* SceneManagerGetCurrent(void) {
  if (stackCount > 0) return &sceneStack[stackCount - 1];
  return NULL;
}
