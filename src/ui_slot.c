#include "ui_slot.h"

#include <stdint.h>
#include <string.h>

#include "ui.h"

// === 点阵数字 ===
//
// 字形以 60 位/15 位掩码存放，绘制时逐位还原成 1×1 矩形。位序在文件头
// 与 ui_slot.h 里固定为 bit = row * width + col：row 0 为顶行、col 0 为
// 左列，最低位对应左上角。改字形只改表，不改绘制路径。
static const uint64_t kDigits6x10[10] = {
    0x07A186186186185EULL,  // 0
    0x0384104104104184ULL,  // 1
    0x0FC108421082085EULL,  // 2
    0x07A182081C82085EULL,  // 3
    0x041043F451494610ULL,  // 4
    0x07A18208207C107FULL,  // 5
    0x07A186185F04109CULL,  // 6
    0x010410821042087FULL,  // 7
    0x07A186185E86185EULL,  // 8
    0x039082083E86185EULL,  // 9
};

static const uint16_t kDigits3x5[10] = {
    0x7B6F,  // 0
    0x749A,  // 1
    0x73E7,  // 2
    0x79E7,  // 3
    0x49ED,  // 4
    0x79CF,  // 5
    0x7BCF,  // 6
    0x4927,  // 7
    0x7BEF,  // 8
    0x79EF,  // 9
};

// 逐位绘制一个掩码字形。width 与 height 只取 6×10 或 3×5，位序与表一致。
static void DrawMaskedGlyph(int x, int y, int width, int height, uint64_t mask,
                            Color tint) {
  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      if ((mask & (1ULL << (row * width + col))) != 0)
        DrawRectangle(x + col, y + row, 1, 1, tint);
    }
  }
}

int UiNumberWidth(int digits) {
  if (digits <= 0) return 0;
  return digits * UI_DIGIT_W + (digits - 1) * UI_DIGIT_GAP;
}

// 两个数字函数的公共骨架：钳制取值与位数，再从右往左填满 digits 位。
// 前导位不补 0：value 除到 0 就停，左端保留空位。
static int ClampNumberDigits(int digits) {
  if (digits < 1) return 1;
  if (digits > 2) return 2;
  return digits;
}

static int ClampNumberValue(int value) {
  if (value < 0) return 0;
  if (value > 99) return 99;
  return value;
}

void UiDrawNumber(int x, int y, int value, int digits, Color tint) {
  value = ClampNumberValue(value);
  digits = ClampNumberDigits(digits);

  for (int place = digits - 1; place >= 0; place--) {
    int digit = value % 10;
    DrawMaskedGlyph(x + place * (UI_DIGIT_W + UI_DIGIT_GAP), y, UI_DIGIT_W,
                    UI_DIGIT_H, kDigits6x10[digit], tint);
    value /= 10;
    if (value == 0) break;
  }
}

void UiDrawNumberSmall(int x, int y, int value, int digits, Color tint) {
  value = ClampNumberValue(value);
  digits = ClampNumberDigits(digits);

  for (int place = digits - 1; place >= 0; place--) {
    int digit = value % 10;
    DrawMaskedGlyph(x + place * (UI_DIGIT_SMALL_W + UI_DIGIT_GAP), y,
                    UI_DIGIT_SMALL_W, UI_DIGIT_SMALL_H, kDigits3x5[digit],
                    tint);
    value /= 10;
    if (value == 0) break;
  }
}

// === 槽位 ===

// 越界告警只报一次：绘制路径每帧都会经过这里，重复告警会把日志刷爆。
// 钳制仍然逐次执行，告警只是提示调用方传错了值。
static bool sRarityWarned = false;

Color UiRarityColor(int rarity) {
  if (rarity < 0) rarity = 0;
  if (rarity > 5) rarity = 5;

  switch (rarity) {
    case 1:
      return gUiStyle.rarityUncommon;
    case 2:
      return gUiStyle.rarityRare;
    case 3:
      return gUiStyle.rarityEpic;
    case 4:
      return gUiStyle.rarityLegendary;
    case 5:
      return gUiStyle.rarityMythic;
    default:
      return gUiStyle.rarityCommon;
  }
}

void UiDrawSlot(int x, int y, const UiSlotItem* item) {
  if (item == NULL) return;

  int rarity = item->rarity;
  if (rarity < 0 || rarity > 5) {
    if (!sRarityWarned) {
      sRarityWarned = true;
      TraceLog(LOG_WARNING,
               "UiSlot: rarity %d is out of range 0..5, value clamped", rarity);
    }
    rarity = (rarity < 0) ? 0 : 5;
  }

  // 品阶外框：整个 52×52 先铺品阶色。
  DrawRectangle(x, y, UI_SLOT_SIZE, UI_SLOT_SIZE, UiRarityColor(rarity));

  // 槽位内部底色：四周内缩 2px。
  DrawRectangle(x + UI_SLOT_BORDER, y + UI_SLOT_BORDER,
                UI_SLOT_SIZE - 2 * UI_SLOT_BORDER,
                UI_SLOT_SIZE - 2 * UI_SLOT_BORDER, gUiStyle.slotBg);

  // 图标正好铺满内区：52 − 2×2 = 48，与图标尺寸相同，不能再内缩。
  // （早先这里多缩了 1px，48×48 的图标会顶掉右/下边框各 1px。）
  if (item->icon.id != 0) {
    DrawTexture(item->icon, x + UI_SLOT_BORDER, y + UI_SLOT_BORDER, WHITE);
  }

  // 数量角标：右下角内缩 2px，固定两位、右对齐、只画非空位。
  int stack = item->stack;
  if (stack > 99) stack = 99;
  if (stack > 0) {
    UiDrawNumber(x + UI_SLOT_SIZE - 2 - UiNumberWidth(2),
                 y + UI_SLOT_SIZE - 2 - UI_DIGIT_H, stack, 2, gUiStyle.text);
  }
}

// === 网格几何 ===

UiPoint UiGridCellPos(int originX, int originY, int columns, int index) {
  if (columns <= 0 || index < 0) return (UiPoint){originX, originY};
  return (UiPoint){originX + (index % columns) * UI_SLOT_PITCH,
                   originY + (index / columns) * UI_SLOT_PITCH};
}

int UiGridIndexAt(int originX, int originY, int columns, int rows, int px,
                  int py) {
  if (columns <= 0 || rows <= 0) return -1;

  int dx = px - originX;
  int dy = py - originY;
  if (dx < 0 || dy < 0) return -1;

  int column = dx / UI_SLOT_PITCH;
  int row = dy / UI_SLOT_PITCH;
  if (column >= columns || row >= rows) return -1;

  // 相对坐标除以 53 余 52 的那一列/行是共用分割线，不属于任何格子。
  if (dx % UI_SLOT_PITCH == UI_SLOT_SIZE || dy % UI_SLOT_PITCH == UI_SLOT_SIZE)
    return -1;

  return row * columns + column;
}

void UiDrawSlotGrid(int x, int y, int columns, int rows,
                    const UiSlotItem* items, int itemCount) {
  if (columns <= 0 || rows <= 0) return;

  int width = columns * UI_SLOT_PITCH - 1;
  int height = rows * UI_SLOT_PITCH - 1;

  // 整块区域先铺满分割线：纵向线在 52 + k*53 列，横向线在同一行号。
  // 槽位随后画在格子里，正好让开这些像素。
  for (int column = 0; column < columns - 1; column++) {
    DrawRectangle(x + UI_SLOT_SIZE + column * UI_SLOT_PITCH, y, 1, height,
                  gUiStyle.slotDivider);
  }
  for (int row = 0; row < rows - 1; row++) {
    DrawRectangle(x, y + UI_SLOT_SIZE + row * UI_SLOT_PITCH, width, 1,
                  gUiStyle.slotDivider);
  }

  if (items == NULL || itemCount <= 0) return;

  int cellCount = columns * rows;
  if (itemCount > cellCount) itemCount = cellCount;
  for (int i = 0; i < itemCount; i++) {
    UiPoint cell = UiGridCellPos(x, y, columns, i);
    UiDrawSlot(cell.x, cell.y, &items[i]);
  }
}

// === 文本截断 ===

// 返回一个合法 UTF-8 序列的字节数，非法序列返回 0。校验覆盖过长编码、
// 代理区与 U+10FFFF 以上的码点，避免把半个序列或非法字节写进结果。
static int Utf8SequenceLength(const char* text) {
  const unsigned char* s = (const unsigned char*)text;
  unsigned char lead = s[0];
  if (lead == 0) return 0;
  if (lead < 0x80) return 1;

  int length = 0;
  unsigned char secondLow = 0x80;
  unsigned char secondHigh = 0xBF;

  if (lead >= 0xC2 && lead <= 0xDF) {
    length = 2;
  } else if (lead >= 0xE0 && lead <= 0xEF) {
    length = 3;
    if (lead == 0xE0) secondLow = 0xA0;   // 排除过长编码
    if (lead == 0xED) secondHigh = 0x9F;  // 排除 UTF-16 代理区
  } else if (lead >= 0xF0 && lead <= 0xF4) {
    length = 4;
    if (lead == 0xF0) secondLow = 0x90;   // 排除过长编码
    if (lead == 0xF4) secondHigh = 0x8F;  // 排除 U+10FFFF 以上
  } else {
    return 0;
  }

  if (s[1] < secondLow || s[1] > secondHigh) return 0;
  for (int i = 2; i < length; i++) {
    if (s[i] < 0x80 || s[i] > 0xBF) return 0;
  }
  return length;
}

int UiTruncateUtf8(const char* src, int maxCodepoints, char* dst,
                   int dstCapacity) {
  if (src == NULL || dstCapacity <= 0) return 0;

  dst[0] = '\0';
  if (maxCodepoints <= 0) return 0;

  int writtenBytes = 0;
  int writtenCodepoints = 0;
  int offset = 0;

  while (writtenCodepoints < maxCodepoints && src[offset] != '\0') {
    int length = Utf8SequenceLength(src + offset);
    if (length == 0) break;

    // 预留一个字节给结尾的 '\0'，放不下就停在最后一个完整码点之后。
    if (writtenBytes + length + 1 > dstCapacity) break;

    memcpy(dst + writtenBytes, src + offset, (size_t)length);
    writtenBytes += length;
    offset += length;
    writtenCodepoints++;
  }

  dst[writtenBytes] = '\0';
  return writtenCodepoints;
}
