#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "font.h"
#include "graphsettings.h"
#include "scene_list.h"
#include "scene_manager.h"
#include "strings.h"
#include "test_util.h"
#include "theme.h"
#include "ui.h"
#include "ui_gallery_scene.h"
#include "ui_page.h"
#include "ui_slot.h"
#include "utils.h"

// 界面层的度量与居中回归测试：把设计画布画一遍，再把像素回读到 CPU 内存
// 逐行核对。这些路径原先只靠临时回读程序人眼确认，公式一改没人拦得住。

#define kProbeX 48
#define kProbeY 48
#define kProbeW 240
#define kProbeH 120
#define kProbeButtonX 48
#define kProbeButtonY 300
#define kInkRowCapacity 128
#define kInkRunCapacity 8

// 逐行剖面的扫描窗口，刻意比标题栏大：内缩量或居中算错时墨迹会落到标题栏
// 之外，窗口正好等于目标区域就会把墨迹截断，量出偏小的尺寸反过来掩盖偏移。
#define kProbeScanX 0
#define kProbeScanY 0
#define kProbeScanW (kProbeX + kProbeW)
#define kProbeScanH (kProbeY + kProbeH + 2 * UI_PADDING)
// 容量必须不小于最高的扫描窗口（kProbeScanH = 192）。取 512 是因为画布
// 只有 480 行，任何留在画布内的窗口都不可能写越界。
#define kRowProfileCapacity 512

// 文案取自 assets/strings/<lang>，字体 codepoint 是精确收集的，写死在 C
// 字面量里 的中文不在图集里，直接变方框。因此这里只用数据文件里的 key。
static const char* const kTitleKey = "messagebox_title";
static const char* const kButtonKey = "button_ok";

// README 实测记录的余量：标题墨迹并集高在 12/24/36 号下为 11/22/33，
// 三色边框吃掉 3 圈之后 12 号余量为 0、字顶字底贴边。下面三个常量把
// 这些数字钉死，改了内缩公式会立刻失败。
static const int kTitleInkHeight12 = 11;
static const int kTitleInkTop12 = 3;
static const int kTitleInkBottom12 = 13;

// === 回读配色 ===
//
// 判定墨迹要靠颜色精确比对，所以测试自己指定一套两两不相同的颜色：标题字
// 与按钮字各自独占一种颜色，不会与边框、填充、阴影混淆。
static void ApplyProbeStyle(void) {
  gUiStyle.desktop = (Color){0x10, 0x10, 0x20, 0xff};
  gUiStyle.background = (Color){0x20, 0x20, 0x40, 0xff};
  gUiStyle.line = (Color){0x00, 0xff, 0x00, 0xff};
  gUiStyle.lineDim = (Color){0xff, 0x00, 0xff, 0xff};
  gUiStyle.text = (Color){0xff, 0xff, 0x00, 0xff};
  gUiStyle.textDim = (Color){0x00, 0xff, 0xff, 0xff};
  gUiStyle.button = (Color){0x80, 0x40, 0x00, 0xff};
  gUiStyle.buttonText = (Color){0xff, 0xff, 0xff, 0xff};
  gUiStyle.titleBar = (Color){0x00, 0x00, 0x00, 0xff};
  gUiStyle.titleText = (Color){0xff, 0x88, 0x00, 0xff};
  gUiStyle.shadow = (Color){0x40, 0x40, 0x40, 0xff};
  gUiStyle.shadowStyle = UI_SHADOW_SOLID;
  gUiStyle.borderStyle = UI_BORDER_NONE;
}

// 圈数是文字要避让的垂直内缩量。这里按契约另写一份而不是复用 ui.c 的
// 静态函数：测试要断言的就是「圈数等于几」，复用实现等于自己判自己。
static int RingCountOf(UiBorderStyle style) {
  if (style == UI_BORDER_SINGLE) return 1;
  if (style == UI_BORDER_TRIPLE) return 3;
  return 0;
}

// 渲染纹理在 GL 里上下颠倒，回读到的图像第 0 行对应画布末行，
// 因此画布坐标要先翻正再交给 GetImageColor。
static Color PixelAt(Image image, int canvasX, int canvasY) {
  return GetImageColor(image, canvasX, image.height - 1 - canvasY);
}

static bool IsInk(Color pixel, Color ink) {
  return pixel.r == ink.r && pixel.g == ink.g && pixel.b == ink.b &&
         pixel.a == ink.a;
}

static int CountInkPixels(Image image, int x, int y, int width, int height,
                          Color ink) {
  int count = 0;
  for (int row = y; row < y + height; row++) {
    for (int column = x; column < x + width; column++) {
      if (IsInk(PixelAt(image, column, row), ink)) count++;
    }
  }
  return count;
}

// 与 CountInkPixels 相反：统计不是 color 的像素。页签容器的底层铺满
// gUiStyle.background，用这个判「内容区确实画了东西」而不是只铺了底色。
static int CountDifferentPixels(Image image, int x, int y, int width,
                                int height, Color color) {
  int count = 0;
  for (int row = y; row < y + height; row++) {
    for (int column = x; column < x + width; column++) {
      if (!IsInk(PixelAt(image, column, row), color)) count++;
    }
  }
  return count;
}

// 一行里第一个墨迹像素的横坐标，找不到返回 -1。滑块两端渐缩时用它判
// 「居中没有跑偏」：只数宽度看不出 1px 的偏移。
static int FirstInkX(Image image, int x, int y, int width, Color ink) {
  for (int column = x; column < x + width; column++) {
    if (IsInk(PixelAt(image, column, y), ink)) return column;
  }
  return -1;
}

// 收集含墨迹的行，并合并成若干条连续行段。同一段墨迹的高度就是字形墨迹
// 的实际高度，段与段之间的上缘差就是行距。
typedef struct {
  int runCount;
  int runTop[kInkRunCapacity];
  int runBottom[kInkRunCapacity];
  int pixelCount;
} InkScan;

static InkScan ScanInk(Image image, int x, int y, int width, int height,
                       Color ink) {
  InkScan scan = {0, {0}, {0}, 0};
  bool inRun = false;
  int lastInkedRow = -1;

  for (int row = y; row < y + height; row++) {
    int rowCount = 0;
    for (int column = x; column < x + width; column++) {
      if (IsInk(PixelAt(image, column, row), ink)) rowCount++;
    }
    scan.pixelCount += rowCount;

    if (rowCount > 0) {
      if (!inRun && scan.runCount < kInkRunCapacity) {
        scan.runTop[scan.runCount] = row;
        scan.runCount++;
      }
      inRun = true;
      lastInkedRow = row;
    } else if (inRun) {
      scan.runBottom[scan.runCount - 1] = lastInkedRow;
      inRun = false;
    }
  }
  if (inRun) scan.runBottom[scan.runCount - 1] = lastInkedRow;
  return scan;
}

static void ReportRunCount(const char* what, InkScan scan) {
  if (scan.runCount == 0) {
    printf("      [measured] %s: no ink\n", what);
  } else {
    printf("      [measured] %s: %d ink run(s), ", what, scan.runCount);
    for (int i = 0; i < scan.runCount; i++)
      printf("run%d [%d, %d] ", i + 1, scan.runTop[i], scan.runBottom[i]);
    printf("\n");
  }
  fflush(stdout);
}

// === 逐行墨迹剖面与主体块 ===
//
// 垂直居中的「主体块」是回归保护的核心。下伸部（拉丁 g、y）会把整串墨迹盒
// 往下撑长，按墨迹盒居中会让字母主体偏上；按主体块（到基线为止）居中才
// 对。回读像素后按行统计墨迹数，主体块取「首条墨迹行到最后一个不低于
// 峰值一半的行」：下伸部是稀疏的少数笔画，逐行像素数会掉到峰值的一半
// 以下，于是被排除在外。这与 src/ui.c 的 TextBodyBounds 是同一条语义，
// 但判据来自渲染结果而非字体度量，两边独立。
typedef struct {
  int rows[kRowProfileCapacity];
  int firstInked;
  int lastInked;
  int peak;
  int pixelCount;
} RowProfile;

// rows 直接用画布行号做下标，firstInked / lastInked 也是画布行号，
// 免得调用方再补一遍扫描窗口的偏移。
static RowProfile ProfileRows(Image image, int x, int y, int width, int height,
                              Color ink) {
  RowProfile profile = {.firstInked = -1, .lastInked = -1};
  for (int row = y; row < y + height; row++) {
    int count = 0;
    for (int column = x; column < x + width; column++) {
      if (IsInk(PixelAt(image, column, row), ink)) count++;
    }
    profile.rows[row] = count;
    profile.pixelCount += count;
    if (count == 0) continue;
    if (profile.firstInked < 0) profile.firstInked = row;
    profile.lastInked = row;
    if (count > profile.peak) profile.peak = count;
  }
  return profile;
}

// 首条墨迹行到最后一个不低于峰值一半的行。峰值太小（整串只剩一根细竖笔
// 之类）时阈值退化成 0，主体块等于整条墨迹，下面那条余量断言就拦不住
// 偏移——所以调用方必须先断言墨迹像素数大于 0，并断言主体块确实短于整条
// 墨迹，否则这条判据会退化成「按墨迹盒居中」而把真正的 bug 放过。
static void BodyBlockOf(RowProfile profile, int* outTop, int* outBottom) {
  *outTop = profile.firstInked;
  *outBottom = profile.firstInked;
  for (int row = profile.firstInked; row <= profile.lastInked; row++) {
    if (profile.rows[row] * 2 >= profile.peak) *outBottom = row;
  }
}

// 一次探针的结果，bodyTop / bodyBottom 是画布行号。上下余量相对边框内区
// 算：内区只在顶部让开 ring0，下缘就是标题栏底 / 按钮底（框底三圈与标题栏
// 不相交，见 README「字体与界面方案约束」里边框圈数那条），与 ui.c 的两处
// 居中公式用的是同一个内缩量。
typedef struct {
  RowProfile profile;
  int bodyTop;
  int bodyBottom;
  int topMargin;
  int bottomMargin;
} BodyProbe;

static void FinishBody(BodyProbe* probe, int innerTop, int innerBottom) {
  int bodyTop = 0;
  int bodyBottom = 0;
  BodyBlockOf(probe->profile, &bodyTop, &bodyBottom);
  probe->bodyTop = bodyTop;
  probe->bodyBottom = bodyBottom;
  probe->topMargin = bodyTop - innerTop;
  probe->bottomMargin = innerBottom - (bodyBottom + 1);
}

static BodyProbe ProbeTitleBody(const char* title, int size, int rings) {
  BodyProbe probe;
  int barHeight = UI_TITLEBAR_HEIGHT(size);
  BeginCanvas();
  ClearBackground(gUiStyle.desktop);
  UiDrawBox(kProbeX, kProbeY, kProbeW, kProbeH, title, size);
  EndCanvas();
  Image image = LoadImageFromTexture(GetCanvasTexture());
  probe.profile = ProfileRows(image, kProbeScanX, kProbeScanY, kProbeScanW,
                              kProbeScanH, gUiStyle.titleText);
  UnloadImage(image);
  FinishBody(&probe, kProbeY + rings, kProbeY + barHeight);
  return probe;
}

// 按钮宽度按文案度量推出，与 ui.c 里摆按钮行用的是同一条算式。
static BodyProbe ProbeButtonBody(const char* label, int size, int rings) {
  BodyProbe probe;
  int height = UI_BUTTON_HEIGHT(size);
  int width = UI_ALIGN12(UiMeasureText(label, size).width + 2 * UI_PADDING);
  BeginCanvas();
  ClearBackground(gUiStyle.desktop);
  UiButton(kProbeButtonX, kProbeButtonY, width, height, label, size, false);
  EndCanvas();
  Image image = LoadImageFromTexture(GetCanvasTexture());
  probe.profile = ProfileRows(
      image, kProbeButtonX - 2 * UI_PADDING, kProbeButtonY - 2 * UI_PADDING,
      width + 4 * UI_PADDING, height + 4 * UI_PADDING, gUiStyle.buttonText);
  UnloadImage(image);
  FinishBody(&probe, kProbeButtonY + rings, kProbeButtonY + height);
  return probe;
}

static void ReportBody(const char* what, BodyProbe probe) {
  printf("      [measured] %s: ink rows %d..%d, peak %d px/row, %d ink px\n",
         what, probe.profile.firstInked, probe.profile.lastInked,
         probe.profile.peak, probe.profile.pixelCount);
  printf("      [measured] %s: row profile (canvas row:pixels)", what);
  for (int row = probe.profile.firstInked; row <= probe.profile.lastInked;
       row++) {
    if (probe.profile.rows[row] > 0)
      printf(" %d:%d", row, probe.profile.rows[row]);
  }
  printf("\n");
  printf(
      "      [measured] %s: body block rows %d..%d, top margin %d, bottom "
      "margin %d\n",
      what, probe.bodyTop, probe.bodyBottom, probe.topMargin,
      probe.bottomMargin);
  fflush(stdout);
}

// 主体块上下余量之差不超过 1 行。判据用对称量而不是「没有越界」：按墨迹盒
// 居中时墨迹只是整体上移，既不越出容器、像素总数也不变，只判越界会放过。
static void CheckBodyCentered(const char* what, BodyProbe probe) {
  char detail[192];
  TestCheckInt(probe.profile.pixelCount > 0 ? 1 : 0, 1,
               "the probed text has ink (otherwise the margin gap below is "
               "meaningless)");
  if (probe.profile.pixelCount == 0) return;
  int gap = probe.topMargin > probe.bottomMargin
                ? probe.topMargin - probe.bottomMargin
                : probe.bottomMargin - probe.topMargin;
  snprintf(detail, sizeof(detail),
           "%s is centered by body block (body rows %d..%d, top margin %d, "
           "bottom margin %d, gap %d)",
           what, probe.bodyTop, probe.bodyBottom, probe.topMargin,
           probe.bottomMargin, gap);
  TestCheck(gap <= 1, detail);
}

// === 用例 ===

// 行距公式：度量按 UI_LINE_STEP 累加，渲染回读的段间距同样等于它。
// 多行文本两行用同一串字形，墨迹上缘对齐，段间距就是行距本身。
static void CaseLineStep(void) {
  TestBeginCase(
      "line step: measured equals line count times step, render readback moves "
      "line 2 down by exactly one step");
  ApplyProbeStyle();

  static const int kSizes[] = {12, 24, 36};
  const char* text = "OK\nOK";

  for (int i = 0; i < 3; i++) {
    int size = kSizes[i];
    TestCheckInt(UI_LINE_STEP(size), size,
                 "the line step macro equals the font size");

    UiSize measured = UiMeasureText(text, size);
    TestCheckInt(measured.height, size + (2 - 1) * UI_LINE_STEP(size),
                 "measured height of two lines = size + one line step");
    TestCheckInt(measured.height, 2 * UI_LINE_STEP(size),
                 "measured height of two lines = line count * line step");

    BeginCanvas();
    ClearBackground(gUiStyle.desktop);
    UiDrawText(kProbeX, kProbeY, text, size, gUiStyle.text);
    EndCanvas();
    Image image = LoadImageFromTexture(GetCanvasTexture());
    InkScan scan = ScanInk(image, kProbeX, kProbeY, measured.width + 4,
                           measured.height + size, gUiStyle.text);
    UnloadImage(image);

    ReportRunCount("ink of the two-line text", scan);
    TestCheckInt(scan.runCount, 2, "two ink runs read back (one per line)");
    TestCheckInt(scan.pixelCount > 0 ? 1 : 0, 1, "both ink runs are non-empty");
    if (scan.runCount == 2) {
      TestCheckInt(scan.runTop[1] - scan.runTop[0], UI_LINE_STEP(size),
                   "the line 2 ink top is exactly one line step below line 1");
    } else {
      TestFail("could not locate two ink runs, cannot verify the line step");
    }
  }
  TestEndCase();
}

// 边框不吃文字：标题墨迹画在边框之后，无边框与三色边框下的墨迹像素数
// 必须逐档相等。若绘制顺序反了，ring0 会盖掉墨迹上缘，两档立刻不等。
static void CaseBorderDoesNotCoverTitle(void) {
  TestBeginCase(
      "border does not cover text: title ink pixel count matches step by step "
      "under none and triple");
  ApplyProbeStyle();

  const char* title = GetString(kTitleKey);
  TestCheck(strcmp(title, kTitleKey) != 0,
            "the title string comes from the data file, not a fallback to the "
            "key itself");

  static const int kSizes[] = {12, 24, 36};
  for (int i = 0; i < 3; i++) {
    int size = kSizes[i];
    int withoutBorder = 0;
    int withTriple = 0;

    gUiStyle.borderStyle = UI_BORDER_NONE;
    BeginCanvas();
    ClearBackground(gUiStyle.desktop);
    UiDrawBox(kProbeX, kProbeY, kProbeW, kProbeH, title, size);
    EndCanvas();
    Image image = LoadImageFromTexture(GetCanvasTexture());
    withoutBorder = CountInkPixels(image, kProbeX, kProbeY, kProbeW, kProbeH,
                                   gUiStyle.titleText);
    UnloadImage(image);

    gUiStyle.borderStyle = UI_BORDER_TRIPLE;
    BeginCanvas();
    ClearBackground(gUiStyle.desktop);
    UiDrawBox(kProbeX, kProbeY, kProbeW, kProbeH, title, size);
    EndCanvas();
    image = LoadImageFromTexture(GetCanvasTexture());
    withTriple = CountInkPixels(image, kProbeX, kProbeY, kProbeW, kProbeH,
                                gUiStyle.titleText);
    UnloadImage(image);

    TestCheckInt(withoutBorder > 0 ? 1 : 0, 1,
                 "the title has ink without a border (otherwise the equality "
                 "below is vacuous)");
    TestCheckInt(
        withTriple, withoutBorder,
        "title ink pixel count under a triple border equals the none case");
  }
  TestEndCase();
}

// 内缩量正确：三色边框的 ring0 占据框顶三行，这三行内不允许出现标题字色
// 或按钮字色的像素。文字画在边框之后，一旦内缩量算错就是反过来把 ring0
// 啃出缺口。
static void CaseRing0HasNoTextPixels(void) {
  TestBeginCase(
      "correct inset: no title or button text color pixels inside the three "
      "rows of triple ring0");
  ApplyProbeStyle();
  gUiStyle.borderStyle = UI_BORDER_TRIPLE;

  const char* title = GetString(kTitleKey);
  const char* label = GetString(kButtonKey);
  static const int kSizes[] = {12, 24, 36};
  int rings = RingCountOf(UI_BORDER_TRIPLE);
  TestCheckInt(rings, 3, "a three-color border has 3 rings");

  for (int i = 0; i < 3; i++) {
    int size = kSizes[i];
    int buttonHeight = UI_BUTTON_HEIGHT(size);
    int buttonWidth =
        UI_ALIGN12(UiMeasureText(label, size).width + 2 * UI_PADDING);

    BeginCanvas();
    ClearBackground(gUiStyle.desktop);
    UiDrawBox(kProbeX, kProbeY, kProbeW, kProbeH, title, size);
    UiButton(kProbeButtonX, kProbeButtonY, buttonWidth, buttonHeight, label,
             size, false);
    EndCanvas();
    Image image = LoadImageFromTexture(GetCanvasTexture());

    int titleRing = CountInkPixels(image, kProbeX, kProbeY, kProbeW, rings,
                                   gUiStyle.titleText);
    int titleInk = CountInkPixels(image, kProbeX, kProbeY, kProbeW,
                                  UI_TITLEBAR_HEIGHT(size) + UI_PADDING,
                                  gUiStyle.titleText);
    int buttonRing = CountInkPixels(image, kProbeButtonX, kProbeButtonY,
                                    buttonWidth, rings, gUiStyle.buttonText);
    int buttonInk =
        CountInkPixels(image, kProbeButtonX, kProbeButtonY, buttonWidth,
                       buttonHeight, gUiStyle.buttonText);
    UnloadImage(image);

    TestCheckInt(titleInk > 0 ? 1 : 0, 1,
                 "the title has ink (otherwise the 0 below is meaningless)");
    TestCheckInt(buttonInk > 0 ? 1 : 0, 1,
                 "the button has ink (otherwise the 0 below is meaningless)");
    TestCheckInt(titleRing, 0,
                 "no title text color pixels inside title bar ring0");
    TestCheckInt(buttonRing, 0,
                 "no button text color pixels inside button ring0");
  }
  TestEndCase();
}

// 12 号下的已知余量：标题墨迹并集高 11，三色边框吃掉 3 圈后
// 标题栏内区 14 - 3 = 11，余量 0、字顶字底贴边。这是「12 号不建议用
// triple」这个结论的根据，用断言钉住。
static void CaseTripleMarginAtSize12(void) {
  TestBeginCase(
      "inset: at size 12 a three-color border leaves 0 margin, ink touches "
      "both edges");
  ApplyProbeStyle();
  gUiStyle.borderStyle = UI_BORDER_TRIPLE;

  const char* title = GetString(kTitleKey);
  static const int kSizes[] = {12, 24, 36};
  int rings = RingCountOf(UI_BORDER_TRIPLE);

  for (int i = 0; i < 3; i++) {
    int size = kSizes[i];
    int barHeight = UI_TITLEBAR_HEIGHT(size);
    BeginCanvas();
    ClearBackground(gUiStyle.desktop);
    UiDrawBox(kProbeX, kProbeY, kProbeW, kProbeH, title, size);
    EndCanvas();
    Image image = LoadImageFromTexture(GetCanvasTexture());
    // 扫描窗口刻意比标题栏高：内缩量算错时标题会掉到标题栏之外，
    // 窗口太窄就会把墨迹截断，量出一个偏小的墨迹高而掩盖真正的偏移。
    InkScan scan = ScanInk(image, kProbeX + UI_PADDING, kProbeY, kProbeW,
                           barHeight + UI_PADDING, gUiStyle.titleText);
    UnloadImage(image);

    ReportRunCount("title ink inside the title bar", scan);
    TestCheckInt(scan.runCount, 1,
                 "exactly one title ink run inside the title bar");
    if (scan.runCount != 1) {
      TestFail(
          "could not locate the single title ink run, cannot verify the inset");
      continue;
    }

    int inkHeight = scan.runBottom[0] - scan.runTop[0] + 1;
    int leftOver = barHeight - rings - inkHeight;

    if (size == 12) {
      TestCheckInt(inkHeight, kTitleInkHeight12,
                   "title ink union height at size 12 is 11px");
      TestCheckInt(scan.runTop[0] - kProbeY, kTitleInkTop12,
                   "title ink top at size 12 lands 3px below the frame top "
                   "(flush inside ring0)");
      TestCheckInt(scan.runBottom[0] - kProbeY, kTitleInkBottom12,
                   "title ink bottom at size 12 lands 13px below the frame top "
                   "(flush with the title bar bottom)");
      TestCheckInt(leftOver, 0, "top and bottom margins are both 0 at size 12");
    } else {
      int expected = (size == 24) ? 1 : 2;
      TestCheckInt(leftOver, expected,
                   "the remaining padding for a three-color border matches the "
                   "README measurement at this size");
    }
    TestCheck(scan.runTop[0] - kProbeY >= rings,
              "the title ink top does not fall into ring0");
  }
  TestEndCase();
}

// 按钮文字的垂直居中与「边框不吃按钮文字」。前四条用例只钉了标题：
// 按钮的内缩量算错时，墨迹只是整体下移——既不出现在 ring0 里，像素数
// 也不变，于是全都放过。这里按墨迹在三色边框内区的上下余量是否对称来
// 判，把按钮的居中公式也钉住。
static void CaseButtonInkCentered(void) {
  TestBeginCase(
      "button text: ink pixel counts match under triple and none, and it is "
      "centered in the inner area");
  const char* label = GetString(kButtonKey);
  static const int kSizes[] = {12, 24, 36};
  int rings = RingCountOf(UI_BORDER_TRIPLE);

  for (int i = 0; i < 3; i++) {
    int size = kSizes[i];
    int buttonHeight = UI_BUTTON_HEIGHT(size);
    int buttonWidth =
        UI_ALIGN12(UiMeasureText(label, size).width + 2 * UI_PADDING);

    InkScan scans[2];
    for (int t = 0; t < 2; t++) {
      ApplyProbeStyle();
      gUiStyle.borderStyle = (t == 0) ? UI_BORDER_NONE : UI_BORDER_TRIPLE;
      BeginCanvas();
      ClearBackground(gUiStyle.desktop);
      UiButton(kProbeButtonX, kProbeButtonY, buttonWidth, buttonHeight, label,
               size, false);
      EndCanvas();
      Image image = LoadImageFromTexture(GetCanvasTexture());
      // 扫描窗口比按钮高：居中算错时墨迹会跑到按钮之外，窗口太窄就量不到
      scans[t] = ScanInk(image, kProbeButtonX, kProbeButtonY, buttonWidth,
                         buttonHeight + UI_PADDING, gUiStyle.buttonText);
      UnloadImage(image);
    }

    ReportRunCount("button text ink (none)", scans[0]);
    ReportRunCount("button text ink (triple)", scans[1]);
    TestCheckInt(scans[0].pixelCount > 0 ? 1 : 0, 1,
                 "the button has ink (otherwise the 0 below is meaningless)");
    TestCheckInt(scans[1].runCount, 1,
                 "exactly one text ink run inside the button");
    TestCheckInt(
        scans[1].pixelCount, scans[0].pixelCount,
        "button ink pixel count under a triple border equals the none case");
    if (scans[1].runCount != 1) continue;

    // 三色边框的内区是 [按钮顶 + 圈数, 按钮底 - 圈数)，居中要求上下余量对称
    int topMargin = scans[1].runTop[0] - (kProbeButtonY + rings);
    int bottomMargin =
        (kProbeButtonY + buttonHeight) - (scans[1].runBottom[0] + 1);
    int gap = (topMargin > bottomMargin) ? topMargin - bottomMargin
                                         : bottomMargin - topMargin;
    char what[160];
    snprintf(what, sizeof(what),
             "button text at size %d is centered in the border inner area "
             "(top margin %d, bottom margin %d)",
             size, topMargin, bottomMargin);
    TestCheckInt(gap <= 1 ? 1 : 0, 1, what);
    TestCheck(scans[1].runTop[0] - kProbeButtonY >= rings,
              "the button text top does not fall into ring0");
  }
  TestEndCase();
}

// 主体块居中：带下伸部的拉丁文案按「到基线为止的主体块」垂直居中，而不是
// 按整串墨迹盒。英文标题 Message Box 曾偏上一个像素方块（主体 2 行黑在
// 上、6 行在下），根因就是下伸部把墨迹盒往下撑长。这里既测带下伸部的
// 拉丁文案，也测没有下伸部的文案，确认新规则没有把后者推偏。
// 拉丁测试文案用 C 字面量：ASCII 永远在图集里，不必走数据文件。
// 用 Message Box 而不是 g、y 之类的短串：峰值取自主体块最密的一行，短串
// 里下伸笔画占一半，峰值低而下伸部够宽，行像素数掉不到峰值一半以下，主体
// 块就分不出来。
static void CaseBodyBlockCentered(void) {
  TestBeginCase(
      "body block: latin text with descenders and text without descenders are "
      "both centered by the body block instead of the whole ink box");
  ApplyProbeStyle();
  gUiStyle.borderStyle = UI_BORDER_TRIPLE;
  const int size = 24;
  int rings = RingCountOf(UI_BORDER_TRIPLE);

  BodyProbe titleDescender = ProbeTitleBody("Message Box", size, rings);
  ReportBody("title \"Message Box\" (has descenders)", titleDescender);
  CheckBodyCentered("title \"Message Box\" (has descenders)", titleDescender);
  // 主体块必须真的短于整条墨迹，否则上面那条判据量的还是墨迹盒，等于把
  // 「按墨迹盒居中」这个 bug 本身当成正确答案放过去。
  TestCheck(titleDescender.bodyBottom < titleDescender.profile.lastInked,
            "the peak-half rule drops the descender: the body block ends above "
            "the last ink row");

  BodyProbe buttonDescender = ProbeButtonBody("Message Box", size, rings);
  ReportBody("button \"Message Box\" (has descenders)", buttonDescender);
  CheckBodyCentered("button \"Message Box\" (has descenders)", buttonDescender);
  TestCheck(buttonDescender.bodyBottom < buttonDescender.profile.lastInked,
            "the peak-half rule drops the descender in the button too");

  const char* noDescender = GetString(kTitleKey);
  TestCheck(strcmp(noDescender, kTitleKey) != 0,
            "the no-descender string comes from the data file, not a fallback "
            "to the key itself");
  BodyProbe titlePlain = ProbeTitleBody(noDescender, size, rings);
  ReportBody("title from the data file (no descenders)", titlePlain);
  CheckBodyCentered("title from the data file (no descenders)", titlePlain);

  // 没有下伸部时主体块就是整条墨迹，两条余量应当各自对称。
  TestCheckInt(titlePlain.bodyTop, titlePlain.profile.firstInked,
               "without descenders the body block starts at the first ink row");
  TestCheckInt(titlePlain.bodyBottom, titlePlain.profile.lastInked,
               "without descenders the body block ends at the last ink row");
  TestEndCase();
}

// === 批次 A1：槽位 / 点阵数字 / 网格 / 截断 / 图鉴场景 ===

// 槽位探针用一套两两不同的颜色，与桌面黑底、品阶色都分得开。
static void ApplySlotProbeStyle(void) {
  ApplyProbeStyle();
  gUiStyle.slotBg = (Color){0x12, 0x34, 0x56, 0xff};
  gUiStyle.slotDivider = (Color){0x65, 0x43, 0x21, 0xff};
  gUiStyle.rarityCommon = (Color){0x80, 0x80, 0x80, 0xff};
  gUiStyle.rarityUncommon = (Color){0x37, 0xa0, 0x30, 0xff};
  gUiStyle.rarityRare = (Color){0x6b, 0x7f, 0xff, 0xff};
  gUiStyle.rarityEpic = (Color){0xd8, 0x69, 0xd0, 0xff};
  gUiStyle.rarityLegendary = (Color){0xf8, 0xad, 0x00, 0xff};
  gUiStyle.rarityMythic = (Color){0xff, 0x35, 0x00, 0xff};
}

// 6×10 字形的独立副本。测试只认渲染回读的像素，不引用 ui_slot.c 的
// 私有表：若绘制路径与这张表不一致，逐位比对立刻失败。
static const uint64_t kExpectedDigits6x10[10] = {
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

// 槽位几何：52×52 足迹、2px 品阶边框、槽位底色、1px 共用分割线。
// 同时构造一个越界品阶，确认颜色被钳制且告警只出现一次。
static void CaseSlotGeometry(void) {
  TestBeginCase(
      "slot geometry: 52x52 footprint, rarity border, slot background, 1px "
      "divider, and a clamped out-of-range rarity warning once");
  ApplySlotProbeStyle();

  const Color rare = gUiStyle.rarityRare;
  const Color bg = gUiStyle.slotBg;
  const Color divider = gUiStyle.slotDivider;

  UiSlotItem item = (UiSlotItem){.icon = (Texture2D){0}, .rarity = 2,
                                 .stack = 0};
  BeginCanvas();
  ClearBackground(BLACK);
  UiDrawSlot(0, 0, &item);
  EndCanvas();
  Image image = LoadImageFromTexture(GetCanvasTexture());

  TestCheckColor(PixelAt(image, 0, 0), rare,
                 "top-left corner is the rarity color");
  TestCheckColor(PixelAt(image, 51, 51), rare,
                 "bottom-right corner is the rarity color");
  TestCheckColor(PixelAt(image, 0, 51), rare,
                 "bottom-left corner is the rarity color");
  TestCheckColor(PixelAt(image, 51, 0), rare,
                 "top-right corner is the rarity color");
  TestCheckColor(PixelAt(image, 2, 2), bg,
                 "(2,2) is the slot background");
  TestCheckColor(PixelAt(image, 49, 49), bg,
                 "(49,49) is the slot background");
  TestCheckInt(CountInkPixels(image, 4, 4, 44, 44, rare), 0,
               "the inner 44x44 area has no rarity color");
  TestCheck(!IsInk(PixelAt(image, 52, 0), rare),
            "the slot footprint ends before x=52");
  UnloadImage(image);

  // 两个槽位由网格负责共用分割线：x=52 整列都应是 slotDivider。
  UiSlotItem pair[2] = {
      (UiSlotItem){.icon = (Texture2D){0}, .rarity = 2, .stack = 0},
      (UiSlotItem){.icon = (Texture2D){0}, .rarity = 2, .stack = 0},
  };
  BeginCanvas();
  ClearBackground(BLACK);
  UiDrawSlotGrid(0, 0, 2, 1, pair, 2);
  EndCanvas();
  image = LoadImageFromTexture(GetCanvasTexture());
  int dividerPixels = 0;
  for (int row = 0; row < UI_SLOT_SIZE; row++) {
    if (IsInk(PixelAt(image, UI_SLOT_SIZE, row), divider)) dividerPixels++;
  }
  TestCheckInt(dividerPixels, UI_SLOT_SIZE,
               "x=52 is a full-height 1px slotDivider column");
  TestCheckColor(PixelAt(image, 0, 0), rare,
                 "the first grid cell keeps its rarity frame");
  TestCheckColor(PixelAt(image, UI_SLOT_PITCH, 0), rare,
                 "the second grid cell starts one pitch to the right");
  UnloadImage(image);

  // 越界品阶：颜色钳到 5，静态标志保证告警只出现一次。
  TestLogReset();
  UiSlotItem bad = (UiSlotItem){.icon = (Texture2D){0}, .rarity = 7,
                                .stack = 0};
  BeginCanvas();
  ClearBackground(BLACK);
  UiDrawSlot(0, 0, &bad);
  UiDrawSlot(0, 0, &bad);
  EndCanvas();
  image = LoadImageFromTexture(GetCanvasTexture());
  TestCheckColor(PixelAt(image, 0, 0), gUiStyle.rarityMythic,
                 "out-of-range rarity 7 is clamped to the mythic frame");
  UnloadImage(image);
  TestCheckInt(TestLogWarnCount(), 1,
               "an out-of-range rarity warns only once across two draws");
  TestCheckColor(UiRarityColor(-3), gUiStyle.rarityCommon,
                 "UiRarityColor clamps a negative rarity to common");
  TestCheckColor(UiRarityColor(99), gUiStyle.rarityMythic,
                 "UiRarityColor clamps an oversized rarity to mythic");
  TestEndCase();
}

// 点阵数字：每个数字单独画在干净画布上，6×10 区域逐位与字形表比对。
// 这个用例补的是 CaseSlotGeometry 摸不到的一条路径：那边用 icon.id == 0
// 的空图标，于是「图标画在哪个偏移」根本没被验证。历史上正是这里多缩了
// 1px，48×48 的图标顶掉右/下边框各 1px，而所有断言照样通过。
static void CaseSlotWithOpaqueIcon(void) {
  TestBeginCase(
      "slot with an opaque 48x48 icon: all four 2px borders stay intact and "
      "the icon covers exactly the inner area");
  ApplySlotProbeStyle();

  const Color rare = gUiStyle.rarityRare;
  const Color iconInk = MAGENTA;

  Image raw = GenImageColor(UI_SLOT_ICON, UI_SLOT_ICON, iconInk);
  Texture2D texture = LoadTextureFromImage(raw);
  UnloadImage(raw);
  TestCheckInt(texture.id != 0 ? 1 : 0, 1,
               "the probe icon texture was created");

  UiSlotItem item = (UiSlotItem){.icon = texture, .rarity = 2, .stack = 0};
  BeginCanvas();
  ClearBackground(BLACK);
  UiDrawSlot(0, 0, &item);
  EndCanvas();
  Image image = LoadImageFromTexture(GetCanvasTexture());
  UnloadTexture(texture);

  TestCheckColor(PixelAt(image, 10, 0), rare, "top border row 0 is rarity");
  TestCheckColor(PixelAt(image, 10, 1), rare, "top border row 1 is rarity");
  TestCheckColor(PixelAt(image, 10, 50), rare, "bottom border row 50 is rarity");
  TestCheckColor(PixelAt(image, 10, 51), rare, "bottom border row 51 is rarity");
  TestCheckColor(PixelAt(image, 0, 10), rare, "left border col 0 is rarity");
  TestCheckColor(PixelAt(image, 1, 10), rare, "left border col 1 is rarity");
  TestCheckColor(PixelAt(image, 50, 10), rare, "right border col 50 is rarity");
  TestCheckColor(PixelAt(image, 51, 10), rare, "right border col 51 is rarity");

  TestCheckColor(PixelAt(image, 2, 2), iconInk, "inner area starts at (2,2)");
  TestCheckColor(PixelAt(image, 49, 49), iconInk, "inner area ends at (49,49)");
  TestCheckInt(CountInkPixels(image, 2, 2, UI_SLOT_ICON, UI_SLOT_ICON, iconInk),
               UI_SLOT_ICON * UI_SLOT_ICON,
               "the whole 48x48 inner area is covered by the icon");
  UnloadImage(image);
  TestEndCase();
}

static void CaseBitmapDigits(void) {
  TestBeginCase(
      "bitmap digits: every 6x10 glyph pixel matches the mask bit for bit");
  ApplySlotProbeStyle();

  const Color ink = WHITE;
  for (int digit = 0; digit < 10; digit++) {
    BeginCanvas();
    ClearBackground(BLACK);
    UiDrawNumber(0, 0, digit, 1, ink);
    EndCanvas();
    Image image = LoadImageFromTexture(GetCanvasTexture());

    int mismatches = 0;
    int litPixels = 0;
    for (int row = 0; row < UI_DIGIT_H; row++) {
      for (int column = 0; column < UI_DIGIT_W; column++) {
        int bit = row * UI_DIGIT_W + column;
        bool expected =
            (kExpectedDigits6x10[digit] & (1ULL << bit)) != 0;
        bool actual = IsInk(PixelAt(image, column, row), ink);
        if (expected) litPixels++;
        if (expected != actual) mismatches++;
      }
    }
    UnloadImage(image);

    char what[160];
    snprintf(what, sizeof(what),
             "digit %d matches all 60 mask positions", digit);
    TestCheckInt(mismatches, 0, what);
    snprintf(what, sizeof(what), "digit %d has at least one lit pixel", digit);
    TestCheck(litPixels > 0, what);
  }

  TestCheckInt(UiNumberWidth(1), 6, "UiNumberWidth(1)");
  TestCheckInt(UiNumberWidth(2), 13, "UiNumberWidth(2)");
  TestCheckInt(UiNumberWidth(0), 0, "UiNumberWidth(0)");
  TestCheckInt(UiNumberWidth(-3), 0, "UiNumberWidth(-3)");
  TestEndCase();
}

// 网格数学：5 列 × 2 行、原点 (12,152)，节距 53。
static void CaseGridMath(void) {
  TestBeginCase(
      "grid math: cell positions at pitch 53, centers hit their own cell, "
      "dividers and outside points return -1");
  ApplySlotProbeStyle();

  static const int kIndices[4] = {0, 4, 5, 9};
  static const int kExpectedX[4] = {12, 224, 12, 224};
  static const int kExpectedY[4] = {152, 152, 205, 205};

  for (int i = 0; i < 4; i++) {
    UiPoint cell = UiGridCellPos(12, 152, 5, kIndices[i]);
    char what[96];
    snprintf(what, sizeof(what), "cell %d x is %d", kIndices[i],
             kExpectedX[i]);
    TestCheckInt(cell.x, kExpectedX[i], what);
    snprintf(what, sizeof(what), "cell %d y is %d", kIndices[i],
             kExpectedY[i]);
    TestCheckInt(cell.y, kExpectedY[i], what);
  }

  for (int i = 0; i < 10; i++) {
    UiPoint cell = UiGridCellPos(12, 152, 5, i);
    int hit = UiGridIndexAt(12, 152, 5, 2, cell.x + 26, cell.y + 26);
    char what[96];
    snprintf(what, sizeof(what), "the center of cell %d maps back to %d", i, i);
    TestCheckInt(hit, i, what);
  }

  TestCheckInt(UiGridIndexAt(12, 152, 5, 2, 64, 178), -1,
               "the vertical divider at x=64 is not a cell");
  TestCheckInt(UiGridIndexAt(12, 152, 5, 2, 38, 204), -1,
               "the horizontal divider at y=204 is not a cell");
  TestCheckInt(UiGridIndexAt(12, 152, 5, 2, 11, 152), -1,
               "a point left of the grid returns -1");
  TestCheckInt(UiGridIndexAt(12, 152, 5, 2, 276, 152), -1,
               "a point right of the grid returns -1");
  TestCheckInt(UiGridIndexAt(12, 152, 5, 2, 12, 257), -1,
               "a point below the grid returns -1");
  TestCheckInt(UiGridIndexAt(12, 152, 0, 2, 38, 178), -1,
               "columns <= 0 returns -1");
  TestCheckInt(UiGridIndexAt(12, 152, 5, 0, 38, 178), -1,
               "rows <= 0 returns -1");
  TestEndCase();
}

// 文本截断：按码点计数，不切开 UTF-8 序列，容量不足时保留完整码点。
static void CaseTruncateUtf8(void) {
  TestBeginCase(
      "UTF-8 truncation: code point counting, capacity handling, empty and "
      "NULL inputs, and invalid-sequence stop");
  char buffer[64];

  int written = UiTruncateUtf8("abcdefghij", 4, buffer, (int)sizeof(buffer));
  TestCheckInt(written, 4, "ASCII cut to 4 returns 4 code points");
  TestCheckStr(buffer, "abcd", "ASCII cut to 4 keeps abcd");

  // 15 个汉字，每个 3 字节；截到 10 个码点应留下 30 字节。
  const char* fifteen = "一二三四五六七八九十甲乙丙丁戊";
  written = UiTruncateUtf8(fifteen, 10, buffer, (int)sizeof(buffer));
  TestCheckInt(written, 10, "15 CJK code points cut to 10 returns 10");
  TestCheckInt((int)strlen(buffer), 30,
               "10 CJK code points occupy 30 UTF-8 bytes");
  TestCheckInt(GetCodepointCount(buffer), 10,
               "GetCodepointCount agrees the result has 10 code points");

  // 5 字节只能放一个 3 字节汉字加结尾的 '\0'。
  written = UiTruncateUtf8("汉字", 10, buffer, 5);
  TestCheckInt(written, 1, "a 5-byte buffer holds exactly one CJK code point");
  TestCheckStr(buffer, "汉", "the first complete CJK code point is kept");
  TestCheckInt((int)strlen(buffer), 3, "one CJK code point is 3 bytes long");

  buffer[0] = 'z';
  written = UiTruncateUtf8("abc", 0, buffer, (int)sizeof(buffer));
  TestCheckInt(written, 0, "maxCodepoints 0 returns 0");
  TestCheckInt(buffer[0], '\0', "maxCodepoints 0 writes an empty string");

  buffer[0] = 'x';
  written = UiTruncateUtf8(NULL, 4, buffer, (int)sizeof(buffer));
  TestCheckInt(written, 0, "a NULL source returns 0");
  TestCheckInt(buffer[0], 'x', "a NULL source writes no memory");

  buffer[0] = 'y';
  written = UiTruncateUtf8("abc", 4, buffer, 0);
  TestCheckInt(written, 0, "dstCapacity 0 returns 0");
  TestCheckInt(buffer[0], 'y', "dstCapacity 0 writes no memory");

  // 非法序列：'a' 之后是一个不完整的三字节序列，必须停在那里。
  static const char kInvalid[] = {'a', (char)0xE4, (char)0xB8, 'z', '\0'};
  written = UiTruncateUtf8(kInvalid, 10, buffer, (int)sizeof(buffer));
  TestCheckInt(written, 1, "an invalid UTF-8 sequence stops truncation");
  TestCheckStr(buffer, "a", "the bytes before the invalid sequence survive");
  TestEndCase();
}

// 图鉴场景出图：切到 ui_gallery 画三帧，回读画布确认六种品阶色与
// slotDivider 都真的出现在画面上。
static void CaseGallerySceneRenders(void) {
  TestBeginCase(
      "ui_gallery scene: three frames render all six rarity colors and at "
      "least one slotDivider pixel");
  gUiStyle = kDefaultUiStyle;

  InitSceneManager();
  SceneManager_RequestSceneSwitch(&UiGalleryScene);
  SceneManager_ProcessPendingSwitch();

  for (int frame = 0; frame < 3; frame++) {
    SceneManager_UpdateCurrentScene();
    BeginCanvas();
    ClearBackground(gUiStyle.desktop);
    SceneManager_DrawCurrentScene();
    EndCanvas();
  }

  Image image = LoadImageFromTexture(GetCanvasTexture());
  int common = CountInkPixels(image, 0, 0, GRAPH_DESIGN_WIDTH,
                              GRAPH_DESIGN_HEIGHT, gUiStyle.rarityCommon);
  int uncommon = CountInkPixels(image, 0, 0, GRAPH_DESIGN_WIDTH,
                                GRAPH_DESIGN_HEIGHT, gUiStyle.rarityUncommon);
  int rare = CountInkPixels(image, 0, 0, GRAPH_DESIGN_WIDTH, GRAPH_DESIGN_HEIGHT,
                            gUiStyle.rarityRare);
  int epic = CountInkPixels(image, 0, 0, GRAPH_DESIGN_WIDTH, GRAPH_DESIGN_HEIGHT,
                            gUiStyle.rarityEpic);
  int legendary = CountInkPixels(image, 0, 0, GRAPH_DESIGN_WIDTH,
                                 GRAPH_DESIGN_HEIGHT, gUiStyle.rarityLegendary);
  int mythic = CountInkPixels(image, 0, 0, GRAPH_DESIGN_WIDTH,
                              GRAPH_DESIGN_HEIGHT, gUiStyle.rarityMythic);
  int divider = CountInkPixels(image, 0, 0, GRAPH_DESIGN_WIDTH,
                               GRAPH_DESIGN_HEIGHT, gUiStyle.slotDivider);
  UnloadImage(image);

  TestCheck(common > 0, "rarity 0 (common) appears in the gallery");
  TestCheck(uncommon > 0, "rarity 1 (uncommon) appears in the gallery");
  TestCheck(rare > 0, "rarity 2 (rare) appears in the gallery");
  TestCheck(epic > 0, "rarity 3 (epic) appears in the gallery");
  TestCheck(legendary > 0, "rarity 4 (legendary) appears in the gallery");
  TestCheck(mythic > 0, "rarity 5 (mythic) appears in the gallery");
  TestCheck(divider > 0, "slotDivider appears in the gallery grid");

  SceneManager_UnloadCurrentScene();
  TestEndCase();
}

// === 批次 A2：页签栏 / 滚动条 / 状态条 / 页面容器 ===

// 页签命中与页签栏钳制：四个边界点、越界点、tabCount 的上限与下限，
// 以及「超出 9 项只告警一次」。这条用例必须排在任何 tabCount > 9 的
// 绘制之前，否则静态告警标志已经置位，计数断言会永远得到 0。
static void CasePageTabHit(void) {
  TestBeginCase(
      "page tabs: horizontal hit testing on both edges, count clamping, and a "
      "single clamp warning");
  ApplyProbeStyle();

  TestCheckInt(UiPageTabWidth(6), 106, "six tabs are 106 px wide");
  TestCheckInt(UiPageTabWidth(9), 71, "nine tabs are 71 px wide");
  TestCheckInt(UiPageTabWidth(0), 0, "tabCount 0 has no width");

  TestCheckInt(UiPageTabAt(0, 0, 6), 0, "(0,0) hits tab 0");
  TestCheckInt(UiPageTabAt(105, 15, 6), 0, "(105,15) is still tab 0");
  TestCheckInt(UiPageTabAt(106, 0, 6), 1, "(106,0) starts tab 1");
  TestCheckInt(UiPageTabAt(639, 15, 6), 5,
               "the last tab absorbs the 4 px remainder");
  TestCheckInt(UiPageTabAt(0, UI_PAGE_TAB_BAR_HEIGHT, 6), -1,
               "y equal to the bar height is content, not a tab");
  TestCheckInt(UiPageTabAt(-1, 0, 6), -1, "a negative x returns -1");
  TestCheckInt(UiPageTabAt(640, 0, 6), -1, "x=640 is outside the canvas");
  TestCheckInt(UiPageTabAt(0, 0, 0), -1, "tabCount 0 returns -1");
  TestCheckInt(UiPageTabAt(568, 8, 12), 8,
               "tabCount 12 clamps to 9: 640/9 = 71, tab 8 starts at 568");
  TestCheckInt(UiPageTabAt(639, 8, 12), 8,
               "the clamped ninth tab reaches the right edge");

  static const char* const kLabels[6] = {"A", "B", "C", "D", "E", "F"};
  BeginCanvas();
  ClearBackground(gUiStyle.desktop);
  UiDrawPageTabs(99, 6, kLabels);
  EndCanvas();
  Image image = LoadImageFromTexture(GetCanvasTexture());
  TestCheckColor(PixelAt(image, 600, 8), gUiStyle.button,
                 "selectedTab 99 clamps to the last tab");
  TestCheckColor(PixelAt(image, 8, 8), gUiStyle.background,
                 "the first tab stays unselected");
  UnloadImage(image);

  static const char* const kManyLabels[12] = {"A", "B", "C", "D", "E", "F",
                                               "G", "H", "I", "J", "K", "L"};
  TestLogReset();
  BeginCanvas();
  ClearBackground(gUiStyle.desktop);
  UiDrawPageTabs(0, 12, kManyLabels);
  UiDrawPageTabs(0, 12, kManyLabels);
  EndCanvas();
  image = LoadImageFromTexture(GetCanvasTexture());
  TestCheckColor(PixelAt(image, 600, 8), gUiStyle.background,
                 "the ninth clamped tab is drawn at the right edge");
  TestCheckColor(PixelAt(image, 600, UI_PAGE_TAB_BAR_HEIGHT), gUiStyle.desktop,
                 "nothing is painted below the tab bar");
  UnloadImage(image);
  TestCheckInt(TestLogWarnCount(), 1,
               "an oversized tab count warns only once across two draws");
  TestCheck(TestLogHasWarn("UiPage: tab count"),
            "the tab clamp warning names the module and the tab count");
  TestEndCase();
}

// 滑块数学：内容不足一屏时不画；高度按下限钳制；firstVisible 取 0、
// 中间、最大值时位置单调递增，且最大值处滑块底边正好贴住轨道底边。
static void CaseScrollbarThumbMath(void) {
  TestBeginCase(
      "scrollbar thumb math: hidden when content fits, 6px minimum, and a "
      "monotonic position that ends flush with the track bottom");
  TestCheckInt(UiScrollbarThumbHeight(100, 10, 10), 0,
               "content equal to the visible count hides the thumb");
  TestCheckInt(UiScrollbarThumbHeight(100, 5, 10), 0,
               "content smaller than the visible count hides the thumb");
  TestCheckInt(UiScrollbarThumbY(100, 0, 10, 10, 0), 0,
               "the thumb position is 0 when the thumb is hidden");
  TestCheckInt(UiScrollbarThumbHeight(100, 100, 10), 10,
               "a tenth of 100 rows gives a 10px thumb");
  TestCheckInt(UiScrollbarThumbHeight(100, 1000, 10), 6,
               "the thumb is clamped to the 6px minimum");

  int height = UiScrollbarThumbHeight(100, 1000, 10);
  int yTop = UiScrollbarThumbY(100, height, 1000, 10, 0);
  int yMiddle = UiScrollbarThumbY(100, height, 1000, 10, 495);
  int yBottom = UiScrollbarThumbY(100, height, 1000, 10, 990);
  TestCheckInt(yTop, 0, "firstVisible 0 puts the thumb at the track top");
  TestCheck(yMiddle > yTop && yMiddle < yBottom,
            "the middle firstVisible puts the thumb strictly between the ends");
  TestCheckInt(yBottom + height, 100,
               "the last firstVisible puts the thumb bottom flush with the "
               "track bottom");
  TestCheckInt(UiScrollbarThumbY(100, height, 1000, 10, 99999), yBottom,
               "firstVisible beyond the maximum is clamped");
  TestCheckInt(UiScrollbarThumbY(100, height, 1000, 10, -5), 0,
               "a negative firstVisible is clamped to 0");

  TestCheckInt(UiScrollbarThumbHeight(0, 10, 5), 0,
               "trackHeight 0 returns a 0 thumb height");
  TestCheckInt(UiScrollbarThumbHeight(100, 0, 5), 0,
               "contentCount 0 returns a 0 thumb height");
  TestCheckInt(UiScrollbarThumbHeight(100, 10, 0), 0,
               "visibleCount 0 returns a 0 thumb height");
  TestEndCase();
}

// 滑块形状：100 行轨道、内容 20 可见 10，滑块高 50。首尾两行各 1px、
// 次首尾两行 3px、其余直段 5px，且宽度居中对齐；轨道其余像素是底色。
static void CaseScrollbarThumbShape(void) {
  TestBeginCase(
      "scrollbar thumb shape: 1/3/5 tapered ends, a 5px straight middle, and "
      "no thumb when the content fits");
  ApplyProbeStyle();

  const int trackHeight = 100;
  const int contentCount = 20;
  const int visibleCount = 10;
  const int thumbHeight =
      UiScrollbarThumbHeight(trackHeight, contentCount, visibleCount);
  TestCheckInt(thumbHeight, 50, "the probe thumb is 50 rows tall");

  BeginCanvas();
  ClearBackground(gUiStyle.desktop);
  UiDrawScrollbar(0, 0, trackHeight, contentCount, visibleCount, 0);
  EndCanvas();
  Image image = LoadImageFromTexture(GetCanvasTexture());

  TestCheckInt(CountInkPixels(image, 0, 0, UI_SCROLLBAR_WIDTH, 1,
                              gUiStyle.titleText),
               1, "the first thumb row is exactly one pixel wide");
  TestCheckInt(FirstInkX(image, 0, 0, UI_SCROLLBAR_WIDTH, gUiStyle.titleText),
               2, "the first row's single pixel is centered at x=2");
  TestCheckInt(CountInkPixels(image, 0, 1, UI_SCROLLBAR_WIDTH, 1,
                              gUiStyle.titleText),
               3, "the second thumb row is three pixels wide");
  TestCheckInt(FirstInkX(image, 0, 1, UI_SCROLLBAR_WIDTH, gUiStyle.titleText),
               1, "the second row starts at x=1");
  TestCheckInt(CountInkPixels(image, 0, 2, UI_SCROLLBAR_WIDTH, 1,
                              gUiStyle.titleText),
               5, "the third thumb row is five pixels wide");
  TestCheckInt(FirstInkX(image, 0, 2, UI_SCROLLBAR_WIDTH, gUiStyle.titleText),
               0, "the five-pixel rows start at x=0");
  TestCheckInt(CountInkPixels(image, 0, 25, UI_SCROLLBAR_WIDTH, 1,
                              gUiStyle.titleText),
               5, "a straight middle row stays five pixels wide");
  TestCheckInt(CountInkPixels(image, 0, 47, UI_SCROLLBAR_WIDTH, 1,
                              gUiStyle.titleText),
               5, "the third row from the bottom is five pixels wide");
  TestCheckInt(CountInkPixels(image, 0, 48, UI_SCROLLBAR_WIDTH, 1,
                              gUiStyle.titleText),
               3, "the second row from the bottom is three pixels wide");
  TestCheckInt(FirstInkX(image, 0, 48, UI_SCROLLBAR_WIDTH, gUiStyle.titleText),
               1, "the penultimate row starts at x=1");
  TestCheckInt(CountInkPixels(image, 0, 49, UI_SCROLLBAR_WIDTH, 1,
                              gUiStyle.titleText),
               1, "the last thumb row is exactly one pixel wide");
  TestCheckInt(FirstInkX(image, 0, 49, UI_SCROLLBAR_WIDTH, gUiStyle.titleText),
               2, "the last row's single pixel is centered at x=2");

  int restRows = trackHeight - thumbHeight;
  TestCheckInt(CountInkPixels(image, 0, thumbHeight, UI_SCROLLBAR_WIDTH,
                              restRows, gUiStyle.titleText),
               0, "the rest of the track has no thumb pixels");
  TestCheckInt(CountInkPixels(image, 0, thumbHeight, UI_SCROLLBAR_WIDTH,
                              restRows, gUiStyle.background),
               UI_SCROLLBAR_WIDTH * restRows,
               "the rest of the track is background color");
  UnloadImage(image);

  // 内容正好一屏：只画轨道，不画滑块。
  BeginCanvas();
  ClearBackground(gUiStyle.desktop);
  UiDrawScrollbar(0, 0, 50, 5, 5, 0);
  EndCanvas();
  image = LoadImageFromTexture(GetCanvasTexture());
  TestCheckInt(CountInkPixels(image, 0, 0, UI_SCROLLBAR_WIDTH, 50,
                              gUiStyle.titleText),
               0, "content that fits draws no thumb at all");
  TestCheckInt(CountInkPixels(image, 0, 0, UI_SCROLLBAR_WIDTH, 50,
                              gUiStyle.background),
               UI_SCROLLBAR_WIDTH * 50, "the track is still painted");
  UnloadImage(image);
  TestEndCase();
}

// 状态条：填充宽度公式（整数除法四舍五入）、越界钳制，以及 100 宽、
// 5/10 的条在一条内行上正好 49 个填充像素。
static void CaseBarFill(void) {
  TestBeginCase(
      "bar fill: rounded fill width math, a clamped oversized value, and a "
      "100px bar at 5/10 painting 49 line pixels per row");
  TestCheckInt(UiBarFillWidth(100, 0, 10), 0, "value 0 gives an empty fill");
  TestCheckInt(UiBarFillWidth(100, 5, 10), 49,
               "half of a 100px bar is 49px after the 1px borders");
  TestCheckInt(UiBarFillWidth(100, 10, 10), 98,
               "value equal to max fills the whole inner width");
  TestCheckInt(UiBarFillWidth(100, 11, 10), 98,
               "value above max is clamped to the inner width");
  TestCheckInt(UiBarFillWidth(100, 5, 0), 0, "max 0 gives an empty fill");
  TestCheckInt(UiBarFillWidth(12, 3, 4), 8,
               "three quarters of a 12px bar rounds to 8px");
  TestCheckInt(UiBarFillWidth(2, 1, 2), 0,
               "a bar narrower than 3px cannot hold the frame");

  ApplySlotProbeStyle();
  BeginCanvas();
  ClearBackground(gUiStyle.desktop);
  UiDrawBar(0, 0, 100, 5, 10);
  EndCanvas();
  Image image = LoadImageFromTexture(GetCanvasTexture());

  TestCheckColor(PixelAt(image, 0, 0), gUiStyle.lineDim,
                 "the outer corner is the lineDim frame");
  TestCheckColor(PixelAt(image, 1, 1), gUiStyle.line,
                 "the first inner pixel is fill");
  TestCheckColor(PixelAt(image, 49, 1), gUiStyle.line,
                 "the 49th inner column is fill");
  TestCheckColor(PixelAt(image, 50, 1), gUiStyle.slotBg,
                 "the 50th inner column is still track");
  TestCheckInt(CountInkPixels(image, 1, 1, 98, 1, gUiStyle.line), 49,
               "an inner row has exactly 49 fill pixels");
  TestCheckInt(CountInkPixels(image, 1, 1, 98, UI_BAR_HEIGHT - 2,
                              gUiStyle.line),
               49 * (UI_BAR_HEIGHT - 2),
               "the fill area is 49 columns by 10 rows");
  UnloadImage(image);
  TestEndCase();
}

// 页面容器：矩形是 {40,0,600,480}；页号钳到 0..5；六页各画一帧，内容区
// 都有非底色像素；第 1 页出现全部六种品阶色。
static void CasePageContainerRenders(void) {
  TestBeginCase(
      "page container: content rect is 0,16,640,464, every page paints inside "
      "it, page 1 carries all six rarity colors, and the page index clamps");
  gUiStyle = kDefaultUiStyle;

  Rectangle content = UiPageContentRect();
  TestCheckInt((int)content.x, 0, "the content rect starts at x=0");
  TestCheckInt((int)content.y, UI_PAGE_TAB_BAR_HEIGHT,
               "the content rect starts below the tab bar");
  TestCheckInt((int)content.width, 640, "the content rect is the full 640 wide");
  TestCheckInt((int)content.height, 480 - UI_PAGE_TAB_BAR_HEIGHT,
               "the content rect is 480 minus the tab bar tall");

  UiGallerySetPage(-3);
  TestCheckInt(UiGalleryGetPage(), 0, "a negative page clamps to 0");
  UiGallerySetPage(99);
  TestCheckInt(UiGalleryGetPage(), 5, "an oversized page clamps to 5");

  InitSceneManager();
  SceneManager_RequestSceneSwitch(&UiGalleryScene);
  SceneManager_ProcessPendingSwitch();

  for (int page = 0; page < 6; page++) {
    UiGallerySetPage(page);
    BeginCanvas();
    ClearBackground(gUiStyle.desktop);
    SceneManager_DrawCurrentScene();
    EndCanvas();
    Image image = LoadImageFromTexture(GetCanvasTexture());

    int painted = CountDifferentPixels(image, (int)content.x, (int)content.y,
                                       (int)content.width,
                                       (int)content.height,
                                       gUiStyle.background);
    char what[128];
    snprintf(what, sizeof(what),
             "page %d paints non-background pixels inside the content rect",
             page + 1);
    TestCheck(painted > 0, what);

    if (page == 0) {
      TestCheck(CountInkPixels(image, (int)content.x, (int)content.y,
                               (int)content.width, (int)content.height,
                               gUiStyle.rarityCommon) > 0,
                "page 1 shows rarity 0 (common)");
      TestCheck(CountInkPixels(image, (int)content.x, (int)content.y,
                               (int)content.width, (int)content.height,
                               gUiStyle.rarityUncommon) > 0,
                "page 1 shows rarity 1 (uncommon)");
      TestCheck(CountInkPixels(image, (int)content.x, (int)content.y,
                               (int)content.width, (int)content.height,
                               gUiStyle.rarityRare) > 0,
                "page 1 shows rarity 2 (rare)");
      TestCheck(CountInkPixels(image, (int)content.x, (int)content.y,
                               (int)content.width, (int)content.height,
                               gUiStyle.rarityEpic) > 0,
                "page 1 shows rarity 3 (epic)");
      TestCheck(CountInkPixels(image, (int)content.x, (int)content.y,
                               (int)content.width, (int)content.height,
                               gUiStyle.rarityLegendary) > 0,
                "page 1 shows rarity 4 (legendary)");
      TestCheck(CountInkPixels(image, (int)content.x, (int)content.y,
                               (int)content.width, (int)content.height,
                               gUiStyle.rarityMythic) > 0,
                "page 1 shows rarity 5 (mythic)");
    }
    UnloadImage(image);
  }

  SceneManager_UnloadCurrentScene();
  TestEndCase();
}

int main(void) {
  SetTraceLogLevel(LOG_WARNING);

  // 隐藏窗口即可：全部绘制发生在 640*480 的设计画布上，回读的是画布
  // 纹理而不是窗口后备缓冲，因此窗口可见性与 DPI 都不影响结果。
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  InitWindow(GRAPH_DESIGN_WIDTH, GRAPH_DESIGN_HEIGHT,
             "The Law Of Prey UI 测试");
  // 无窗口就无 GL 上下文，后面每次纹理上传都会失败，CloseWindow 还会崩。
  // 直接报一行能看懂的错，而不是给出一段段错误堆栈。
  if (!IsWindowReady()) {
    TraceLog(LOG_ERROR,
             "window or OpenGL context is not available, cannot run the UI "
             "tests (headless session? run under Xvfb: xvfb-run -a "
             "bash scripts/run_tests.sh)");
    return 1;
  }
  TestLogBegin();

  InitAssetsDirectory();
  InitGameFont();
  InitStrings();
  InitTheme();
  InitCanvas();

  TestBegin("UI line step and centering regression tests");
  CaseLineStep();
  CaseBorderDoesNotCoverTitle();
  CaseRing0HasNoTextPixels();
  CaseTripleMarginAtSize12();
  CaseButtonInkCentered();
  CaseBodyBlockCentered();
  CaseSlotGeometry();
  CaseSlotWithOpaqueIcon();
  CaseBitmapDigits();
  CaseGridMath();
  CaseTruncateUtf8();
  CaseGallerySceneRenders();
  CasePageTabHit();
  CaseScrollbarThumbMath();
  CaseScrollbarThumbShape();
  CaseBarFill();
  CasePageContainerRenders();

  int result = TestEnd();

  // 卸载顺序与本体一致：字体、文案表与图形资源都必须在 CloseWindow 之前
  // 释放，此时 GL 上下文尚在。
  UnloadStrings();
  UnloadGameFont();
  UiUnloadStyle();
  UnloadCanvas();
  CloseWindow();
  return result;
}