#include "ui.h"

#include <string.h>

#include "font.h"
#include "graphsettings.h"

// === 原语 ===

// === 全局配色 ===

// 内置默认配色。主题文件（assets/themes/*.cfg）的缺键用这些值兜底；
// 修改时须同步更新 default.cfg 的对应键，两者互为权威副本。
// 色值迁移自原 raygui 默认主题：
//   background=background, button=base_normal, text=text_normal,
//   textDim=text_disabled, line=line, titleBar=border_focused,
//   titleText=白（反白标题栏）。
const UiStyle kDefaultUiStyle = {
    .background = {0xf5, 0xf5, 0xf5, 0xff},
    .line = {0x90, 0xab, 0xb5, 0xff},
    .text = {0x68, 0x68, 0x68, 0xff},
    .textDim = {0xae, 0xb7, 0xb8, 0xff},
    .button = {0xc9, 0xc9, 0xc9, 0xff},
    .buttonText = {0x1a, 0x1a, 0x1a, 0xff},
    .titleBar = {0x5b, 0xb2, 0xd9, 0xff},
    .titleText = {0xff, 0xff, 0xff, 0xff},
};

UiStyle gUiStyle = kDefaultUiStyle;

// === 行数 ===
// 每个 '\n' 若其后还有内容，就算新的一行；末尾的 '\n' 之后无内容，
// 不算行（否则高度会比绘制多出一格）。中间的连续换行各自产生一个
// 空行，空行不绘制但仍占一个行距。
static int CountLines(const char* text) {
  if (*text == '\0') return 1;

  int lines = 1;
  for (const char* p = text; *p != '\0'; p++) {
    if (*p == '\n' && *(p + 1) != '\0') lines++;
  }
  return lines;
}

UiSize UiMeasureText(const char* text, int size) {
  // 宽度直接交给 MeasureTextEx：raylib 6.0 实测已按最长行取宽，
  // 且与行序无关（"ab\nabcdef" 与 "abcdef\nab" 结果相同），
  // 因此不自行切行求宽。向上取整，避免小数截断导致右侧裁切。
  Vector2 measured = MeasureUIText(text, size);
  int width = (int)measured.x;
  if (measured.x > (float)width) width++;

  int lines = CountLines(text);
  // 首行占一个字号高，之后每行前进一个行距。
  int height = size + (lines - 1) * UI_LINE_STEP(size);
  return (UiSize){width, height};
}

void UiDrawText(int posX, int posY, const char* text, int size, Color tint) {
  int step = UI_LINE_STEP(size);
  int y = posY;

  // 逐行绘制。整串交给 DrawUIText 会用 raylib 的「字号 + 2」步进，
  // 那不是 12 的整数倍，多行文字会错开像素格。
  while (1) {
    const char* lineEnd = strchr(text, '\n');
    int lineLength =
        (lineEnd != NULL) ? (int)(lineEnd - text) : (int)strlen(text);

    // 复制到临时缓冲以复用单行绘制接口。行长远小于栈空间足够，
    // 不引入动态分配。
    char line[256];
    if (lineLength > 0) {
      int copyLength = (lineLength < (int)sizeof(line) - 1)
                           ? lineLength
                           : (int)sizeof(line) - 1;
      memcpy(line, text, (size_t)copyLength);
      line[copyLength] = '\0';
      DrawUIText(posX, y, line, size, tint);
    }

    if (lineEnd == NULL) return;
    text = lineEnd + 1;
    // 换行后已无内容：这是末尾换行，不存在下一行，不该再前进，
    // 否则 y 会比 CountLines 算出的高度多走一格。
    if (*text == '\0') return;
    y += step;
  }
}

// === 原语 ===

void UiDrawRect(int x, int y, int width, int height) {
  DrawRectangle(x, y, width, height, gUiStyle.background);
  DrawRectangleLines(x, y, width, height, gUiStyle.line);
}

void UiDrawLabel(int x, int y, const char* text, int size) {
  UiDrawText(x, y, text, size, gUiStyle.text);
}

// raylib 把字形画在 posY + glyph.offsetY，墨迹的上下边界与字号并不相等。
// 垂直居中必须按真实墨迹算：加载字号变大后 offsetY 同比增长，若仍按
// 「高度 - 字号」摆放，文字会整体下坠。返回 false 表示串内没有可绘制字形。
static bool TextInkBounds(const char* text, int size, int* outTop,
                          int* outHeight) {
  Font f = GetUIFont(size);
  int top = 0;
  int bottom = 0;
  bool found = false;
  for (const char* p = text; *p != '\0';) {
    int charBytes = 0;
    int codepoint = GetCodepointNext(p, &charBytes);
    p += charBytes;
    int index = GetGlyphIndex(f, codepoint);
    if (index < 0 || f.recs[index].height <= 0.0f) continue;
    int glyphTop = f.glyphs[index].offsetY;
    int glyphBottom = glyphTop + (int)f.recs[index].height;
    if (!found || glyphTop < top) top = glyphTop;
    if (!found || glyphBottom > bottom) bottom = glyphBottom;
    found = true;
  }
  if (found) {
    *outTop = top;
    *outHeight = bottom - top;
  }
  return found;
}

bool UiButton(int x, int y, int width, int height, const char* label, int size,
              bool focused) {
  // 命中测试用画布坐标：GetMousePosition 给的是窗口坐标，画布放大后不通用。
  Vector2 mouse = GetCanvasMouse();
  bool hovered = (mouse.x >= (float)x && mouse.x < (float)(x + width) &&
                  mouse.y >= (float)y && mouse.y < (float)(y + height));

  // 焦点是按钮唯一的变色来源：持有焦点时前景/背景互换（底色取
  // buttonText、文字取 button），鼠标悬停与按下不产生任何视觉变化。
  Color bg = focused ? gUiStyle.buttonText : gUiStyle.button;
  Color fg = focused ? gUiStyle.button : gUiStyle.buttonText;
  DrawRectangle(x, y, width, height, bg);
  DrawRectangleLines(x, y, width, height, gUiStyle.line);

  // 水平居中按度量宽；垂直居中按墨迹边界，使墨迹中线落在按钮高度中点。
  UiSize textSize = UiMeasureText(label, size);
  int textX = x + (width - textSize.width) / 2;
  int inkTop = 0;
  int inkHeight = 0;
  int textY = y + (height - size) / 2;
  if (TextInkBounds(label, size, &inkTop, &inkHeight))
    textY = y + (height - inkHeight) / 2 - inkTop;
  UiDrawText(textX, textY, label, size, fg);

  // 激活：焦点按钮回车按下即激活；鼠标松开时若光标仍在按钮内才激活，
  // 按下后移开再松开则不触发（按钮无跨帧状态，不区分按下起点）。
  bool activated = false;
  if (focused && IsKeyPressed(KEY_ENTER)) activated = true;
  if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && hovered) activated = true;
  return activated;
}

void UiDrawBox(int x, int y, int width, int height, const char* title,
               int size) {
  int titleBarHeight = UI_TITLEBAR_HEIGHT(size);
  DrawRectangle(x, y, width, titleBarHeight, gUiStyle.titleBar);
  DrawRectangle(x, y + titleBarHeight, width, height - titleBarHeight,
                gUiStyle.background);
  DrawRectangleLines(x, y, width, height, gUiStyle.line);
  // 墨迹下缘会伸到 offsetY + 高度，按栏高居中，使标题上下各留出内边距。
  int inkTop = 0;
  int inkHeight = 0;
  int titleY = y;
  if (TextInkBounds(title, size, &inkTop, &inkHeight))
    titleY = y + (titleBarHeight - inkHeight) / 2 - inkTop;
  UiDrawText(x + UI_PADDING, titleY, title, size, gUiStyle.titleText);
}

int UiMessageBox(const char* title, const char* message, const char* buttons[],
                 int buttonCount, int size, int* focusIndex) {
  if (buttonCount <= 0) return -1;
  if (*focusIndex < 0 || *focusIndex >= buttonCount) *focusIndex = 0;

  // 左右键在按钮间移动焦点（循环），回车留在按钮处激活。
  if (IsKeyPressed(KEY_LEFT)) {
    *focusIndex = (*focusIndex + buttonCount - 1) % buttonCount;
  } else if (IsKeyPressed(KEY_RIGHT)) {
    *focusIndex = (*focusIndex + 1) % buttonCount;
  }
  if (IsKeyPressed(KEY_ESCAPE)) return -1;

  // 尺寸由文本度量推算：最宽者（标题 / 正文 / 按钮行）加左右留白。
  UiSize titleSize = UiMeasureText(title, size);
  UiSize msgSize = UiMeasureText(message, size);

  int buttonHeight = UI_BUTTON_HEIGHT(size);
  int contentWidth = 0;
  for (int i = 0; i < buttonCount; i++) {
    int w = UI_ALIGN12(UiMeasureText(buttons[i], size).width + 2 * UI_PADDING);
    contentWidth += w;
  }
  contentWidth += (buttonCount - 1) * UI_PADDING;

  int maxWidth = titleSize.width;
  if (msgSize.width > maxWidth) maxWidth = msgSize.width;
  if (contentWidth > maxWidth) maxWidth = contentWidth;

  // 标题栏到正文（msgSize.height）到按钮行，每层间隔一个留白。
  int titleBarHeight = UI_TITLEBAR_HEIGHT(size);
  int height = titleBarHeight + UI_PADDING + msgSize.height + UI_PADDING +
               buttonHeight + UI_PADDING;

  int boxWidth = UI_ALIGN12(maxWidth + 2 * UI_PADDING);
  int boxHeight = UI_ALIGN12(height);
  // 在设计分辨率内居中。不能用 GetScreenWidth，那是窗口尺寸而非画布尺寸。
  int x = (GRAPH_DESIGN_WIDTH - boxWidth) / 2;
  int y = (GRAPH_DESIGN_HEIGHT - boxHeight) / 2;

  UiDrawBox(x, y, boxWidth, boxHeight, title, size);

  // 正文紧贴标题栏下方，逐行绘制（行距为字号，12 的倍数）。
  UiDrawText(x + UI_PADDING, y + titleBarHeight + UI_PADDING, message, size,
             gUiStyle.text);

  // 按钮行从框底向上摆放，等宽间距。
  int buttonY = y + boxHeight - UI_PADDING - buttonHeight;
  int buttonX = x + (boxWidth - contentWidth) / 2;
  for (int i = 0; i < buttonCount; i++) {
    int w = UI_ALIGN12(UiMeasureText(buttons[i], size).width + 2 * UI_PADDING);
    if (UiButton(buttonX, buttonY, w, buttonHeight, buttons[i], size,
                 *focusIndex == i))
      return i;
    buttonX += w + UI_PADDING;
  }

  return -2;
}
