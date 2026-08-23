// 此界面用于游玩，涉及程序生成地图技术

#include <stdio.h>

#include "raylib.h"
#include "scene_manager.h"

extern Scene CreateMainMenuScene(void);
extern Scene HelpingScene(void);

static void GamePageInit(void);
static void GamePageUpdate(void);
static void GamePageDraw(void);
static void GamePageUnload(void);

Scene CreateGameScene(void) {
  return (Scene){.name = "Game Scene",
                 .Init = GamePageInit,
                 .Update = GamePageUpdate,
                 .Draw = GamePageDraw,
                 .Unload = GamePageUnload};
}

static void GamePageInit(void) {}

static void GamePageUpdate(void) {
  if (IsKeyPressed(KEY_ESCAPE)) {
    SceneManagerSwitch(CreateMainMenuScene());
  }
  if (IsKeyPressed(KEY_F1)) {
    SceneManagerSwitch(HelpingScene());
  }
}

static void GamePageDraw(void) {
  ClearBackground(BLACK);
  DrawText("Enjoy!", 0, 0, 20, RAYWHITE);
}

static void GamePageUnload(void) {}
