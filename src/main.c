#include <stddef.h>

#include "graphsettings.h"
#include "raylib.h"
#include "scene_manager.h"
#include "utils.h"

extern Scene CreateMainMenuScene(void);

int main(void) {
  const int screenWidth = 1920;
  const int screenHeight = 1080;
  const bool fullscreenEnabled = true;
  int targetFPS = 144;

  InitWindow(screenWidth, screenHeight, "The Law Of Prey");
  SetExitKey(KEY_NULL);
  SetTargetFPS(targetFPS);
  SetFullscreen(fullscreenEnabled);

  SceneManagerInit();
  SceneManagerSwitch(CreateMainMenuScene());

  InitAssetsDirectory();

  while (!WindowShouldClose()) {
    SceneManagerUpdate();

    // Exit when all scenes have been popped
    if (SceneManagerGetCurrent() == NULL) break;

    BeginDrawing();
    SceneManagerDraw();
    EndDrawing();
  }

  CloseWindow();
  return 0;
}
