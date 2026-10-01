#include "graphsettings.h"

#include "raylib.h"

// 设计画布的渲染纹理。InitCanvas 之前 texture.id 为 0。
static RenderTexture2D canvas;

// 整数放大倍数：两个方向都能放得下才取该倍数，放不满的部分留作黑边。
// 窗口比设计分辨率还小时退化为 1，宁可裁切也不做非整数缩小。
static int GetCanvasScale(void) {
  int scaleX = GetScreenWidth() / GRAPH_DESIGN_WIDTH;
  int scaleY = GetScreenHeight() / GRAPH_DESIGN_HEIGHT;
  int scale = (scaleX < scaleY) ? scaleX : scaleY;
  return (scale < 1) ? 1 : scale;
}

// 放大后的画布在窗口内的左上角偏移，使画布居中。整数点偏移，保证像素对齐。
static Vector2 GetCanvasOrigin(int scale) {
  int x = (GetScreenWidth() - GRAPH_DESIGN_WIDTH * scale) / 2;
  int y = (GetScreenHeight() - GRAPH_DESIGN_HEIGHT * scale) / 2;
  return (Vector2){(float)x, (float)y};
}

void EnableFullscreen(void) {
  if (!IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) {
    ToggleBorderlessWindowed();
  }
}

void DisableFullscreen(void) {
  if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) {
    ToggleBorderlessWindowed();
  }
}

void SetFullscreen(bool enable) {
  if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE) != enable) {
    ToggleBorderlessWindowed();
  }
}

void InitCanvas(void) {
  canvas = LoadRenderTexture(GRAPH_DESIGN_WIDTH, GRAPH_DESIGN_HEIGHT);
  // 画布只做整数倍放大，点采样才能保持像素硬边。
  SetTextureFilter(canvas.texture, TEXTURE_FILTER_POINT);
}

void UnloadCanvas(void) {
  if (canvas.texture.id != 0) {
    UnloadRenderTexture(canvas);
    canvas.texture.id = 0;
  }
}

void BeginCanvas(void) { BeginTextureMode(canvas); }

void EndCanvas(void) { EndTextureMode(); }

void DrawCanvas(void) {
  int scale = GetCanvasScale();
  Vector2 origin = GetCanvasOrigin(scale);

  // 渲染纹理在 GL 里上下翻转，源矩形高度取负以摆正。
  Rectangle source = {0.0f, 0.0f, (float)GRAPH_DESIGN_WIDTH,
                      -(float)GRAPH_DESIGN_HEIGHT};
  Rectangle dest = {origin.x, origin.y, (float)(GRAPH_DESIGN_WIDTH * scale),
                    (float)(GRAPH_DESIGN_HEIGHT * scale)};
  DrawTexturePro(canvas.texture, source, dest, (Vector2){0.0f, 0.0f}, 0.0f,
                 WHITE);
}

Vector2 GetCanvasMouse(void) {
  Vector2 mouse = GetMousePosition();
  int scale = GetCanvasScale();
  Vector2 origin = GetCanvasOrigin(scale);
  return (Vector2){(mouse.x - origin.x) / (float)scale,
                   (mouse.y - origin.y) / (float)scale};
}
