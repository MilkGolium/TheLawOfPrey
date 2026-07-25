#include "entity.h"
#include "raylib.h"
#include "scene_manager.h"

extern Scene CreateSettingsScene(void);
extern Scene CreateTestingScene(void);
// extern Scene CreateLoadGameScene(void);

static void MainMenuInit(void);
static void MainMenuUpdate(void);
static void MainMenuDraw(void);
static void MainMenuUnload(void);

Scene CreateMainMenuScene(void) {
  return (Scene){.name = "Main Menu",
                 .Init = MainMenuInit,
                 .Update = MainMenuUpdate,
                 .Draw = MainMenuDraw,
                 .Unload = MainMenuUnload};
}

static void MainMenuInit(void) {}

static void MainMenuUpdate(void) {
  if (IsKeyPressed(KEY_ENTER)) {
    SceneManagerSwitch(CreateTestingScene());
  }
  if (IsKeyPressed(KEY_S)) {
    SceneManagerSwitch(CreateSettingsScene());
  }
  if (IsKeyPressed(KEY_Q)) {
    SceneManagerClose();
  }
}

static void MainMenuDraw(void) {
  ClearBackground(DARKBLUE);
  DrawText("Main Menu", 340, 200, 40, WHITE);
  DrawText("Start Game (Enter)", 340, 260, 20, LIGHTGRAY);
  DrawText("Settings (S)", 340, 320, 20, LIGHTGRAY);
  DrawText("Quit (Q)", 340, 380, 20, LIGHTGRAY);
}

static void MainMenuUnload(void) {}
