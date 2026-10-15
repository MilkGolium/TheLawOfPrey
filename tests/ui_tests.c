#include <stdio.h>
#include <string.h>

#include "font.h"
#include "graphsettings.h"
#include "strings.h"
#include "test_util.h"
#include "theme.h"
#include "ui.h"
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

int main(void) {
  SetTraceLogLevel(LOG_WARNING);

  // 隐藏窗口即可：全部绘制发生在 640*480 的设计画布上，回读的是画布
  // 纹理而不是窗口后备缓冲，因此窗口可见性与 DPI 都不影响结果。
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  InitWindow(GRAPH_DESIGN_WIDTH, GRAPH_DESIGN_HEIGHT,
             "The Law Of Prey UI 测试");
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