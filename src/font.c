#include "font.h"

#include <assert.h>
#include <string.h>

#include "lang.h"
#include "utils.h"

// === 配置 ===

#define FONT_PATH "assets/fonts/ttf/fusion-pixel-12px-proportional-zh_hans.ttf"
#define DATABASE_DIR "assets/databases"
#define DEFAULT_FONT_SIZE 24
#define BMP_RANGE 0x10000
#define FONT_SIZE_COUNT 3

// 对外字号，单位为 em 像素，必须是 12 的倍数。
static const int kFontSizes[FONT_SIZE_COUNT] = {12, 24, 36};

// 传给 raylib LoadFontEx 的值。raylib 按 hhea 行高（本字体 asc-desc=1600）
// 而非 upem（1200）缩放字形，故 raylib 字号 = 对外字号 * 4/3。直接传
// 12/24/36 会得到每设计像素 0.75/1.5/2.25 个像素，栅格化落在半像素上，
// 图集带灰边；传 16/32/48 才是每设计像素 1/2/3 个像素，得到纯二值图集。
static const int kFontLoadSizes[FONT_SIZE_COUNT] = {16, 32, 48};

// 收集与检查共同的目标列名：物品表用前三列，界面文案用 text 列。
static const char* const kTargetColumns[] = {"display_name", "description",
                                             "inspect_text", "text"};
#define TARGET_COLUMN_COUNT 4

// BMP 范围内的 codepoint 去重位图（覆盖 CJK 基本区与 ASCII）
static unsigned char cpBitmap[BMP_RANGE / 8];

// 收集到的唯一 codepoint，升序排列
static int* collectedCps = NULL;
static int collectedCount = 0;

// 收集阶段从 CSV 提取出的文本快照。缺字检查复用同一份，
// 不再对磁盘做第二遍扫描。UnloadGameFont 负责释放。
static char** sTexts = NULL;
static int sTextCount = 0;
static int sTextCapacity = 0;

// 超出收集范围的码点数，收集与缺字检查各记一份、不混算
static int sDroppedBeyondRange = 0;
static int sSkippedBeyondRange = 0;

#ifndef NDEBUG
// 收集阶段是否已经就越界丢弃告警过，供不变式断言核对
static bool sDroppedReported = false;
#endif

typedef enum {
  kFontStateUnloaded,  // 尚未尝试加载
  kFontStateReady,     // 加载成功
  kFontStateFailed,    // 加载失败，不重试
} FontState;

// 按尺寸懒加载的字体
static Font sFonts[FONT_SIZE_COUNT];
static FontState sFontStates[FONT_SIZE_COUNT];

// codepoint 未收集完成时的拒绝告警只发一次，否则绘制路径每帧刷屏
static bool sCollectWarned = false;

static bool initialized = false;

// === codepoint 去重 ===

// 收集范围与检查范围共用这一个谓词：两端范围由构造保证一致，
// 不会出现「收得进来却查得出缺字」这类把排查带偏的组合。
static bool IsCollectable(int cp) { return cp >= 0 && cp < BMP_RANGE; }

static void MarkCodepoint(int cp) {
  if (!IsCollectable(cp)) {
    // 越界不静默丢弃：BMP 以上的字形无论怎么补数据都进不了图集，
    // 计数后由收集阶段一次性告警。
    sDroppedBeyondRange++;
    return;
  }
  cpBitmap[cp >> 3] |= (unsigned char)(1u << (cp & 7));
}

static bool IsCodepointMarked(int cp) {
  if (!IsCollectable(cp)) return false;
  return (cpBitmap[cp >> 3] & (1u << (cp & 7))) != 0;
}

static void CollectFromString(const char* text) {
  if (!text) return;
  int count = 0;
  int* cps = LoadCodepoints(text, &count);
  for (int i = 0; i < count; i++) MarkCodepoint(cps[i]);
  UnloadCodepoints(cps);
}

// === CSV 扫描 ===

// 对 CSV 中目标列的每个文本字段调用 callback
typedef void (*TextCallback)(const char* text, void* userData);

typedef struct {
  int targetCols[TARGET_COLUMN_COUNT];  // 目标列在行内的下标，-1 为无
  bool headerParsed;
  TextCallback callback;
  void* userData;
} FileScan;

// 表头行定位目标列，数据行只把目标列交给回调。列名随文件而定，
// 因此定位结果按文件各存一份。
static void OnCsvRow(const char* const* fields, int fieldCount,
                     void* userData) {
  FileScan* scan = (FileScan*)userData;
  if (!scan->headerParsed) {
    for (int c = 0; c < TARGET_COLUMN_COUNT; c++) {
      scan->targetCols[c] = -1;
      for (int i = 0; i < fieldCount; i++) {
        if (strcmp(fields[i], kTargetColumns[c]) == 0) {
          scan->targetCols[c] = i;
          break;
        }
      }
    }
    scan->headerParsed = true;
    return;
  }
  for (int c = 0; c < TARGET_COLUMN_COUNT; c++) {
    int col = scan->targetCols[c];
    if (col >= 0 && col < fieldCount)
      scan->callback(fields[col], scan->userData);
  }
}

// 扫描目录下全部 CSV，逐行回调。解析器与截断告警由 utils 负责，
// 这里只管选文件与定位列。
static void ScanCsvDir(const char* dir, TextCallback cb, void* userData) {
  FilePathList files = LoadDirectoryFiles(dir);
  for (unsigned int i = 0; i < files.count; i++) {
    if (!IsFileExtension(files.paths[i], ".csv")) continue;
    FileScan scan = {.targetCols = {-1, -1, -1, -1},
                     .headerParsed = false,
                     .callback = cb,
                     .userData = userData};
    CsvReadFile(files.paths[i], OnCsvRow, &scan);
  }
  UnloadDirectoryFiles(files);
}

// === 收集 ===

// 把字段文本拷进快照，供缺字检查复用
static void SnapshotText(const char* text) {
  if (sTextCount == sTextCapacity) {
    int capacity = sTextCapacity ? sTextCapacity * 2 : 64;
    char** grown = (char**)MemRealloc(sTexts, (size_t)capacity * sizeof(char*));
    if (!grown) {
      TraceLog(LOG_ERROR,
               "Font: failed to grow the text snapshot list, later text is not "
               "collected");
      return;
    }
    sTexts = grown;
    sTextCapacity = capacity;
  }
  size_t size = (size_t)strlen(text) + 1;
  char* copy = (char*)MemAlloc(size);
  if (!copy) {
    TraceLog(LOG_ERROR, "Font: text snapshot allocation failed");
    return;
  }
  memcpy(copy, text, size);
  sTexts[sTextCount++] = copy;
}

static void CollectCallback(const char* text, void* unused) {
  (void)unused;
  SnapshotText(text);
  CollectFromString(text);
}

static void AddBufferCodepoints(void) {
  // ASCII 可见字符
  for (int c = 32; c <= 126; c++) MarkCodepoint(c);

  // 常用中文标点
  static const int punct[] = {
      0x3001, 0x3002, 0xFF0C, 0xFF01, 0xFF1F, 0xFF0A, 0xFF1B,
      0x201C, 0x201D, 0x2018, 0x2019, 0xFF08, 0xFF09, 0x3010,
      0x3011, 0x300A, 0x300B, 0x2026, 0x2014,
  };
  int pn = (int)(sizeof(punct) / sizeof(punct[0]));
  for (int i = 0; i < pn; i++) MarkCodepoint(punct[i]);
}

static void CollectAllCodepoints(void) {
  memset(cpBitmap, 0, sizeof(cpBitmap));
  sDroppedBeyondRange = 0;

  ScanCsvDir(DATABASE_DIR, CollectCallback, NULL);
  // 界面文案同样是精确收集的来源，因此界面用字不需要在代码里另列一份。
  // 目录跟着当前语言走，所以 LangSet() 必须先于 InitGameFont()（见 lang.h）。
  ScanCsvDir(LangStringsDir(), CollectCallback, NULL);

  AddBufferCodepoints();

  if (sDroppedBeyondRange > 0) {
    TraceLog(LOG_WARNING,
             "Font: %d codepoint(s) beyond collection range (above U+%04X), "
             "not in the font atlas",
             sDroppedBeyondRange, (unsigned)BMP_RANGE);
#ifndef NDEBUG
    sDroppedReported = true;
#endif
  }
}

static void BuildCodepointArray(void) {
  collectedCount = 0;
  for (int i = 0; i < BMP_RANGE; i++)
    if (IsCodepointMarked(i)) collectedCount++;

  if (collectedCount == 0) {
    TraceLog(LOG_WARNING, "Font: no codepoint collected");
    return;
  }

  collectedCps = (int*)MemAlloc((size_t)collectedCount * sizeof(int));
  if (!collectedCps) {
    TraceLog(LOG_ERROR, "Font: codepoint array allocation failed (%d)",
             collectedCount);
    collectedCount = 0;
    return;
  }
  int idx = 0;
  for (int i = 0; i < BMP_RANGE; i++)
    if (IsCodepointMarked(i)) collectedCps[idx++] = i;
  // 已按升序排列（遍历顺序即为升序）
}

#ifndef NDEBUG
// 收集阶段的不变式：进数组的码点必然可收集；一旦发生越界丢弃，
// 收集阶段必须已经告警过，否则说明告警与计数脱节。
static void AssertCollectedInvariants(void) {
  for (int i = 0; i < collectedCount; i++) {
    assert(IsCollectable(collectedCps[i]));
  }
  assert(sDroppedBeyondRange == 0 || sDroppedReported);
}
#endif

// === 缺字检查 ===

static void CheckCallback(const char* text, void* userData) {
  int* missing = (int*)userData;
  int offset = 0;
  int cpSize = 0;
  while (text[offset] != '\0') {
    int cp = GetCodepointNext(text + offset, &cpSize);
    if (cpSize <= 0) cpSize = 1;  // 防止死循环
    if (cp > 0) {
      if (!IsCollectable(cp)) {
        // 收不进图集的码点单独计数，不计入缺字
        sSkippedBeyondRange++;
      } else if (!FontHasGlyph(cp)) {
        TraceLog(LOG_WARNING, "Font: missing glyph U+%04X", (unsigned)cp);
        (*missing)++;
      }
    }
    offset += cpSize;
  }
}

static void CheckMissingGlyphs(int* missing) {
  *missing = 0;
  sSkippedBeyondRange = 0;
  // 复用收集阶段的文本快照，不再读第二遍盘
  for (int i = 0; i < sTextCount; i++) CheckCallback(sTexts[i], missing);
  if (sSkippedBeyondRange > 0) {
    TraceLog(LOG_WARNING,
             "Font: %d codepoint(s) beyond collection range at the check "
             "stage, not counted as missing glyphs",
             sSkippedBeyondRange);
  }
}

// === 字体加载 ===
static int FindSizeIndex(int size) {
  for (int i = 0; i < FONT_SIZE_COUNT; i++)
    if (kFontSizes[i] == size) return i;
  return -1;
}

// 可用的字体即加载成功的图集。LoadFontEx 有两种失败形态：文件读不到时
// 返回零值 Font（glyphCount 为 0，DrawTextEx/MeasureTextEx 会越界）；
// TTF 解析失败时 raylib 会退回内置 ASCII 默认字体，那份 Font 的
// texture.id 不为 0，只查它就会把默认字体当成图集、中文照样静默变方框，
// 故额外比对 glyphs 指针是不是默认字体那一份。
bool FontIsUsable(Font font) {
  return font.texture.id != 0 && font.glyphs != GetFontDefault().glyphs;
}

static void LoadFontAt(int idx) {
  if (!collectedCps || collectedCount == 0) {
    // 此时 LoadFontEx 只能拿到 ASCII，中文会静默变方框，故拒绝加载
    if (!sCollectWarned) {
      sCollectWarned = true;
      TraceLog(LOG_WARNING,
               "Font: codepoint collection not finished, refusing to load the "
               "font atlas");
    }
    return;
  }
  sFonts[idx] =
      LoadFontEx(FONT_PATH, kFontLoadSizes[idx], collectedCps, collectedCount);
  if (!FontIsUsable(sFonts[idx])) {
    if (sFonts[idx].texture.id == 0) {
      TraceLog(LOG_ERROR, "Font: failed to load %s@%d", FONT_PATH,
               kFontLoadSizes[idx]);
    } else {
      TraceLog(LOG_ERROR,
               "Font: %s@%d failed to parse, raylib fell back to the built-in "
               "default font",
               FONT_PATH, kFontLoadSizes[idx]);
    }
    sFontStates[idx] = kFontStateFailed;
    return;
  }
  SetTextureFilter(sFonts[idx].texture, TEXTURE_FILTER_POINT);
  sFontStates[idx] = kFontStateReady;
}

Font GetUIFont(int size) {
  int idx = FindSizeIndex(size);
  if (idx < 0) idx = FindSizeIndex(DEFAULT_FONT_SIZE);
  if (idx < 0) idx = 0;  // 最终回退

  if (sFontStates[idx] == kFontStateUnloaded) LoadFontAt(idx);
  if (sFontStates[idx] == kFontStateReady) return sFonts[idx];

  // 该尺寸不可用：回退到其它尺寸中已经加载成功的一个
  for (int i = 1; i <= FONT_SIZE_COUNT; i++) {
    int j = (idx + i) % FONT_SIZE_COUNT;
    if (sFontStates[j] == kFontStateReady) return sFonts[j];
  }
  // 全尺寸都失败：返回未加载的 Font，调用方按 texture.id == 0 早退
  return sFonts[idx];
}

// === 公开接口 ===

bool FontHasGlyph(int codepoint) {
  if (!collectedCps || collectedCount == 0) return false;
  int lo = 0, hi = collectedCount - 1;
  while (lo <= hi) {
    int mid = lo + (hi - lo) / 2;
    if (collectedCps[mid] == codepoint) return true;
    if (collectedCps[mid] < codepoint)
      lo = mid + 1;
    else
      hi = mid - 1;
  }
  return false;
}

void DrawUIText(int posX, int posY, const char* text, int size, Color tint) {
  Font f = GetUIFont(size);
  // 未加载成功的 Font 会让 DrawTextEx 越界，直接不画
  if (!FontIsUsable(f)) return;
  // 按图集分辨率（baseSize）绘制，scaleFactor 才是 1，字形原样落到像素格。
  DrawTextEx(f, text, (Vector2){(float)posX, (float)posY}, (float)f.baseSize,
             1.0f, tint);
}

Vector2 MeasureUIText(const char* text, int size) {
  Font f = GetUIFont(size);
  if (!FontIsUsable(f)) return (Vector2){0, 0};
  return MeasureTextEx(f, text, (float)f.baseSize, 1.0f);
}

void InitGameFont(void) {
  if (initialized) return;
  initialized = true;

  // 1. 收集 codepoint
  CollectAllCodepoints();

  // 2. 构建排序数组
  BuildCodepointArray();

#ifndef NDEBUG
  AssertCollectedInvariants();
#endif

  TraceLog(LOG_INFO, "Font: collected %d unique codepoints", collectedCount);

  // 3. 加载默认尺寸（24px），触发图集加载
  GetUIFont(DEFAULT_FONT_SIZE);

  // 4. 缺字检查
  int missing = 0;
  CheckMissingGlyphs(&missing);
  if (missing == 0) {
    TraceLog(LOG_INFO, "Font: glyph coverage check passed");
  } else {
    TraceLog(LOG_WARNING, "Font: %d missing glyph(s) found", missing);
  }
}

void UnloadGameFont(void) {
  for (int i = 0; i < FONT_SIZE_COUNT; i++) {
    // 只卸载真正加载成功的字体，失败档位的 Font 是零值占位
    if (sFontStates[i] == kFontStateReady) UnloadFont(sFonts[i]);
    sFontStates[i] = kFontStateUnloaded;
  }
  MemFree(collectedCps);
  collectedCps = NULL;
  collectedCount = 0;

  for (int i = 0; i < sTextCount; i++) MemFree(sTexts[i]);
  MemFree(sTexts);
  sTexts = NULL;
  sTextCount = 0;
  sTextCapacity = 0;

  sCollectWarned = false;
  initialized = false;
}