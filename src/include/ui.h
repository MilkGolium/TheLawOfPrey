#ifndef UI_H
#define UI_H

#include <raylib.h>

// 自有界面层的度量与原语。不依赖任何第三方 GUI 库。
//
// 约定：
//   - 全部尺寸为整像素。坐标与尺寸都应落在 12 的倍数上，理由见「待定」
//     中像素网格的讨论。
//   - 不做自动折行，也不做宽度上限截断。文本有多长，控件就有多宽；
//     锚点放在屏幕外时控件同样会跑到屏幕外，这是预期行为。
//   - 换行只认显式的 '\n'，由调用方决定。文本度量与绘制都不猜测宽度。

// 界面层全局配色。主题文件（assets/themes/*.cfg）只能改这些字段；
// 字体度量由字体模块独占，不在主题可配置范围内，见 theme.c 的
// kForbiddenKeys。
typedef struct UiStyle {
  Color background;  // 控件背景色
  Color line;        // 边框、分隔线
  Color text;        // 正文文字
  Color textDim;     // 次要文字（禁用、弱化）
  Color button;      // 按钮底色
  Color buttonText;  // 按钮文字色
  Color titleBar;    // box 标题栏底色
  Color titleText;   // box 标题栏文字色
} UiStyle;

// 内置默认配色，也是 gUiStyle 的初始值。
// 主题文件缺某个键时用这里的值兜底（见 theme.c 的 ApplyThemeFile）。
// 修改时必须同步更新 assets/themes/default.cfg 中的对应值。
extern const UiStyle kDefaultUiStyle;

// 当前生效的全局配色。InitTheme() 之前即为默认值；
// LoadTheme() 成功后整体替换（失败不动）。
extern UiStyle gUiStyle;

// 文本像素尺寸。字段命名为 width/height 而非 x/y，避免与 Rectangle
// 的位置语义混淆：Rectangle.x 是位置，这里是尺寸。
typedef struct UiSize {
  int width;
  int height;
} UiSize;

// 行距。raylib 的 MeasureTextEx 行间步进是「字号 + 2」（实测 12/24/36
// 对应 14/26/38），既不是字号也不对齐 12 的像素格，且无法通过
// MeasureTextEx 的 spacing 参数调整（spacing 只管行内字符的水平间隔）。
// 因此多行文本由本模块逐行绘制，y 步进取字号本身：等于 12 的整数倍，
// 每行都落在像素格上，且与 DOS 时代终端行高即字高的排版一致。
#define UI_LINE_STEP(size) (size)

// 文本像素尺寸，size 为 12、24 或 36。含显式 '\n' 的多行文本。
// 宽度取 MeasureTextEx 的结果：raylib 已按最长行取宽且与行序无关，
// 不需要自行切行。高度按 UI_LINE_STEP 累加，不采用 MeasureTextEx 的
// .y（那是按「字号 + 2」算的，见上）。
UiSize UiMeasureText(const char* text, int size);

// 逐行绘制文本，遇到 '\n' 换行，行距为 UI_LINE_STEP(size)。
// posX/posY 是首行左上角。tint 传给 DrawUIText。
// 单行文本（含无换行的普通字符串）等价于 DrawUIText。
void UiDrawText(int posX, int posY, const char* text, int size, Color tint);

// === 原语 ===
//
// 四类原语（矩形 / label / button / box）与模态消息框。全部度量引用
// 自己的宏，不写死数值；配色调一律从 gUiStyle 取，不接受颜色参数。

// 内部留白。所有原语的间距统一由此取，保证坐标与尺寸落在 12 的倍数上。
#define UI_PADDING 12

// 向上取整到 12 的倍数，使文本度量得出的非网格尺寸收敛到像素格。
#define UI_ALIGN12(v) (((v) + 11) / 12 * 12)

// 按钮高度：字号 24 时得 36（24 + 12），也是 12 的倍数。
#define UI_BUTTON_HEIGHT(size) ((size) + UI_PADDING)

// 绘制矩形：以 gUiStyle.background 填充，gUiStyle.line 画 1px 边框。
void UiDrawRect(int x, int y, int width, int height);

// 绘制 label：以 gUiStyle.text 颜色逐行绘制。
// 等价于 UiDrawText(..., gUiStyle.text)，只是换行仍由本模块处理。
void UiDrawLabel(int x, int y, const char* text, int size);

// 绘制按钮并响应输入，返回 true 表示本帧被激活。
// focused 为 true 时本按钮持有键盘焦点：焦点是按钮唯一的变色来源，
// 前景/背景互换（底色取 buttonText、文字取 button）；mouse 悬停与
// 按下不产生任何视觉变化。回车激活焦点按钮；鼠标按住并在按钮内
// 松开也激活（可选便利，不作唯一入口）。边框一律 line 色。
// 焦点归属由调用方管理，本函数不保存任何状态。
bool UiButton(int x, int y, int width, int height, const char* label, int size,
              bool focused);

// 绘制带标题栏的容器：标题栏高为 size，底色 titleBar、文字 titleText；
// 内容区底色 background，外框 line 色。
void UiDrawBox(int x, int y, int width, int height, const char* title,
               int size);

// 模态消息框。尺寸由文本度量推算并在屏幕居中，不接收外部矩形
// （原 raygui 的固定 400*100 在 24px 字体下装不下标题栏 + 正文 +
// 按钮 + 间距，见 README「界面与字体」）。左右方向键在按钮间移动
// 焦点，回车激活，ESC 取消，鼠标点击按钮同样激活。
// 返回值：被激活的按钮下标（>= 0）；ESC 取消返回 -1；本帧无操作
// 返回 -2。focusIndex 由调用方持有，重新打开前置 0 即重置焦点。
int UiMessageBox(const char* title, const char* message, const char* buttons[],
                 int buttonCount, int size, int* focusIndex);

#endif  // UI_H
