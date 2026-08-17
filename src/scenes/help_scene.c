// 按 F1 进入此场景，显示游玩界面相关的快捷键等帮助信息

#include <stdio.h>

#include "raylib.h"
#include "scene_manager.h"

static void HelpingPageInit(void);
static void HelpingPageUpdate(void);
static void HelpingPageDraw(void);
static void HelpingPageUnload(void);

Scene CreateHelpingScene(void) {
  return (Scene){.name = "Helping Scene",
                 .Init = HelpingPageInit,
                 .Update = HelpingPageUpdate,
                 .Draw = HelpingPageDraw,
                 .Unload = HelpingPageUnload};
}

static void HelpingPageInit(void) {}

static void HelpingPageUpdate(void) {}

static void HelpingPageDraw(void) {
  ClearBackground(YELLOW);
  DrawText("Cheat sheet", 0, 0, 40, BROWN);
}

static void HelpingPageUnload(void) {}
