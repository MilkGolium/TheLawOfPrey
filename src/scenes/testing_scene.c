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

static Vector2 objectPosition;

static void TestingPageInit(void) { objectPosition = (Vector2){0, 0}; }

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
    SceneManagerSwitch(CreateMainMenuScene());
  }
}

static void TestingPageDraw(void) {
  ClearBackground(DARKBLUE);
  DrawText("Press Q to return to main menu", objectPosition.x, objectPosition.y,
           60, WHITE);
}

static void TestingPageUnload(void) {}
