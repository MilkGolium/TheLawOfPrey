#include "graphsettings.h"
#include "scene_list.h"
#include "scene_manager.h"
#include "strings.h"
#include "ui.h"
#include "ui_slot.h"

// 界面原语图鉴。这一页只演示纯绘制的东西：品阶槽位、网格分割线、点阵
// 数字与 UTF-8 截断。它没有输入焦点，也没有跨帧状态；Ctrl+Q 返回标题。
// 页面容器、页签栏、滚动条与状态条属于下一批，不在这里。

// 六个槽位各用一张不同图标，正好覆盖 0..5 六种品阶。
enum { kSlotCount = 6 };

// 网格 5×2 的品阶与角标做混合，用来证明 1px 分割线。
enum { kGridColumns = 5, kGridRows = 2 };

static const char* const kIconPaths[kSlotCount] = {
    "assets/sprites/item_icons/bone.png",
    "assets/sprites/item_icons/branch.png",
    "assets/sprites/item_icons/feather.png",
    "assets/sprites/item_icons/stone.png",
    "assets/sprites/item_icons/wood.png",
    "assets/sprites/item_icons/resin.png",
};

// 六个独立槽位的角标：依次为 1、7、12、99、0、3。
static const int kSlotStacks[kSlotCount] = {1, 7, 12, 99, 0, 3};

// 网格 5×2 的品阶与角标做混合，用来证明 1px 分割线。
static const int kGridStacks[10] = {1, 5, 12, 0, 99, 3, 7, 0, 42, 8};

// 图鉴页的布局坐标全部写死为整数，左上原点。与 README「界面几何与
// 配色（游玩场景）」的槽位尺寸、节距一致。
static const int kTitleX = 12;
static const int kTitleY = 8;
static const int kTitleSize = 24;
static const int kLabelSize = 12;
static const int kSlotsLabelY = 48;
static const int kSlotsY = 64;
static const int kSlotStepX = 56;
static const int kGridLabelY = 136;
static const int kGridX = 12;
static const int kGridY = 152;
static const int kDigitsLabelY = 280;
static const int kDigitsY = 296;
static const int kSmallDigitsY = 312;
static const int kTruncateLabelY = 336;
static const int kFullTextY = 352;
static const int kTruncatedTextY = 368;
static const int kTextGap = 8;

static Texture2D sIcons[kSlotCount];
static UiSlotItem sSlots[kSlotCount];
static UiSlotItem sGridItems[kGridColumns * kGridRows];

// Ctrl+Q 是「返回」：图鉴页回标题，左右任一 Ctrl 都算。
static bool IsBackKeyPressed(void) {
  return IsKeyPressed(KEY_Q) &&
         (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL));
}

void UiGallerySceneInit(void) {
  for (int i = 0; i < kSlotCount; i++) {
    sIcons[i] = LoadTexture(kIconPaths[i]);
    sSlots[i] =
        (UiSlotItem){.icon = sIcons[i], .rarity = i, .stack = kSlotStacks[i]};
  }

  for (int i = 0; i < kGridColumns * kGridRows; i++) {
    sGridItems[i] = (UiSlotItem){.icon = sIcons[i % kSlotCount],
                                 .rarity = i % 6,
                                 .stack = kGridStacks[i]};
  }
}

void UiGallerySceneUpdate(void) {
  if (IsBackKeyPressed()) SceneManager_RequestSceneSwitch(&TitleScene);
}

void UiGallerySceneDraw(void) {
  UiDrawText(kTitleX, kTitleY, GetString("gallery_title"), kTitleSize,
             gUiStyle.titleText);
  UiDrawText(kTitleX, kSlotsLabelY, GetString("gallery_slots"), kLabelSize,
             gUiStyle.text);

  for (int i = 0; i < kSlotCount; i++) {
    UiDrawSlot(kTitleX + i * kSlotStepX, kSlotsY, &sSlots[i]);
  }

  UiDrawText(kTitleX, kGridLabelY, GetString("gallery_grid"), kLabelSize,
             gUiStyle.text);
  UiDrawSlotGrid(kGridX, kGridY, kGridColumns, kGridRows, sGridItems,
                 kGridColumns * kGridRows);

  UiDrawText(kTitleX, kDigitsLabelY, GetString("gallery_digits"), kLabelSize,
             gUiStyle.text);

  // 大号数字：0、7、42、99，各留 8px 间隔。固定两位、右对齐，
  // 前导位不补 0。
  static const int kBigValues[] = {0, 7, 42, 99};
  int x = kTitleX;
  for (int i = 0; i < (int)(sizeof(kBigValues) / sizeof(kBigValues[0])); i++) {
    UiDrawNumber(x, kDigitsY, kBigValues[i], 2, gUiStyle.text);
    x += UiNumberWidth(2) + kTextGap;
  }

  // 小号数字：12、34、56，间隔同样是 8px。
  static const int kSmallValues[] = {12, 34, 56};
  x = kTitleX;
  for (int i = 0; i < (int)(sizeof(kSmallValues) / sizeof(kSmallValues[0]));
       i++) {
    UiDrawNumberSmall(x, kSmallDigitsY, kSmallValues[i], 2, gUiStyle.text);
    x += 2 * (UI_DIGIT_SMALL_W + UI_DIGIT_GAP) + kTextGap;
  }

  UiDrawText(kTitleX, kTruncateLabelY, GetString("gallery_truncate"),
             kLabelSize, gUiStyle.text);

  const char* longText = GetString("gallery_long_text");
  UiDrawText(kTitleX, kFullTextY, longText, kLabelSize, gUiStyle.text);

  // 截到 10 个码点。buffer 64 字节对这条文案足够，截断函数自身也会
  // 保证不越界、不切开 UTF-8 序列。
  char truncated[64];
  UiTruncateUtf8(longText, 10, truncated, (int)sizeof(truncated));
  UiDrawText(kTitleX, kTruncatedTextY, truncated, kLabelSize, gUiStyle.textDim);
}

void UiGallerySceneUnload(void) {
  for (int i = 0; i < kSlotCount; i++) {
    UnloadTexture(sIcons[i]);
    sIcons[i] = (Texture2D){0};
  }
}

Scene UiGalleryScene = {.SceneInit = UiGallerySceneInit,
                        .SceneUpdate = UiGallerySceneUpdate,
                        .SceneDraw = UiGallerySceneDraw,
                        .SceneUnload = UiGallerySceneUnload};
