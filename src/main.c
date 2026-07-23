#include "graphsettings.h"
#include "raylib.h"

int main(void) {
  // Initialization
  // Set the screen width and height
  const int screenWidth = 1024;
  const int screenHeight = 768;
  // Set the initial fullscreen state
  bool fullscreenEnabled = true;

  InitWindow(screenWidth, screenHeight, "The Law Of Prey");
  SetTargetFPS(60);

  // Temporary
  // Enable fullscreen mode at the start
  SetFullscreen(fullscreenEnabled);

  while (!WindowShouldClose()) {
    // Update

    // Draw
    BeginDrawing();
    ClearBackground(BLACK);
    DrawText("Hello World", 0, 0, 60, RED);
    EndDrawing();
  }

  // De-Initialization
  CloseWindow();
  return 0;
}
