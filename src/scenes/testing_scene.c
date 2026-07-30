#include <stdio.h>

#include "raylib.h"
#include "scene_manager.h"

extern Scene CreateMainMenuScene(void);

static void TestingPageInit(void);
static void TestingPageUpdate(void);
static void TestingPageDraw(void);
static void TestingPageUnload(void);

Scene CreateTestingScene(void) {
  return (Scene){.name = "Testing Scene",
                 .Init = TestingPageInit,
                 .Update = TestingPageUpdate,
                 .Draw = TestingPageDraw,
                 .Unload = TestingPageUnload};
}

float FontSize;
static Vector2 objectPosition;

static void TestingPageInit(void) {
  objectPosition = (Vector2){241, 347};
  FontSize = 30;
}

static void TestingPageUpdate(void) {
  if (IsKeyDown(KEY_W)) {
    objectPosition.y--;
  }
  if (IsKeyDown(KEY_S)) {
    objectPosition.y++;
  }
  if (IsKeyDown(KEY_A)) {
    objectPosition.x--;
  }
  if (IsKeyDown(KEY_D)) {
    objectPosition.x++;
  }
  if (IsKeyDown(KEY_Q)) {
    FontSize -= 0.1;
    if (FontSize < 10) FontSize = 10;
  }
  if (IsKeyDown(KEY_E)) {
    FontSize += 0.1;
  }

  if (IsKeyPressed(KEY_ESCAPE)) {
    SceneManagerSwitch(CreateMainMenuScene());
  }
}

static void TestingPageDraw(void) {
  ClearBackground(DARKBLUE);
  DrawText("Press ESC to return to main menu", objectPosition.x,
           objectPosition.y, (int)FontSize, WHITE);

  char debugText[64];
  snprintf(debugText, sizeof(debugText), "x:%d y:%d FontSize:%d",
           (int)objectPosition.x, (int)objectPosition.y, (int)FontSize);
  int debugWidth = MeasureText(debugText, 20);
  DrawText(debugText, GetScreenWidth() - debugWidth - 10, 10, 20, GREEN);
}

static void TestingPageUnload(void) {}
