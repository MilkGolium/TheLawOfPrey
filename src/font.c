#include "font.h"

#include <string.h>

#include "raygui.h"

// === 配置 ===

#define FONT_PATH "assets/fonts/ttf/zpix.ttf"
#define CSV_DIR "assets/database"
#define DEFAULT_FONT_SIZE 24
#define BMP_RANGE 0x10000
#define CSV_FIELD_MAX 1024
#define FONT_SIZE_COUNT 3

static const int kFontSizes[FONT_SIZE_COUNT] = {12, 24, 36};

// BMP 范围内的 codepoint 去重位图（覆盖 CJK 基本区与 ASCII）
static unsigned char cpBitmap[BMP_RANGE / 8];

// 收集到的唯一 codepoint，升序排列
static int* collectedCps = NULL;
static int collectedCount = 0;

// 按尺寸懒加载的字体
static Font fonts[FONT_SIZE_COUNT];
static bool fontsLoaded[FONT_SIZE_COUNT];
static bool initialized = false;

// === codepoint 去重 ===

static void MarkCodepoint(int cp) {
  if (cp < 0 || cp >= BMP_RANGE) return;
  cpBitmap[cp >> 3] |= (unsigned char)(1u << (cp & 7));
}

static bool IsCodepointMarked(int cp) {
  if (cp < 0 || cp >= BMP_RANGE) return false;
  return (cpBitmap[cp >> 3] & (1u << (cp & 7))) != 0;
}

static void CollectFromString(const char* text) {
  if (!text) return;
  int count = 0;
  int* cps = LoadCodepoints(text, &count);
  for (int i = 0; i < count; i++) MarkCodepoint(cps[i]);
  UnloadCodepoints(cps);
}

// === CSV 解析 ===

typedef struct {
  const char* end;
  const char* cursor;
} CsvStream;

// 读取一个 CSV 字段到 out，处理引号与内嵌逗号。
// rowEnd 为 true 表示该字段是当前行最后一个。
// 返回 false 表示流已耗尽。
static bool CsvNextField(CsvStream* s, char* out, int outSize, bool* rowEnd) {
  *rowEnd = false;
  if (s->cursor >= s->end) return false;

  const char* p = s->cursor;
  int outLen = 0;

  if (*p == '"') {
    // 引号字段
    p++;
    while (p < s->end) {
      if (*p == '"') {
        if (p + 1 < s->end && p[1] == '"') {
          if (outLen < outSize - 1) out[outLen++] = '"';
          p += 2;
        } else {
          p++;
          break;
        }
      } else {
        if (outLen < outSize - 1) out[outLen++] = *p;
        p++;
      }
    }
    // 跳过引号结束后的多余字符，直到分隔符
    while (p < s->end && *p != ',' && *p != '\n' && *p != '\r') p++;
  } else {
    // 非引号字段
    while (p < s->end && *p != ',' && *p != '\n' && *p != '\r') {
      if (outLen < outSize - 1) out[outLen++] = *p;
      p++;
    }
  }

  out[outLen] = '\0';

  // 消费分隔符
  if (p < s->end) {
    if (*p == ',') {
      p++;
    } else if (*p == '\r') {
      p++;
      if (p < s->end && *p == '\n') p++;
      *rowEnd = true;
    } else {  // '\n'
      p++;
      *rowEnd = true;
    }
  } else {
    *rowEnd = true;  // 文件末尾视为行结束
  }

  s->cursor = p;
  return true;
}

// 对 CSV 中目标列的每个文本字段调用 callback
typedef void (*TextCallback)(const char* text, void* userData);

static void ProcessCsvText(const char* path, TextCallback cb, void* userData) {
  int dataSize = 0;
  unsigned char* data = LoadFileData(path, &dataSize);
  if (!data || dataSize == 0) {
    TraceLog(LOG_WARNING, "Font: 无法读取 CSV: %s", path);
    return;
  }

  // 跳过 UTF-8 BOM
  const char* p = (const char*)data;
  const char* end = p + dataSize;
  if (dataSize >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
    p += 3;
  }

  CsvStream s = {.end = end, .cursor = p};
  char field[CSV_FIELD_MAX];
  bool rowEnd = false;

  // 解析表头，定位目标列
  int displayCol = -1, descCol = -1, inspectCol = -1;
  int col = 0;
  do {
    if (!CsvNextField(&s, field, sizeof(field), &rowEnd)) break;
    if (strcmp(field, "display_name") == 0)
      displayCol = col;
    else if (strcmp(field, "description") == 0)
      descCol = col;
    else if (strcmp(field, "inspect_text") == 0)
      inspectCol = col;
    col++;
  } while (!rowEnd);

  if (displayCol < 0 && descCol < 0 && inspectCol < 0) {
    TraceLog(LOG_INFO, "Font: %s 无目标列，跳过", path);
    UnloadFileData(data);
    return;
  }

  // 遍历数据行，从目标列收集文本
  bool eof = false;
  while (!eof && s.cursor < s.end) {
    col = 0;
    do {
      if (!CsvNextField(&s, field, sizeof(field), &rowEnd)) {
        eof = true;
        break;
      }
      if (col == displayCol || col == descCol || col == inspectCol) {
        cb(field, userData);
      }
      col++;
    } while (!rowEnd);
  }

  UnloadFileData(data);
}

// === 收集 ===

static void CollectCallback(const char* text, void* unused) {
  (void)unused;
  CollectFromString(text);
}

static void AddBufferCodepoints(void) {
  // ASCII 可见字符
  for (int c = 32; c <= 126; c++) MarkCodepoint(c);

  // 常用中文标点
  static const int punct[] = {
      0x3001, 0x3002, 0xFF0C, 0xFF01, 0xFF1F, 0xFF1A, 0xFF1B,
      0x201C, 0x201D, 0x2018, 0x2019, 0xFF08, 0xFF09, 0x3010,
      0x3011, 0x300A, 0x300B, 0x2026, 0x2014,
  };
  int n = (int)(sizeof(punct) / sizeof(punct[0]));
  for (int i = 0; i < n; i++) MarkCodepoint(punct[i]);
}

static void CollectAllCodepoints(void) {
  memset(cpBitmap, 0, sizeof(cpBitmap));

  FilePathList files = LoadDirectoryFiles(CSV_DIR);
  for (unsigned int i = 0; i < files.count; i++) {
    if (IsFileExtension(files.paths[i], ".csv"))
      ProcessCsvText(files.paths[i], CollectCallback, NULL);
  }
  UnloadDirectoryFiles(files);

  AddBufferCodepoints();
}

static void BuildCodepointArray(void) {
  collectedCount = 0;
  for (int i = 0; i < BMP_RANGE; i++)
    if (IsCodepointMarked(i)) collectedCount++;

  if (collectedCount == 0) {
    TraceLog(LOG_WARNING, "Font: 未收集到任何 codepoint");
    return;
  }

  collectedCps = (int*)MemAlloc(collectedCount * sizeof(int));
  int idx = 0;
  for (int i = 0; i < BMP_RANGE; i++)
    if (IsCodepointMarked(i)) collectedCps[idx++] = i;
  // 已按升序排列（遍历顺序即为升序）
}

// === 缺字检查 ===

static void CheckCallback(const char* text, void* userData) {
  int* missing = (int*)userData;
  int offset = 0;
  int cpSize = 0;
  while (text[offset] != '\0') {
    int cp = GetCodepointNext(text + offset, &cpSize);
    if (cpSize <= 0) cpSize = 1;  // 防止死循环
    if (cp > 0 && !Font_HasGlyph(cp)) {
      TraceLog(LOG_WARNING, "Font: 缺字 U+%04X", (unsigned)cp);
      (*missing)++;
    }
    offset += cpSize;
  }
}

static void CheckMissingGlyphs(int* missing) {
  *missing = 0;
  FilePathList files = LoadDirectoryFiles(CSV_DIR);
  for (unsigned int i = 0; i < files.count; i++) {
    if (IsFileExtension(files.paths[i], ".csv"))
      ProcessCsvText(files.paths[i], CheckCallback, missing);
  }
  UnloadDirectoryFiles(files);
}

// === 字体加载 ===

static int FindSizeIndex(int size) {
  for (int i = 0; i < FONT_SIZE_COUNT; i++)
    if (kFontSizes[i] == size) return i;
  return -1;
}

Font GetUIFont(int size) {
  int idx = FindSizeIndex(size);
  if (idx < 0) idx = FindSizeIndex(DEFAULT_FONT_SIZE);
  if (idx < 0) idx = 0;  // 最终回退

  if (!fontsLoaded[idx]) {
    fonts[idx] =
        LoadFontEx(FONT_PATH, kFontSizes[idx], collectedCps, collectedCount);
    if (fonts[idx].texture.id == 0) {
      TraceLog(LOG_ERROR, "Font: 加载失败 %s@%d", FONT_PATH, kFontSizes[idx]);
    } else {
      SetTextureFilter(fonts[idx].texture, TEXTURE_FILTER_POINT);
    }
    fontsLoaded[idx] = true;
  }
  return fonts[idx];
}

// === 公开接口 ===

bool Font_HasGlyph(int codepoint) {
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
  DrawTextEx(f, text, (Vector2){(float)posX, (float)posY}, (float)size, 1.0f,
             tint);
}

Vector2 MeasureUIText(const char* text, int size) {
  Font f = GetUIFont(size);
  return MeasureTextEx(f, text, (float)size, 1.0f);
}

void InitGameFont(void) {
  if (initialized) return;
  initialized = true;

  // 1. 收集 codepoint
  CollectAllCodepoints();

  // 2. 构建排序数组
  BuildCodepointArray();

  TraceLog(LOG_INFO, "Font: 收集到 %d 个唯一 codepoint", collectedCount);

  // 3. 加载默认尺寸（24px）
  Font f = GetUIFont(DEFAULT_FONT_SIZE);

  // 4. 同步给 raygui
  if (f.texture.id > 0) {
    GuiSetFont(f);
    GuiSetStyle(DEFAULT, TEXT_SIZE, DEFAULT_FONT_SIZE);
    GuiSetStyle(DEFAULT, TEXT_SPACING, 1);
  }

  // 5. 缺字检查
  int missing = 0;
  CheckMissingGlyphs(&missing);
  if (missing == 0) {
    TraceLog(LOG_INFO, "Font: 缺字检查通过");
  } else {
    TraceLog(LOG_WARNING, "Font: 发现 %d 处缺字", missing);
  }
}

void UnloadGameFont(void) {
  for (int i = 0; i < FONT_SIZE_COUNT; i++) {
    if (fontsLoaded[i]) {
      UnloadFont(fonts[i]);
      fontsLoaded[i] = false;
    }
  }
  if (collectedCps) {
    MemFree(collectedCps);
    collectedCps = NULL;
    collectedCount = 0;
  }
  initialized = false;
}
