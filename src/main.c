#include "raylib.h"

// Function to enable fullscreen mode
void EnableFullscreen(void) {
  if (!IsWindowFullscreen()) {
    ToggleFullscreen();
  }
}

// Function to disable fullscreen mode
void DisableFullscreen(void) {
  if (IsWindowFullscreen()) {
    ToggleFullscreen();
  }
}

// Function to set fullscreen mode based on a boolean value
void SetFullscreen(bool enable) {
  if (IsWindowFullscreen() != enable) {
    ToggleFullscreen();
  }
}

int main(void) {
  // Initialization
  // Set the screen width and height
  const int screenWidth = 1024;
  const int screenHeight = 768;
  // Set the initial fullscreen state
  bool fullscreenEnabled = true;

  InitWindow(screenWidth, screenHeight, "The Law Of Prey");
  SetTargetFPS(60);

  while (!WindowShouldClose()) {
    // Update
    // Toggle fullscreen mode when F11 is pressed
    if (IsKeyPressed(KEY_F11)) fullscreenEnabled = !fullscreenEnabled;
    // Set the fullscreen state based on the boolean value
    if (fullscreenEnabled)
      EnableFullscreen();
    else
      DisableFullscreen();

    // Draw
    BeginDrawing();
    ClearBackground(BLACK);
    DrawText("Hello, world!", 0, 0, 60, RED);
    EndDrawing();
  }

  // De-Initialization
  CloseWindow();
  return 0;
}
