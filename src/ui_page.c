#include "ui_page.h"

#include <stdbool.h>
#include <stddef.h>

#include "graphsettings.h"
#include "ui.h"
#include "ui_slot.h"

// === 页面容器与页签栏 ===

// 页签数量越界的告警只报一次：绘制路径每帧都会经过这里，重复告警会把
// 日志刷爆。钳制仍然逐次执行，告警只是提示调用方传错了值。
static bool sTabClampWarned = false;

Rectangle UiPageContentRect(void) {
  return (Rectangle){(float)UI_PAGE_TAB_WIDTH, 0.0f,
                     (float)(GRAPH_DESIGN_WIDTH - UI_PAGE_TAB_WIDTH),
                     (float)GRAPH_DESIGN_HEIGHT};
}

void UiDrawPageTabs(int selectedTab, int tabCount, const char* const* labels) {
  if (tabCount <= 0) return;

  if (tabCount > UI_PAGE_TAB_COUNT_MAX) {
    if (!sTabClampWarned) {
      sTabClampWarned = true;
      TraceLog(LOG_WARNING, "UiPage: tab count %d exceeds %d, value clamped",
               tabCount, UI_PAGE_TAB_COUNT_MAX);
    }
    tabCount = UI_PAGE_TAB_COUNT_MAX;
  }

  int selected = selectedTab;
  if (selected < 0) selected = 0;
  if (selected >= tabCount) selected = tabCount - 1;

  for (int i = 0; i < tabCount; i++) {
    Color background = (i == selected) ? gUiStyle.button : gUiStyle.background;
    Color foreground = (i == selected) ? gUiStyle.buttonText : gUiStyle.text;
    int y = i * UI_PAGE_TAB_HEIGHT;

    DrawRectangle(0, y, UI_PAGE_TAB_WIDTH, UI_PAGE_TAB_HEIGHT, background);

    if (labels == NULL || labels[i] == NULL) continue;

    // 页签太窄，文案最多留 3 个码点；4 字节的 UTF-8 码点最多 12 字节，
    // 再加结尾的 '\0'，16 字节的栈缓冲足够。
    char truncated[16];
    UiTruncateUtf8(labels[i], 3, truncated, (int)sizeof(truncated));
    int textWidth = UiMeasureText(truncated, 12).width;
    int textX = (UI_PAGE_TAB_WIDTH - textWidth) / 2;
    UiDrawText(textX, y + 2, truncated, 12, foreground);
  }
}

int UiPageTabAt(int px, int py, int tabCount) {
  if (tabCount <= 0) return -1;
  if (px < 0 || py < 0) return -1;

  if (tabCount > UI_PAGE_TAB_COUNT_MAX) tabCount = UI_PAGE_TAB_COUNT_MAX;

  if (px >= UI_PAGE_TAB_WIDTH) return -1;
  if (py >= tabCount * UI_PAGE_TAB_HEIGHT) return -1;

  return py / UI_PAGE_TAB_HEIGHT;
}

// === 滚动条 ===

int UiScrollbarThumbHeight(int trackHeight, int contentCount,
                           int visibleCount) {
  if (trackHeight <= 0 || contentCount <= 0 || visibleCount <= 0) return 0;
  if (contentCount <= visibleCount) return 0;

  // 先乘后除，用 64 位中间值避免大内容量时的整数溢出。
  int height = (int)((long long)trackHeight * visibleCount / contentCount);
  if (height < UI_SCROLLBAR_MIN_THUMB) height = UI_SCROLLBAR_MIN_THUMB;
  if (height > trackHeight) height = trackHeight;
  return height;
}

int UiScrollbarThumbY(int trackHeight, int thumbHeight, int contentCount,
                      int visibleCount, int firstVisible) {
  if (trackHeight <= 0 || contentCount <= 0 || visibleCount <= 0) return 0;
  if (contentCount <= visibleCount) return 0;

  int maxFirst = contentCount - visibleCount;
  if (firstVisible < 0) firstVisible = 0;
  if (firstVisible > maxFirst) firstVisible = maxFirst;

  if (thumbHeight < 0) thumbHeight = 0;
  if (thumbHeight > trackHeight) thumbHeight = trackHeight;

  int range = trackHeight - thumbHeight;
  if (range <= 0) return 0;

  int denominator = maxFirst > 1 ? maxFirst : 1;
  return (int)((long long)range * firstVisible / denominator);
}

void UiDrawScrollbar(int x, int y, int trackHeight, int contentCount,
                     int visibleCount, int firstVisible) {
  if (trackHeight <= 0 || contentCount <= 0 || visibleCount <= 0) return;

  DrawRectangle(x, y, UI_SCROLLBAR_WIDTH, trackHeight, gUiStyle.background);

  if (contentCount <= visibleCount) return;

  int thumbHeight =
      UiScrollbarThumbHeight(trackHeight, contentCount, visibleCount);
  int thumbY = UiScrollbarThumbY(trackHeight, thumbHeight, contentCount,
                                 visibleCount, firstVisible);
  if (thumbHeight <= 0) return;

  // 两端渐缩：首尾两行 1px、次首尾两行 3px、其余 5px。5px 的奇数列里
  // 只有奇数宽度能居中，所以不写成 5,4,3,2,1（那会得到蝴蝶结）。
  for (int row = 0; row < thumbHeight; row++) {
    int width = UI_SCROLLBAR_WIDTH;
    if (row == 0 || row == thumbHeight - 1) {
      width = 1;
    } else if (row == 1 || row == thumbHeight - 2) {
      width = 3;
    }
    int offset = (UI_SCROLLBAR_WIDTH - width) / 2;
    DrawRectangle(x + offset, y + thumbY + row, width, 1, gUiStyle.titleText);
  }
}

// === 状态条 ===

int UiBarFillWidth(int width, int value, int max) {
  if (width < 3) return 0;

  int inner = width - 2;
  if (max <= 0 || value <= 0) return 0;
  if (value >= max) return inner;

  int fill = (int)(((long long)value * inner + max / 2) / max);
  if (fill < 0) fill = 0;
  if (fill > inner) fill = inner;
  return fill;
}

void UiDrawBar(int x, int y, int width, int value, int max) {
  if (width < 3) return;

  DrawRectangle(x, y, width, UI_BAR_HEIGHT, gUiStyle.lineDim);
  DrawRectangle(x + 1, y + 1, width - 2, UI_BAR_HEIGHT - 2, gUiStyle.slotBg);

  int fill = UiBarFillWidth(width, value, max);
  if (fill > 0)
    DrawRectangle(x + 1, y + 1, fill, UI_BAR_HEIGHT - 2, gUiStyle.line);
}
