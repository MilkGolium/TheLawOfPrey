#ifndef GRAPHSETTINGS_H
#define GRAPHSETTINGS_H

#include <raylib.h>
#include <stdbool.h>

// 设计分辨率。游戏逻辑与界面一律按这个尺寸绘制，再由 DrawCanvas 整数倍
// 放大到窗口。640*480 是 VGA 标准，纵向 480 = 40 * 12，与 12 像素网格对齐。
#define GRAPH_DESIGN_WIDTH 640
#define GRAPH_DESIGN_HEIGHT 480

void EnableFullscreen(void);
void DisableFullscreen(void);
void SetFullscreen(bool enable);

// 设计画布：一张 GRAPH_DESIGN_WIDTH x GRAPH_DESIGN_HEIGHT 的渲染纹理。
// 场景在画布内绘制，DrawCanvas 再把它按整数倍居中放大到窗口，余下为黑边。
// 整数倍是硬要求：非整数倍会让像素字在液晶上被插值成模糊边缘。
void InitCanvas(void);
void UnloadCanvas(void);
void BeginCanvas(void);
void EndCanvas(void);
void DrawCanvas(void);

// 画布坐标下的鼠标位置：窗口坐标减去黑边偏移后除以放大倍数。界面层的
// 命中测试必须用它，GetMousePosition 给的是窗口坐标，画布放大后不通用。
Vector2 GetCanvasMouse(void);

#endif
