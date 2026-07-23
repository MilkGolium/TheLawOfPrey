#include "graphsettings.h"
#include "raylib.h"

// Player temple
typedef struct {
  int strength;
  int endurance;
  int constitution;
  int agility;
  int perception;

  int hunger;  // 0 is full, and 100 is starving.
  int thirst;  // 0 is hydrated, and 100 is dehydrated.

  int max_carry_weight;
  int reputation;
} Player;

// Entity temple
typedef struct {
  Vector2 position;
  bool has_render;
  Texture2D texture;
  // more properties...
} Entity;

typedef struct {
  const char* name;
  void (*Init)(void);
  void (*Update)(void);
  void (*Draw)(void);
  bool isActive;
} Scene;

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
  SetTargetFPS(60);
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
