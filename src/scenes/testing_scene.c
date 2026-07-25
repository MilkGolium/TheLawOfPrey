#include "raylib.h"
#include "scene_manager.h"

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

Vector2 objectPosition;

static void TestingPageInit(void) {
  // 初始化资源
  objectPosition = (Vector2){0, 0};
}

static void TestingPageUpdate(void) {
  // 按S进入设置
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
}

static void TestingPageDraw(void) {
  ClearBackground(DARKBLUE);
  DrawText("This is an empty scene", objectPosition.x, objectPosition.y, 60,
           WHITE);
}

static void TestingPageUnload(void) {
  // 清理菜单资源
}
