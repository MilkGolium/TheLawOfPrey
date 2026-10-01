#include "theme.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ui.h"

// === 配置 ===

#define THEME_DIR "assets/themes"
#define THEME_DEFAULT_NAME "default"
#define THEME_LINE_MAX 512
#define THEME_NAME_MAX 64
#define THEME_MAX_COUNT 64

// === 键表 ===

// 配色键映射到 UiStyle 的字段偏移，以 offsetof 代替 raygui 的
// GuiControlProperty 枚举，UiStyle 增加字段时无需改键表实现。
// 键名在匹配前归一为小写并剥掉全部分隔符（NormalizeKey），
// 因此这里一律写扁平小写：配置里 textDim、text-dim、text_dim
// 三种写法等价，都能命中 textdim 这一项。
typedef struct {
  const char* key;
  size_t offset;
} ThemeColorKey;

static const ThemeColorKey kColorKeys[] = {
    {"desktop", offsetof(UiStyle, desktop)},
    {"background", offsetof(UiStyle, background)},
    {"line", offsetof(UiStyle, line)},
    {"linedim", offsetof(UiStyle, lineDim)},
    {"text", offsetof(UiStyle, text)},
    {"textdim", offsetof(UiStyle, textDim)},
    {"button", offsetof(UiStyle, button)},
    {"buttontext", offsetof(UiStyle, buttonText)},
    {"titlebar", offsetof(UiStyle, titleBar)},
    {"titletext", offsetof(UiStyle, titleText)},
    {"shadow", offsetof(UiStyle, shadow)},
};

// 枚举键。取值是标识符而非十六进制，标识表同样按归一后的
// 小写存：border_style、border-style、borderStyle 都收敛到 borderstyle。
// 值列的顺序必须与 ui.h 里枚举的声明顺序一致，下标直接当枚举值用。
typedef struct {
  const char* key;
  size_t offset;
  const char* const* names;
  int nameCount;
} ThemeEnumKey;

static const char* const kBorderStyleNames[] = {"none", "single", "triple"};
static const char* const kShadowStyleNames[] = {"auto", "solid", "dither"};

static const ThemeEnumKey kEnumKeys[] = {
    {"borderstyle", offsetof(UiStyle, borderStyle), kBorderStyleNames, 3},
    {"shadowstyle", offsetof(UiStyle, shadowStyle), kShadowStyleNames, 3},
};

// auto 的分界：桌面色亮度低于一半就判暗。整数近似即可，
// 目的是决定实心还是抖动，不需要感知精确。
// 判暗用实心实色阴影，判亮用抖动，否则黑实心会糊成一片脏。
static const int kShadowLumaThreshold = 128;

static int Luminance(Color c) { return (c.r * 30 + c.g * 59 + c.b * 11) / 100; }

// 字体度量与边框宽度由程序独占。社区主题若能改动它们，
// 就能把 24px 像素格对齐与中文渲染悄悄改坏，因此只告警不生效。
// 键名经 NormalizeKey 归一（剥分隔符），这里存扁平小写形式。
static const char* kForbiddenKeys[] = {
    "textsize",      "textspacing",           "textlinespacing",
    "textalignment", "textalignmentvertical", "textpadding",
    "borderwidth",
};

// === 状态 ===

static char themeNames[THEME_MAX_COUNT][THEME_NAME_MAX];
static int themeCount = 0;
static char currentTheme[THEME_NAME_MAX] = "";

// === 文本工具 ===

static char* Trim(char* s) {
  while (*s == ' ' || *s == '\t') s++;
  size_t len = strlen(s);
  while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t')) s[--len] = '\0';
  return s;
}

// 键名统一为小写并剥掉连字符与下划线，使 text-dim、text_dim、
// textDim 全部收敛为 textdim，等价匹配。调用方保证 s 可写。
static void NormalizeKey(char* s) {
  char* out = s;
  for (const char* p = s; *p != '\0'; p++) {
    char c = *p;
    if (c == '-' || c == '_') continue;
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    *out++ = c;
  }
  *out = '\0';
}

// 只接受 6 位或 8 位十六进制，写作 0x 前缀亦可。6 位按不透明处理。
// 少于 6 位或介于两者之间的位数一律判为非法，避免静默产生错误配色。
static bool ParseColor(const char* text, Color* out) {
  const char* p = text;
  if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;

  char* end = NULL;
  unsigned long value = strtoul(p, &end, 16);
  if (end == p) return false;

  int digits = (int)(end - p);
  if (digits != 6 && digits != 8) return false;
  if (*end != '\0') return false;

  // 不补高位的话 6 位会解出 0x00RRGGBB，四个通道全部错位。
  if (digits == 6) value = (value << 8) | 0xFF;

  out->r = (unsigned char)((value >> 24) & 0xFF);
  out->g = (unsigned char)((value >> 16) & 0xFF);
  out->b = (unsigned char)((value >> 8) & 0xFF);
  out->a = (unsigned char)(value & 0xFF);
  return true;
}

// === 查表 ===

static size_t FindColorOffset(const char* normalizedKey) {
  int count = (int)(sizeof(kColorKeys) / sizeof(kColorKeys[0]));
  for (int i = 0; i < count; i++) {
    if (strcmp(normalizedKey, kColorKeys[i].key) == 0)
      return kColorKeys[i].offset;
  }
  return SIZE_MAX;
}

static const ThemeEnumKey* FindEnumKey(const char* normalizedKey) {
  int count = (int)(sizeof(kEnumKeys) / sizeof(kEnumKeys[0]));
  for (int i = 0; i < count; i++) {
    if (strcmp(normalizedKey, kEnumKeys[i].key) == 0) return &kEnumKeys[i];
  }
  return NULL;
}

static bool IsForbiddenKey(const char* normalizedKey) {
  int count = (int)(sizeof(kForbiddenKeys) / sizeof(kForbiddenKeys[0]));
  for (int i = 0; i < count; i++) {
    if (strcmp(normalizedKey, kForbiddenKeys[i]) == 0) return true;
  }
  return false;
}

// === 解析 ===

// 解析一行 key = value 写入 style，成功写进一个配色或装饰枚举返回 true。
// 调用方保证 style 以默认值起步：这里只覆盖文件里出现的键。
static bool ProcessLine(const char* fileName, const char* raw, int length,
                        UiStyle* style) {
  char line[THEME_LINE_MAX];
  if (length >= THEME_LINE_MAX) {
    TraceLog(LOG_WARNING, "Theme: %s 存在超长行（%d 字符），已截断", fileName,
             length);
    length = THEME_LINE_MAX - 1;
  }
  memcpy(line, raw, (size_t)length);
  line[length] = '\0';

  char* s = Trim(line);
  if (s[0] == '\0' || s[0] == '#' || s[0] == ';') return false;

  char* eq = strchr(s, '=');
  if (eq == NULL) {
    TraceLog(LOG_WARNING, "Theme: %s 无法解析的行：%s", fileName, s);
    return false;
  }
  *eq = '\0';
  char* key = Trim(s);
  char* value = Trim(eq + 1);

  NormalizeKey(key);

  if (IsForbiddenKey(key)) {
    TraceLog(LOG_WARNING, "Theme: %s 的 %s 由程序独占，主题无权设置，已忽略",
             fileName, key);
    return false;
  }

  size_t offset = FindColorOffset(key);
  if (offset != SIZE_MAX) {
    Color color;
    if (!ParseColor(value, &color)) {
      TraceLog(LOG_WARNING, "Theme: %s 的 %s 取值非法：%s", fileName, key,
               value);
      return false;
    }
    *(Color*)((char*)style + offset) = color;
    return true;
  }

  const ThemeEnumKey* enumKey = FindEnumKey(key);
  if (enumKey == NULL) {
    TraceLog(LOG_WARNING, "Theme: %s 未知主题键 %s，已忽略", fileName, key);
    return false;
  }

  // 枚举取值不归一：标识符按原样比对，未知值只告警并跳过该行，
  // 与坏十六进制的处理一致，不牵连同一文件里的其他键。
  for (int i = 0; i < enumKey->nameCount; i++) {
    if (strcmp(value, enumKey->names[i]) == 0) {
      *(int*)((char*)style + enumKey->offset) = i;
      return true;
    }
  }

  TraceLog(LOG_WARNING, "Theme: %s 的 %s 取值非法：%s", fileName, key, value);
  return false;
}

// === 应用 ===

// 从内置默认配色出发解析整个文件，只覆盖文件写到且解析成功的键：
// 缺键即默认色，坏键逐条告警且不影响其他行。解析结果先放在局部
// 副本，没有任何一行成功时才整体放弃（保持当前样式不变），避免
// 坏文件留下半套配色。
static bool ApplyThemeFile(const char* path, const char* fileName) {
  int size = 0;
  unsigned char* data = LoadFileData(path, &size);
  if (data == NULL || size == 0) {
    TraceLog(LOG_WARNING, "Theme: 无法读取主题文件 %s", path);
    if (data != NULL) UnloadFileData(data);
    return false;
  }

  const char* p = (const char*)data;
  const char* end = p + size;
  if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
    p += 3;
  }

  UiStyle style = kDefaultUiStyle;
  int applied = 0;

  while (p < end) {
    const char* lineEnd = p;
    while (lineEnd < end && *lineEnd != '\n' && *lineEnd != '\r') lineEnd++;
    if (ProcessLine(fileName, p, (int)(lineEnd - p), &style)) applied++;
    p = lineEnd;
    while (p < end && (*p == '\n' || *p == '\r')) p++;
  }

  UnloadFileData(data);

  if (applied == 0) {
    TraceLog(LOG_WARNING,
             "Theme: %s 未包含任何有效配色或装饰设置，保持当前样式", fileName);
    return false;
  }

  // auto 在这里定死：渲染层只做 switch，不再判断桌面亮度。
  if (style.shadowStyle == UI_SHADOW_AUTO) {
    style.shadowStyle = Luminance(style.desktop) < kShadowLumaThreshold
                            ? UI_SHADOW_SOLID
                            : UI_SHADOW_DITHER;
  }

  gUiStyle = style;
  return true;
}

// === 扫描 ===

static int CompareThemeName(const void* a, const void* b) {
  return strcmp((const char*)a, (const char*)b);
}

static void ScanThemes(void) {
  themeCount = 0;

  FilePathList files = LoadDirectoryFiles(THEME_DIR);
  if (files.count == 0) {
    TraceLog(LOG_WARNING, "Theme: %s 下没有找到主题文件，使用内置默认配色",
             THEME_DIR);
    UnloadDirectoryFiles(files);
    return;
  }

  int found = 0;
  for (unsigned int i = 0; i < files.count; i++) {
    if (!IsFileExtension(files.paths[i], ".cfg")) continue;
    if (found >= THEME_MAX_COUNT) {
      TraceLog(LOG_WARNING, "Theme: 主题数量超过上限 %d，忽略 %s",
               THEME_MAX_COUNT, files.paths[i]);
      continue;
    }

    const char* base = files.paths[i];
    const char* slash = strrchr(base, '/');
    if (slash != NULL) base = slash + 1;
    const char* dot = strrchr(base, '.');

    int len = dot != NULL ? (int)(dot - base) : (int)strlen(base);
    if (len <= 0 || len >= THEME_NAME_MAX) {
      TraceLog(LOG_WARNING, "Theme: 主题名长度非法，已忽略 %s", files.paths[i]);
      continue;
    }

    memcpy(themeNames[found], base, (size_t)len);
    themeNames[found][len] = '\0';
    found++;
  }

  // LoadDirectoryFiles 的返回顺序不保证稳定，排序后每次启动的
  // 主题列表顺序才一致，设置界面里的下标才有意义。
  qsort(themeNames, (size_t)found, THEME_NAME_MAX, CompareThemeName);

  themeCount = found;
  UnloadDirectoryFiles(files);
  TraceLog(LOG_INFO, "Theme: 在 %s 下发现 %d 个主题", THEME_DIR, themeCount);
}

// === 公开接口 ===

int GetThemeCount(void) { return themeCount; }

const char* GetThemeName(int index) {
  if (index < 0 || index >= themeCount) return "";
  return themeNames[index];
}

bool LoadTheme(const char* name) {
  if (name == NULL || name[0] == '\0') {
    TraceLog(LOG_WARNING, "Theme: 主题名为空");
    return false;
  }
  if (strlen(name) >= THEME_NAME_MAX) {
    TraceLog(LOG_WARNING, "Theme: 主题名过长，已忽略");
    return false;
  }

  if (!ApplyThemeFile(TextFormat("%s/%s.cfg", THEME_DIR, name), name)) {
    return false;
  }

  TextCopy(currentTheme, name);
  TraceLog(LOG_INFO, "Theme: 已加载主题 %s", currentTheme);
  return true;
}

const char* GetCurrentThemeName(void) { return currentTheme; }

void InitTheme(void) {
  ScanThemes();
  if (themeCount == 0) return;

  if (!LoadTheme(THEME_DEFAULT_NAME)) {
    // 缺少 default.cfg 时退回列表首项，保证社区只放自定义主题
    // 也能正常启动。
    const char* first = GetThemeName(0);
    if (strcmp(first, THEME_DEFAULT_NAME) != 0) LoadTheme(first);
  }
}