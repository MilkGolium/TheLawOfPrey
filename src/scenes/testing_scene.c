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

int FontSize;
static Vector2 objectPosition;

static void TestingPageInit(void) {
  objectPosition = (Vector2){0, 0};
  FontSize = 60;
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
    FontSize--;
  }
  if (IsKeyDown(KEY_E)) {
    FontSize++;
  }
  if (IsKeyPressed(KEY_ESCAPE)) {
    SceneManagerSwitch(CreateMainMenuScene());
  }
}

static void TestingPageDraw(void) {
  ClearBackground(DARKBLUE);
  DrawText("Press Ctrl+Q to return to main menu", objectPosition.x,
           objectPosition.y, FontSize, WHITE);

  char debugText[64];
  snprintf(debugText, sizeof(debugText), "x:%d y:%d FontSize:%d",
           (int)objectPosition.x, (int)objectPosition.y, FontSize);
  int debugWidth = MeasureText(debugText, 20);
  DrawText(debugText, GetScreenWidth() - debugWidth - 10, 10, 20, GREEN);
}

static void TestingPageUnload(void) {}
