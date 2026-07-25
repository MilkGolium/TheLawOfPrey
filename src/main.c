#include "graphsettings.h"
#include "raylib.h"
#include "scene_manager.h"
#include "stddef.h"  // NULL 值需要这个头文件

// Declare scene creation function
extern Scene CreateMainMenuScene(void);

int main(void) {
  // Initialization
  // Set the screen width and height
  const int screenWidth = 1024;
  const int screenHeight = 768;

  // Set the initial fullscreen state
  bool fullscreenEnabled = true;

  InitWindow(screenWidth, screenHeight, "The Law Of Prey");

  // Temporary

  // Enable fullscreen mode at the start
  SetTargetFPS(120);

  SetFullscreen(fullscreenEnabled);

  // Initialize the scene manager
  SceneManagerInit();

  // Enter main menu on startup
  SceneManagerPush(CreateMainMenuScene());

  while (!WindowShouldClose()) {
    // Update
    // Update the current scene
    SceneManagerUpdate();

    // If the scene stack is empty, all scenes have been popped, exit the
    // program.
    if (SceneManagerGetCurrent() == NULL) break;

    // Draw
    BeginDrawing();
    // Draw current scene
    SceneManagerDraw();
    EndDrawing();
  }

  // De-Initialization
  CloseWindow();
  return 0;
}
