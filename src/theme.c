#include "theme.h"

#include <stdlib.h>
#include <string.h>

#include "raygui.h"

// === 配置 ===

#define THEME_DIR "assets/themes"
#define THEME_DEFAULT_NAME "default"
#define THEME_LINE_MAX 512
#define THEME_NAME_MAX 64
#define THEME_MAX_COUNT 64
#define THEME_MAX_ENTRIES 256

// === 键表 ===

typedef struct {
  const char* key;
  int property;
} ThemeColorKey;

// 全局配色键，映射到 GuiControlProperty。顺序与 raygui 的
// GuiLoadStyleDefault() 保持一致，便于逐项核对。
static const ThemeColorKey kColorKeys[] = {
    {"border_normal", BORDER_COLOR_NORMAL},
    {"base_normal", BASE_COLOR_NORMAL},
    {"text_normal", TEXT_COLOR_NORMAL},
    {"border_focused", BORDER_COLOR_FOCUSED},
    {"base_focused", BASE_COLOR_FOCUSED},
    {"text_focused", TEXT_COLOR_FOCUSED},
    {"border_pressed", BORDER_COLOR_PRESSED},
    {"base_pressed", BASE_COLOR_PRESSED},
    {"text_pressed", TEXT_COLOR_PRESSED},
    {"border_disabled", BORDER_COLOR_DISABLED},
    {"base_disabled", BASE_COLOR_DISABLED},
    {"text_disabled", TEXT_COLOR_DISABLED},
    {"line", LINE_COLOR},
    {"background", BACKGROUND_COLOR},
};

// 字体度量与边框宽度由程序独占。社区主题若能改动它们，
// 就能把 24px 像素格对齐与中文渲染悄悄改坏，因此只告警不生效。
static const char* kForbiddenKeys[] = {
    "text_size",
    "text_spacing",
    "text_line_spacing",
    "text_alignment",
    "text_alignment_vertical",
    "text_padding",
    "border_width",
};

typedef struct {
  const char* name;
  int control;
} ThemeControlName;

// 节区名到 raygui 控件类型的映射。匹配时忽略大小写与分隔符，
// 因此 textbox、text-box、text_box 等价。
static const ThemeControlName kControlNames[] = {
    {"label", LABEL},
    {"button", BUTTON},
    {"toggle", TOGGLE},
    {"slider", SLIDER},
    {"progressbar", PROGRESSBAR},
    {"checkbox", CHECKBOX},
    {"combo", COMBOBOX},
    {"dropdown", DROPDOWNBOX},
    {"textbox", TEXTBOX},
    {"valuebox", VALUEBOX},
    {"tabbar", TABBAR},
    {"listview", LISTVIEW},
    {"colorpicker", COLORPICKER},
    {"scrollbar", SCROLLBAR},
    {"statusbar", STATUSBAR},
};

// === 状态 ===

typedef struct {
  int control;
  int property;
  int value;
} ThemeEntry;

static char themeNames[THEME_MAX_COUNT][THEME_NAME_MAX];
static int themeCount = 0;
static char currentTheme[THEME_NAME_MAX] = "";

static ThemeEntry staged[THEME_MAX_ENTRIES];
static int stagedCount = 0;

// === 文本工具 ===

static char* Trim(char* s) {
  while (*s == ' ' || *s == '\t') s++;
  size_t len = strlen(s);
  while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t')) s[--len] = '\0';
  return s;
}

// 键名统一为小写并把连字符换成下划线，使 border-normal 等价于
// border_normal。调用方保证 s 可写。
static void NormalizeKey(char* s) {
  for (char* p = s; *p != '\0'; p++) {
    if (*p == '-') {
      *p = '_';
    } else if (*p >= 'A' && *p <= 'Z') {
      *p = (char)(*p - 'A' + 'a');
    }
  }
}

// 忽略大小写与所有分隔符的宽松比较，专用于节区名匹配。
static bool LooseEquals(const char* a, const char* b) {
  while (*a != '\0' && *b != '\0') {
    while (*a == '-' || *a == '_' || *a == ' ') a++;
    while (*b == '-' || *b == '_' || *b == ' ') b++;
    if (*a == '\0' || *b == '\0') break;
    char ca = *a;
    char cb = *b;
    if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
    if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
    if (ca != cb) return false;
    a++;
    b++;
  }
  while (*a == '-' || *a == '_' || *a == ' ') a++;
  while (*b == '-' || *b == '_' || *b == ' ') b++;
  return *a == '\0' && *b == '\0';
}

// 只接受 6 位或 8 位十六进制，写作 0x 前缀亦可。少写位数会被
// 静默补成不透明色，与其给出错误配色，不如直接判为非法。
static bool ParseColor(const char* text, int* out) {
  const char* p = text;
  if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;

  char* end = NULL;
  unsigned long value = strtoul(p, &end, 16);
  if (end == p) return false;

  int digits = (int)(end - p);
  if (digits != 6 && digits != 8) return false;
  if (*end != '\0') return false;

  *out = (int)value;
  return true;
}

// === 查表 ===

static int FindColorProperty(const char* normalizedKey) {
  int count = (int)(sizeof(kColorKeys) / sizeof(kColorKeys[0]));
  for (int i = 0; i < count; i++) {
    if (strcmp(normalizedKey, kColorKeys[i].key) == 0)
      return kColorKeys[i].property;
  }
  return -1;
}

static bool IsForbiddenKey(const char* normalizedKey) {
  int count = (int)(sizeof(kForbiddenKeys) / sizeof(kForbiddenKeys[0]));
  for (int i = 0; i < count; i++) {
    if (strcmp(normalizedKey, kForbiddenKeys[i]) == 0) return true;
  }
  return false;
}

static int FindControl(const char* section) {
  int count = (int)(sizeof(kControlNames) / sizeof(kControlNames[0]));
  for (int i = 0; i < count; i++) {
    if (LooseEquals(section, kControlNames[i].name)) return kControlNames[i].control;
  }
  return -1;
}

// === 解析 ===

static void ProcessLine(const char* fileName, const char* raw, int length,
                        int* control) {
  char line[THEME_LINE_MAX];
  if (length >= THEME_LINE_MAX) {
    TraceLog(LOG_WARNING, "Theme: %s 存在超长行（%d 字符），已截断", fileName,
             length);
    length = THEME_LINE_MAX - 1;
  }
  memcpy(line, raw, (size_t)length);
  line[length] = '\0';

  char* s = Trim(line);
  if (s[0] == '\0' || s[0] == '#' || s[0] == ';') return;

  if (s[0] == '[') {
    size_t len = strlen(s);
    if (s[len - 1] != ']') {
      TraceLog(LOG_WARNING, "Theme: %s 节区头缺少右括号：%s", fileName, s);
      return;
    }
    s[len - 1] = '\0';
    char* section = Trim(s + 1);
    int found = FindControl(section);
    if (found < 0) {
      TraceLog(LOG_WARNING, "Theme: %s 未知控件节区 [%s]，已忽略", fileName,
               section);
      return;
    }
    *control = found;
    return;
  }

  char* eq = strchr(s, '=');
  if (eq == NULL) {
    TraceLog(LOG_WARNING, "Theme: %s 无法解析的行：%s", fileName, s);
    return;
  }
  *eq = '\0';
  char* key = Trim(s);
  char* value = Trim(eq + 1);

  NormalizeKey(key);

  if (IsForbiddenKey(key)) {
    TraceLog(LOG_WARNING,
             "Theme: %s 的 %s 由程序独占，主题无权设置，已忽略", fileName, key);
    return;
  }

  int property = FindColorProperty(key);
  if (property < 0) {
    TraceLog(LOG_WARNING, "Theme: %s 未知配色键 %s，已忽略", fileName, key);
    return;
  }

  int color = 0;
  if (!ParseColor(value, &color)) {
    TraceLog(LOG_WARNING, "Theme: %s 的 %s 取值非法：%s", fileName, key, value);
    return;
  }

  if (stagedCount >= THEME_MAX_ENTRIES) {
    TraceLog(LOG_WARNING, "Theme: %s 配色项超过上限 %d，多余部分已忽略",
             fileName, THEME_MAX_ENTRIES);
    return;
  }

  staged[stagedCount].control = *control;
  staged[stagedCount].property = property;
  staged[stagedCount].value = color;
  stagedCount++;
}

// === 应用 ===

static void ApplyStaged(const char* fileName) {
  // 守住字体度量不变量。主题按构造只应触碰配色属性，
  // 一旦将来有人把禁用键误写进颜色表，这里会立刻暴露，
  // 而不是让界面在无人察觉时错位。
  int sizeBefore = GuiGetStyle(DEFAULT, TEXT_SIZE);
  int spacingBefore = GuiGetStyle(DEFAULT, TEXT_SPACING);
  int lineBefore = GuiGetStyle(DEFAULT, TEXT_LINE_SPACING);

  for (int i = 0; i < stagedCount; i++) {
    GuiSetStyle(staged[i].control, staged[i].property, staged[i].value);
  }

  if (GuiGetStyle(DEFAULT, TEXT_SIZE) != sizeBefore ||
      GuiGetStyle(DEFAULT, TEXT_SPACING) != spacingBefore ||
      GuiGetStyle(DEFAULT, TEXT_LINE_SPACING) != lineBefore) {
    TraceLog(LOG_ERROR, "Theme: %s 改动了字体度量，界面排版可能已损坏",
             fileName);
  }
}

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

  stagedCount = 0;
  int control = DEFAULT;

  while (p < end) {
    const char* lineEnd = p;
    while (lineEnd < end && *lineEnd != '\n' && *lineEnd != '\r') lineEnd++;
    ProcessLine(fileName, p, (int)(lineEnd - p), &control);
    p = lineEnd;
    while (p < end && (*p == '\n' || *p == '\r')) p++;
  }

  UnloadFileData(data);

  if (stagedCount == 0) {
    TraceLog(LOG_WARNING, "Theme: %s 未包含任何有效配色，保持当前样式",
             fileName);
    stagedCount = 0;
    return false;
  }

  ApplyStaged(fileName);
  stagedCount = 0;
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
    TraceLog(LOG_WARNING, "Theme: %s 下没有找到主题文件，使用 raygui 默认配色",
             THEME_DIR);
    UnloadDirectoryFiles(files);
    return;
  }

  int found = 0;
  for (unsigned int i = 0; i < files.count; i++) {
    if (!IsFileExtension(files.paths[i], ".cfg")) continue;
    if (found >= THEME_MAX_COUNT) {
      TraceLog(LOG_WARNING, "Theme: 主题数量超过上限 %d，忽略 %s", THEME_MAX_COUNT,
               files.paths[i]);
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
