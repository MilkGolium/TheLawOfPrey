#include "ui_gallery_scene.h"

#include "graphsettings.h"
#include "scene_list.h"
#include "scene_manager.h"
#include "strings.h"
#include "ui.h"
#include "ui_page.h"
#include "ui_slot.h"

// 界面原语图鉴。左侧是六项页签栏，右侧内容区按页展示 A1 与 A2 的全部
// 原语：品阶槽位、网格分割线、点阵数字、UTF-8 截断、状态条与滚动条。
// 页签是本场景唯一的输入（鼠标左键），Ctrl+Q 返回标题。键盘切页要等
// 修饰键跨平台实测，本批不做。

// 六个槽位各用一张不同图标，正好覆盖 0..5 六种品阶。
enum { kSlotCount = 6 };

// 网格 5×2 的品阶与角标做混合，用来证明 1px 分割线。
enum { kGridColumns = 5, kGridRows = 2 };

// 滚动列表页的 4×4 槽位网格。
enum { kScrollColumns = 4, kScrollRows = 4 };

// 六项页签、六个内容页。
enum { kTabCount = 6, kPageCount = 6 };

static const char* const kTabKeys[kTabCount] = {
    "gallery_tab_slots",    "gallery_tab_grid", "gallery_tab_digits",
    "gallery_tab_truncate", "gallery_tab_bars", "gallery_tab_scroll",
};

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

// 内容区从页签栏右侧开始；标题与左侧留白沿用 A1 的 12px，整体右移 40px。
static const int kContentMarginX = UI_PAGE_TAB_WIDTH + 12;
static const int kTitleY = 8;
static const int kTitleSize = 24;
static const int kLabelSize = 12;
static const int kSlotsLabelY = 48;
static const int kSlotsY = 64;
static const int kGridLabelY = 136;
static const int kGridY = 152;
static const int kDigitsLabelY = 280;
static const int kDigitsY = 296;
static const int kSmallDigitsY = 312;
static const int kTruncateLabelY = 336;
static const int kFullTextY = 352;
static const int kTruncatedTextY = 368;
static const int kTextGap = 8;

// 状态条页：三条 200px 宽的条，标签在条上方。
static const int kBarX = 60;
static const int kBarWidth = 200;
static const int kBarLabelY[3] = {64, 128, 192};
static const int kBarY[3] = {80, 144, 208};
static const int kBarValues[3] = {0, 50, 100};

// 滚动条页：4×4 网格右侧贴一条滚动条，再向右 40px 放两个演示滑块。
static const int kScrollGridX = 60;
static const int kScrollGridY = 40;
// 4 列 × 53 − 1 = 211，既是网格高度也是滚动条轨道高度。
static const int kScrollGridSize = kScrollColumns * UI_SLOT_PITCH - 1;
static const int kScrollbarX = kScrollGridX + kScrollGridSize + 1;
static const int kScrollContentCount = 40;
static const int kScrollVisibleCount = 16;
static const int kDemoScrollbarX = kScrollbarX + 40;
static const int kDemoScrollbarGap = 20;
static const int kDemoNoteY = kScrollGridY + kScrollGridSize + 16;

static Texture2D sIcons[kSlotCount];
static UiSlotItem sSlots[kSlotCount];
static UiSlotItem sGridItems[kGridColumns * kGridRows];
static UiSlotItem sScrollItems[kScrollColumns * kScrollRows];

// 当前页由场景持有，绘制原语不保存跨帧状态。
static int sPage = 0;

void UiGallerySetPage(int page) {
  if (page < 0) page = 0;
  if (page >= kPageCount) page = kPageCount - 1;
  sPage = page;
}

int UiGalleryGetPage(void) { return sPage; }

// Ctrl+Q 是「返回」：图鉴页回标题，左右任一 Ctrl 都算。
static bool IsBackKeyPressed(void) {
  return IsKeyPressed(KEY_Q) &&
         (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL));
}

void UiGallerySceneInit(void) {
  sPage = 0;

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

  for (int i = 0; i < kScrollColumns * kScrollRows; i++) {
    sScrollItems[i] = (UiSlotItem){.icon = sIcons[i % kSlotCount],
                                   .rarity = i % 6,
                                   .stack = kGridStacks[i % 10]};
  }
}

void UiGallerySceneUpdate(void) {
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    // 命中测试用设计画布坐标：窗口坐标在整数倍缩放与居中黑边下不通用。
    Vector2 mouse = GetCanvasMouse();
    int tab = UiPageTabAt((int)mouse.x, (int)mouse.y, kTabCount);
    if (tab >= 0) UiGallerySetPage(tab);
  }

  if (IsBackKeyPressed()) SceneManager_RequestSceneSwitch(&TitleScene);
}

static void DrawPageTitle(void) {
  UiDrawText(kContentMarginX, kTitleY, GetString("gallery_title"), kTitleSize,
             gUiStyle.titleText);
}

// 第 1 页：六阶槽位。用 6×1 网格画，槽位之间保留 1px 共用分割线。
static void DrawSlotsPage(void) {
  DrawPageTitle();
  UiDrawText(kContentMarginX, kSlotsLabelY, GetString("gallery_slots"),
             kLabelSize, gUiStyle.text);
  UiDrawSlotGrid(kContentMarginX, kSlotsY, kSlotCount, 1, sSlots, kSlotCount);
}

// 第 2 页：5×2 网格与 1px 分割线。
static void DrawGridPage(void) {
  DrawPageTitle();
  UiDrawText(kContentMarginX, kGridLabelY, GetString("gallery_grid"),
             kLabelSize, gUiStyle.text);
  UiDrawSlotGrid(kContentMarginX, kGridY, kGridColumns, kGridRows, sGridItems,
                 kGridColumns * kGridRows);
}

// 第 3 页：6×10 与 3×5 点阵数字。
static void DrawDigitsPage(void) {
  DrawPageTitle();
  UiDrawText(kContentMarginX, kDigitsLabelY, GetString("gallery_digits"),
             kLabelSize, gUiStyle.text);

  // 大号数字：0、7、42、99，各留 8px 间隔。固定两位、右对齐，
  // 前导位不补 0。
  static const int kBigValues[] = {0, 7, 42, 99};
  int x = kContentMarginX;
  for (int i = 0; i < (int)(sizeof(kBigValues) / sizeof(kBigValues[0])); i++) {
    UiDrawNumber(x, kDigitsY, kBigValues[i], 2, gUiStyle.text);
    x += UiNumberWidth(2) + kTextGap;
  }

  // 小号数字：12、34、56，间隔同样是 8px。
  static const int kSmallValues[] = {12, 34, 56};
  x = kContentMarginX;
  for (int i = 0; i < (int)(sizeof(kSmallValues) / sizeof(kSmallValues[0]));
       i++) {
    UiDrawNumberSmall(x, kSmallDigitsY, kSmallValues[i], 2, gUiStyle.text);
    x += 2 * (UI_DIGIT_SMALL_W + UI_DIGIT_GAP) + kTextGap;
  }
}

// 第 4 页：完整长文本与按码点截断后的对照。
static void DrawTruncatePage(void) {
  DrawPageTitle();
  UiDrawText(kContentMarginX, kTruncateLabelY, GetString("gallery_truncate"),
             kLabelSize, gUiStyle.text);

  const char* longText = GetString("gallery_long_text");
  UiDrawText(kContentMarginX, kFullTextY, longText, kLabelSize, gUiStyle.text);

  // 截到 10 个码点。buffer 64 字节对这条文案足够，截断函数自身也会
  // 保证不越界、不切开 UTF-8 序列。
  char truncated[64];
  UiTruncateUtf8(longText, 10, truncated, (int)sizeof(truncated));
  UiDrawText(kContentMarginX, kTruncatedTextY, truncated, kLabelSize,
             gUiStyle.textDim);
}

// 第 5 页：三条状态条，值分别为 0、50、100。
static void DrawBarsPage(void) {
  DrawPageTitle();
  static const char* const kBarKeys[3] = {
      "gallery_bar_hunger", "gallery_bar_stamina", "gallery_bar_health"};
  for (int i = 0; i < 3; i++) {
    UiDrawText(kBarX, kBarLabelY[i], GetString(kBarKeys[i]), kLabelSize,
               gUiStyle.text);
    UiDrawBar(kBarX, kBarY[i], kBarWidth, kBarValues[i], 100);
  }
}

// 第 6 页：4×4 槽位网格、贴右的滚动条，以及两个不同 firstVisible 的
// 演示滑块（中间值与最大值）。
static void DrawScrollPage(void) {
  DrawPageTitle();
  UiDrawSlotGrid(kScrollGridX, kScrollGridY, kScrollColumns, kScrollRows,
                 sScrollItems, kScrollColumns * kScrollRows);

  UiDrawScrollbar(kScrollbarX, kScrollGridY, kScrollGridSize,
                  kScrollContentCount, kScrollVisibleCount, 0);

  int maxFirst = kScrollContentCount - kScrollVisibleCount;
  UiDrawScrollbar(kDemoScrollbarX, kScrollGridY, kScrollGridSize,
                  kScrollContentCount, kScrollVisibleCount, maxFirst / 2);
  UiDrawScrollbar(kDemoScrollbarX + kDemoScrollbarGap, kScrollGridY,
                  kScrollGridSize, kScrollContentCount, kScrollVisibleCount,
                  maxFirst);

  UiDrawText(kDemoScrollbarX, kDemoNoteY, GetString("gallery_scroll_note"),
             kLabelSize, gUiStyle.text);
}

void UiGallerySceneDraw(void) {
  const char* labels[kTabCount];
  for (int i = 0; i < kTabCount; i++) labels[i] = GetString(kTabKeys[i]);
  UiDrawPageTabs(sPage, kTabCount, labels);

  // 遮挡页面铺满整张画布：页签栏之后的内容区用 background 填底。
  Rectangle content = UiPageContentRect();
  DrawRectangle((int)content.x, (int)content.y, (int)content.width,
                (int)content.height, gUiStyle.background);

  switch (sPage) {
    case 1:
      DrawGridPage();
      break;
    case 2:
      DrawDigitsPage();
      break;
    case 3:
      DrawTruncatePage();
      break;
    case 4:
      DrawBarsPage();
      break;
    case 5:
      DrawScrollPage();
      break;
    default:
      DrawSlotsPage();
      break;
  }
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
