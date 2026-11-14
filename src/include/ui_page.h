#ifndef UI_PAGE_H
#define UI_PAGE_H

#include <raylib.h>

// 游玩界面的遮挡页面容器：左侧九项页签栏、右侧滚动条，以及 HUD 用的
// 状态条。这些原语与槽位一样没有跨帧状态、不持有输入；页签的点击命中由
// 场景在 Update 里调 UiPageTabAt() 完成，原语本身只负责绘制。
//
// 几何取自 README「界面几何与配色（游玩场景）」：页面铺满 640×480，
// 页签栏占左侧 40px，内容区从 x = 40 开始。所有坐标与尺寸为整像素，
// 配色一律从 gUiStyle 取。

// 页签栏：每项 40×16，最多九项，从画布左上角垂直排列。
#define UI_PAGE_TAB_WIDTH 40
#define UI_PAGE_TAB_HEIGHT 16
#define UI_PAGE_TAB_COUNT_MAX 9

// 滚动条：轨道 5px 宽，滑块最小 6 行（1,3,5,5,3,1 的渐缩序列）。
#define UI_SCROLLBAR_WIDTH 5
#define UI_SCROLLBAR_MIN_THUMB 6

// 状态条高度：1px 外框 + 10px 内区 + 1px 外框。
#define UI_BAR_HEIGHT 12

// 内容区矩形：{40, 0, 600, 480}。页签栏永远占左侧 40px。
Rectangle UiPageContentRect(void);

// 绘制页签栏。labels 是 tabCount 条文案，取前 tabCount 项；文案 12 号、
// 最多 3 个码点（超出用 UiTruncateUtf8 截断），水平居中、纵向 y + 2。
// selectedTab 钳到 0..tabCount-1；tabCount <= 0 时什么都不画；
// tabCount > 9 时按 9 项画并只告警一次。labels 为 NULL 时只画底色。
void UiDrawPageTabs(int selectedTab, int tabCount, const char* const* labels);

// 页签命中测试。px 必须落在 [0, 40)、py 落在 [0, tabCount*16) 才算命中，
// 返回页签下标；越界、负坐标或 tabCount <= 0 返回 -1。tabCount > 9 按 9 算。
int UiPageTabAt(int px, int py, int tabCount);

// 滑块高度。contentCount <= visibleCount 时返回 0（不画滑块）；否则
// max(6, trackHeight*visibleCount/contentCount)，上限为 trackHeight。
// 参数非法（trackHeight/contentCount/visibleCount <= 0）时返回 0。
int UiScrollbarThumbHeight(int trackHeight, int contentCount, int visibleCount);

// 滑块顶部相对轨道顶部的偏移。contentCount <= visibleCount 时返回 0；
// 否则 (trackHeight - thumbHeight) * firstVisible /
//      max(1, contentCount - visibleCount)，firstVisible 先钳到
//      0..contentCount-visibleCount。参数非法时返回 0。
int UiScrollbarThumbY(int trackHeight, int thumbHeight, int contentCount,
                      int visibleCount, int firstVisible);

// 绘制滚动条。轨道宽 5、底色 background；滑块底色 titleText，两端宽度
// 序列为 1,3,5 → 直段 5 → 5,3,1，宽度 w 的偏移为 (5-w)/2。
// 内容不足一屏或参数非法时只画轨道 / 什么都不画。
void UiDrawScrollbar(int x, int y, int trackHeight, int contentCount,
                     int visibleCount, int firstVisible);

// 状态条填充宽度（纯数学，便于测试）。inner = width - 2 ，
// fill = (value*inner + max/2) / max 钳到 0..inner。
// max <= 0 或 value <= 0 返回 0；value >= max 返回 inner；
// width < 3 时返回 0（装不下两侧边框）。
int UiBarFillWidth(int width, int value, int max);

// 绘制状态条。高 UI_BAR_HEIGHT，1px 外框 lineDim，内部轨道 slotBg，
// 填充 line。宽度 < 3 时不画。
void UiDrawBar(int x, int y, int width, int value, int max);

#endif  // UI_PAGE_H
