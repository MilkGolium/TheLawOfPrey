#include "raylib.h"
#include "scene_manager.h"

extern Scene CreateMainMenuScene(void);

static void SettingsInit(void);
static void SettingsUpdate(void);
static void SettingsDraw(void);
static void SettingsUnload(void);

Scene CreateSettingsScene(void) {
  return (Scene){.name = "Settings",
                 .Init = SettingsInit,
                 .Update = SettingsUpdate,
                 .Draw = SettingsDraw,
                 .Unload = SettingsUnload};
}

static void SettingsInit(void) {}

static void SettingsUpdate(void) {
  if (IsKeyPressed(KEY_Q)) {
    SceneManagerSwitch(CreateMainMenuScene());
  }
}

static void SettingsDraw(void) {
  ClearBackground(DARKGRAY);
  DrawText("Settings", 350, 280, 40, WHITE);
}

static void SettingsUnload(void) {}
