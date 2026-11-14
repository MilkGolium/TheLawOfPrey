#ifndef UI_SLOT_H
#define UI_SLOT_H

#include <raylib.h>

// 游玩界面的纯绘制原语：点阵数字、物品槽位与槽位网格、UTF-8 文本截断。
// 这些原语没有输入、没有跨帧状态，配色一律从 gUiStyle 取，坐标与尺寸
// 全部为整像素。页面容器、页签栏、滚动条与状态条属于下一批，不在这里。

// 6×10 数量角标字形。一个数字是 60 位掩码，位序固定为
// bit = row * 6 + col，row 0 为顶行、col 0 为左列。
#define UI_DIGIT_W 6
#define UI_DIGIT_H 10
#define UI_DIGIT_GAP 1

// 3×5 备用字形，位序同样为 bit = row * 3 + col。
#define UI_DIGIT_SMALL_W 3
#define UI_DIGIT_SMALL_H 5

// 槽位几何：52×52 外框，2px 品阶边框，再内缩 1px 铺 48×48 图标。
// 相邻槽位之间共用 1px 分割线，因此横向节距是 52 + 1 = 53。
// 槽位几何：52 = 2（品阶框）+ 48（图标）+ 2（品阶框），没有额外内描边。
// 图标 48 正好填满内区；品阶框与画面之间的分隔由 slotBg（默认纯黑）与
// 图标自带的黑描边承担，再加一圈内描边会挤占图标（52 装不下 2+1+48+1+2）。
#define UI_SLOT_SIZE 52
#define UI_SLOT_BORDER 2
#define UI_SLOT_ICON (UI_SLOT_SIZE - 2 * UI_SLOT_BORDER)
#define UI_SLOT_PITCH (UI_SLOT_SIZE + 1)

// 一个槽位的内容。icon.id == 0 时只画框，不画图；rarity 越界时钳制到
// 0..5；stack 为 0 时不画角标，1..99 原样绘制，超过 99 钳到 99。
typedef struct {
  Texture2D icon;
  int rarity;
  int stack;
} UiSlotItem;

// 整数点。这里刻意不用 Vector2：那是浮点类型，界面几何只允许整像素。
typedef struct {
  int x;
  int y;
} UiPoint;

// 数字总宽：digits 个 6px 字形加 digits-1 个 1px 间隔。
// digits <= 0 返回 0。小号数字没有对应的宽度接口，间隔同样是 1px。
int UiNumberWidth(int digits);

// 按 6×10 掩码逐位绘制数字。value 钳到 0..99，digits 钳到 1..2；
// 按右对齐填满 digits 位，前导位不画（不补 0）。
void UiDrawNumber(int x, int y, int value, int digits, Color tint);

// 按 3×5 掩码逐位绘制数字，语义与 UiDrawNumber 相同。
void UiDrawNumberSmall(int x, int y, int value, int digits, Color tint);

// 品阶色。rarity 钳制到 0..5 后返回 gUiStyle 里对应的颜色。
Color UiRarityColor(int rarity);

// 绘制一个槽位。顺序固定：品阶外框、槽位底色、图标、右下角数量角标。
// 槽位本身不画分割线，分割线由 UiDrawSlotGrid 负责。
void UiDrawSlot(int x, int y, const UiSlotItem* item);

// 网格第 index 个格子左上角的坐标。节距 53，按行优先排列。
UiPoint UiGridCellPos(int originX, int originY, int columns, int index);

// 画布坐标对应的格子下标。落在分割线上、网格外，或 columns/rows 非正时
// 返回 -1。
int UiGridIndexAt(int originX, int originY, int columns, int rows, int px,
                  int py);

// 先铺满整块区域的 1px 分割线，再按顺序画槽位。items 为 NULL 或
// itemCount <= 0 时只画分割线，这是空网格的合法状态。
void UiDrawSlotGrid(int x, int y, int columns, int rows,
                    const UiSlotItem* items, int itemCount);

// 按码点截断 UTF-8。一个汉字算一个码点，不会切开多字节序列；不追加
// 省略号。结果始终以 '\0' 结尾（dstCapacity > 0 时）。src 为 NULL 或
// dstCapacity <= 0 时不写内存并返回 0；maxCodepoints <= 0 时写空串。
// 遇到非法 UTF-8 序列立即停止，已写入的完整码点保留。
int UiTruncateUtf8(const char* src, int maxCodepoints, char* dst,
                   int dstCapacity);

#endif  // UI_SLOT_H
