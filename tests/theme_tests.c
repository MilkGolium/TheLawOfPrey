#include <stdio.h>
#include <string.h>

#include "test_util.h"
#include "theme.h"
#include "ui.h"
#include "utils.h"

// THEME_DIR 由 CMake 的编译定义指向 tests/tmp/themes，游戏本体那边
// 仍是 assets/themes。测试因此可以在自己的临时目录里造文件，
// 不会往 assets/ 下留任何东西。

// === 临时目录 ===

// raylib 6.0 的 MakeDirectory 会连父目录一起建。
static void MakeTempDir(void) {
  if (DirectoryExists(THEME_DIR)) return;
  MakeDirectory(THEME_DIR);
  if (!DirectoryExists(THEME_DIR))
    TestFail("temp theme directory cannot be created");
}

// 每条用例前清空临时目录：上一条用例留下的 .cfg 会被下一次 InitTheme
// 扫到，不清干净就成了互相污染的根源。raylib 6.0 没有删文件的接口，
// 删除走标准库的 remove。
static void ClearTempDir(void) {
  FilePathList files = LoadDirectoryFiles(THEME_DIR);
  for (unsigned int i = 0; i < files.count; i++) {
    if (!IsFileExtension(files.paths[i], ".cfg")) continue;
    if (remove(files.paths[i]) != 0)
      TestFail("temp theme file cannot be removed");
  }
  UnloadDirectoryFiles(files);
  MakeTempDir();
}

// 按字节原样写入一份 .cfg。不补换行、不做行尾转换，用例自己掌握每一份
// 输入的确切字节——BOM 与 CRLF 只有这样才写得出来。
static void WriteThemeBytes(const char* name, const unsigned char* prefix,
                            int prefixLength, const char* content) {
  char path[256];
  snprintf(path, sizeof(path), "%s/%s.cfg", THEME_DIR, name);

  FILE* file = fopen(path, "wb");
  if (file == NULL) {
    TestFail("temp theme file cannot be created");
    return;
  }
  if (prefix != NULL && prefixLength > 0)
    fwrite(prefix, 1, (size_t)prefixLength, file);
  if (content != NULL) fwrite(content, 1, strlen(content), file);
  fclose(file);
}

static void WriteTheme(const char* name, const char* content) {
  WriteThemeBytes(name, NULL, 0, content);
}

static void WriteThemeBom(const char* name, const char* content) {
  static const unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
  WriteThemeBytes(name, bom, 3, content);
}

// === 每条用例的前置复位 ===
//
// gUiStyle 与主题列表都是进程级状态。LoadTheme 失败时不会改动 gUiStyle，
// 所以「失败后样式保持调用前的值」这类断言要求用例自己把前置状态设成
// 一个明确的、能与默认配色区分开的哨兵值。
static void BeginCase(const char* name) {
  TestBeginCase(name);
  TestLogReset();
  ClearTempDir();
  gUiStyle = kDefaultUiStyle;
}

// 6 位十六进制按不透明处理，落地时补 0xFF 高位。
static Color Rgba(unsigned int rgb) {
  Color color = {(unsigned char)((rgb >> 16) & 0xFF),
                 (unsigned char)((rgb >> 8) & 0xFF),
                 (unsigned char)(rgb & 0xFF), 0xFF};
  return color;
}

// === 用例 ===

// BOM 被跳过后第一行就是普通的 key = value。若跳过逻辑没了，第一行的键名
// 会带上 BOM 的三个字节，既匹配不到任何键也会被判成未知键。
static void CaseUtf8BomFirstLineApplies(void) {
  BeginCase("first line after a UTF-8 BOM still applies");
  WriteThemeBom("bom", "text = 0x112233\nline = 0x445566\n");

  TestCheck(LoadTheme("bom"), "LoadTheme(\"bom\") returns true");
  TestCheckColor(gUiStyle.text, Rgba(0x112233),
                 "text on the first line after the BOM applies");
  TestCheckColor(gUiStyle.line, Rgba(0x445566),
                 "line on the second line applies");
  TestCheck(TestLogMissingWarn("unknown key"),
            "no 'unknown key' warning (the BOM did not stick to the key name)");
  TestCheckInt(TestLogWarnCount(), 0, "the whole file produced no warning");
  TestEndCase();
}

// 行结束规则同时认 \n 与 \r。若只认 \n，\r 会留在值末尾让十六进制解析失败。
static void CaseCrlfAppliesWholeFile(void) {
  BeginCase(
      "CRLF line endings: the whole file applies with no unparsable line");
  WriteTheme("crlf",
             "# 注释行\r\n"
             "text = 0x010203\r\n"
             "lineDim = 0x040506\r\n"
             "border_style = single\r\n");

  TestCheck(LoadTheme("crlf"), "LoadTheme(\"crlf\") returns true");
  TestCheckColor(gUiStyle.text, Rgba(0x010203),
                 "text in the CRLF file applies");
  TestCheckColor(gUiStyle.lineDim, Rgba(0x040506),
                 "lineDim in the CRLF file applies");
  TestCheckInt((int)gUiStyle.borderStyle, (int)UI_BORDER_SINGLE,
               "enum key border_style in the CRLF file applies");
  TestCheck(TestLogMissingWarn("failed to parse"),
            "no 'failed to parse' warning");
  TestCheckInt(TestLogWarnCount(), 0, "the whole file produced no warning");
  TestEndCase();
}

// 写成 text = "" 时取值非法。不假定解析器会剥引号，只钉住实际契约：
// 该行告警且不生效，同文件的其它行不受牵连。
static void CaseEmptyQuotedValueRejected(void) {
  BeginCase(
      "empty quoted value is invalid: warns, does not apply, spares other "
      "lines");
  WriteTheme("emptyval", "text = \"\"\nline = 0x0a0b0c\n");

  TestCheck(LoadTheme("emptyval"),
            "a valid key in the same file makes LoadTheme return true");
  TestCheckColor(gUiStyle.line, Rgba(0x0a0b0c),
                 "line in the same file still applies");
  TestCheckColor(gUiStyle.text, kDefaultUiStyle.text,
                 "text stays at the default color");
  TestCheck(TestLogHasWarn("has an invalid value"),
            "'has an invalid value' warning");
  TestCheck(TestLogHasWarn("\"\""),
            "the warning echoes the original value \"\"");
  TestCheckInt(TestLogWarnCount(), 1, "only the empty-quoted line warns");
  TestEndCase();
}

// 只接受 6 位或 8 位十六进制：非十六进制字符、3 位、5 位都要被拒。
static void CaseBadHexRejected(void) {
  BeginCase("three bad hex spellings: warn, do not apply, spare other lines");
  WriteTheme("badhex",
             "text = 0xGGGGGG\n"
             "line = 0x12345\n"
             "background = 123\n"
             "titleBar = 0x0a0b0c\n");

  TestCheck(LoadTheme("badhex"),
            "a valid key in the same file makes LoadTheme return true");
  TestCheckColor(gUiStyle.titleBar, Rgba(0x0a0b0c), "titleBar still applies");
  TestCheckColor(gUiStyle.text, kDefaultUiStyle.text,
                 "0xGGGGGG does not apply");
  TestCheckColor(gUiStyle.line, kDefaultUiStyle.line, "0x12345 does not apply");
  TestCheckColor(gUiStyle.background, kDefaultUiStyle.background,
                 "decimal 123 does not apply");
  TestCheck(TestLogHasWarn("invalid value: 123"),
            "warning echoes the decimal 123");
  TestCheck(TestLogHasWarn("0xGGGGGG"), "the warning echoes 0xGGGGGG");
  TestCheck(TestLogHasWarn("0x12345"), "the warning echoes 0x12345");
  TestCheckInt(TestLogWarnCount(), 3, "each of the three bad lines warns once");
  TestEndCase();
}

static void CaseUnknownKeyRejected(void) {
  BeginCase("unknown key: warns and does not apply");
  WriteTheme("unknown", "nosuchkey = 0xff00ffff\ntext = 0x0d0e0f\n");

  TestCheck(LoadTheme("unknown"),
            "a valid key in the same file makes LoadTheme return true");
  TestCheckColor(gUiStyle.text, Rgba(0x0d0e0f),
                 "text in the same file applies");
  TestCheck(TestLogHasWarn("unknown key"), "'unknown key' warning appeared");
  TestCheck(TestLogHasWarn("nosuchkey"), "the warning names the unknown key");
  TestCheckInt(TestLogWarnCount(), 1, "only the unknown-key line warns");
  TestEndCase();
}

// 字体度量与边框宽度由程序独占，写了必须被忽略并告警。
static void CaseForbiddenKeysRejected(void) {
  BeginCase(
      "program-owned keys: warn and do not apply, valid keys still apply");
  WriteTheme("forbidden",
             "text_size = 20\nborder_width = 5\nline = 0x0a0b0c\n");

  TestCheck(LoadTheme("forbidden"),
            "a valid key in the same file makes LoadTheme return true");
  TestCheckColor(gUiStyle.line, Rgba(0x0a0b0c),
                 "line in the same file applies");
  TestCheck(TestLogHasWarn("owned by the program"),
            "'owned by the program' warning");
  TestCheck(TestLogHasWarn("textsize"),
            "the warning names the normalized textsize");
  TestCheck(TestLogHasWarn("borderwidth"),
            "the warning names the normalized borderwidth");
  TestCheckInt(TestLogWarnCount(), 2,
               "each of the two program-owned keys warns once");
  TestEndCase();
}

static void CaseEnumKeysApply(void) {
  BeginCase("valid enum identifiers apply");
  WriteTheme("enumok", "border_style = triple\nshadow_style = solid\n");

  TestCheck(LoadTheme("enumok"), "LoadTheme(\"enumok\") returns true");
  TestCheckInt((int)gUiStyle.borderStyle, (int)UI_BORDER_TRIPLE,
               "border_style = triple applies");
  TestCheckInt((int)gUiStyle.shadowStyle, (int)UI_SHADOW_SOLID,
               "shadow_style = solid applies");
  TestCheckInt(TestLogWarnCount(), 0, "no warning was produced");
  TestEndCase();
}

// 只含枚举键的文件必须被判定为有效。若 applied 计数漏了枚举分支，
// applied 会是 0，整份文件被当成无效而丢弃。
static void CaseEnumOnlyFileApplies(void) {
  BeginCase(
      "a cfg with only enum keys applies: applied count must include enum "
      "keys");
  WriteTheme("enumbody", "border_style = single\n");

  TestCheck(LoadTheme("enumbody"), "LoadTheme(\"enumbody\") returns true");
  TestCheck(TestLogMissingWarn("no valid color"),
            "no 'no valid color or decoration settings' warning");
  TestCheckInt((int)gUiStyle.borderStyle, (int)UI_BORDER_SINGLE,
               "gUiStyle.borderStyle becomes UI_BORDER_SINGLE");
  TestCheckInt(TestLogWarnCount(), 0, "no warning was produced");
  TestEndCase();
}

// 枚举取值按标识符原样比对，不做大小写归一，因此 TRIPLE 与 hatch 都是
// 未知值。auto 没有命中时不会被定死，仍按默认桌面色落成实心。
static void CaseEnumUnknownValueRejected(void) {
  BeginCase(
      "unknown enum identifiers (uppercase and invented) are invalid: warn and "
      "do not apply");
  WriteTheme("enumbad",
             "border_style = TRIPLE\n"
             "shadow_style = hatch\n"
             "line = 0x0a0b0c\n");

  TestCheck(LoadTheme("enumbad"),
            "a valid key in the same file makes LoadTheme return true");
  TestCheckColor(gUiStyle.line, Rgba(0x0a0b0c),
                 "line in the same file applies");
  TestCheckInt((int)gUiStyle.borderStyle, (int)UI_BORDER_NONE,
               "uppercase TRIPLE does not match border_style");
  TestCheckInt((int)gUiStyle.shadowStyle, (int)UI_SHADOW_SOLID,
               "hatch does not match shadow_style (auto on a dark desktop "
               "resolves to solid)");
  TestCheck(TestLogHasWarn("invalid value: TRIPLE"), "warning echoes TRIPLE");
  TestCheck(TestLogHasWarn("invalid value: hatch"), "warning echoes hatch");
  TestCheckInt(TestLogWarnCount(), 2,
               "each of the two bad enum values warns once");
  TestEndCase();
}

// 缺 default.cfg 时退回列表首项，且退回的是排序后的首项而非目录返回的
// 第一个，因此同一份目录每次启动的结果一致。
static void CaseMissingDefaultFallsBackDeterministically(void) {
  BeginCase(
      "missing default.cfg falls back deterministically to the first sorted "
      "theme");

  WriteTheme("zzz", "text = 0x0a0b0c\n");
  InitTheme();
  TestCheckStr(GetCurrentThemeName(), "zzz",
               "the first InitTheme falls back to zzz");
  TestCheckInt(GetThemeCount(), 1, "the theme list has one entry");
  TestCheckStr(GetThemeName(0), "zzz", "the first list entry is zzz");
  TestCheckColor(gUiStyle.text, Rgba(0x0a0b0c), "text from zzz applies");
  TestCheck(
      TestLogHasWarn("failed to read theme file"),
      "missing default.cfg triggered the 'failed to read theme file' warning");

  ClearTempDir();
  WriteTheme("zzz", "text = 0x0a0b0c\n");
  InitTheme();
  TestCheckStr(GetCurrentThemeName(), "zzz",
               "repeating it gives the same fallback");

  ClearTempDir();
  WriteTheme("zzz", "text = 0x0a0b0c\n");
  WriteTheme("aaa", "text = 0x0d0e0f\n");
  InitTheme();
  TestCheckInt(GetThemeCount(), 2, "both themes are enumerated");
  TestCheckStr(GetThemeName(0), "aaa", "the list is sorted by name");
  TestCheckStr(GetCurrentThemeName(), "aaa",
               "missing default.cfg falls back to the first sorted entry aaa");
  TestEndCase();
}

// 空目录没有任何 .cfg：告警并保留内置默认配色。这里分两步：先用哨兵值
// 确认 InitTheme 根本没有改动 gUiStyle，再回到默认配色确认对外可见的结果。
static void CaseEmptyThemeDirKeepsBuiltinStyle(void) {
  BeginCase("empty directory: warns and uses the built-in default colors");

  ClearTempDir();
  UiStyle sentinel = kDefaultUiStyle;
  sentinel.text = Rgba(0x010203);
  gUiStyle = sentinel;
  InitTheme();
  TestCheckInt(GetThemeCount(), 0, "the theme list is empty");
  TestCheck(TestLogHasWarn("no theme file found"),
            "'no theme file found' warning");
  TestCheckStyle(&gUiStyle, &sentinel, "InitTheme did not modify gUiStyle");

  gUiStyle = kDefaultUiStyle;
  InitTheme();
  TestCheckStyle(&gUiStyle, &kDefaultUiStyle,
                 "gUiStyle equals the built-in default colors");
  TestEndCase();
}

// 整份文件没有一行有效设置：整体放弃，绝不留下半套配色。
static void CaseAllInvalidFileKeepsStyle(void) {
  BeginCase(
      "no valid line in the whole file: LoadTheme fails and the style is "
      "untouched");

  UiStyle sentinel = kDefaultUiStyle;
  sentinel.text = Rgba(0x010203);
  sentinel.borderStyle = UI_BORDER_SINGLE;
  gUiStyle = sentinel;

  WriteTheme("garbage",
             "# 只有注释的一行\n"
             "; 还有一行分号注释\n"
             "nosuchkey = 0xff00ffff\n"
             "text_size = 20\n"
             "text = 0xZZZZZZ\n");

  TestCheck(!LoadTheme("garbage"), "LoadTheme(\"garbage\") returns false");
  TestCheck(TestLogHasWarn("no valid color"),
            "'no valid color or decoration settings' warning appeared");
  TestCheckStyle(
      &gUiStyle, &sentinel,
      "gUiStyle keeps its pre-call value, no half-applied colors left");
  TestEndCase();
}

// auto 必须在写回 gUiStyle 之前按桌面亮度定死：暗底实心、亮底抖动。
// 两份文件都不含 auto 之外的有效枚举，因此 applied 计数靠配色键兜底。
static void CaseAutoShadowResolvedByDesktopLuma(void) {
  BeginCase(
      "auto shadow resolved by desktop luminance: dark desktop solid, light "
      "desktop dither");

  WriteTheme("autodark", "desktop = 0x000000\nshadow_style = auto\n");
  TestCheck(LoadTheme("autodark"), "the dark theme loads");
  TestCheckInt((int)gUiStyle.shadowStyle, (int)UI_SHADOW_SOLID,
               "luminance 0 dark desktop pins auto to solid shadow");

  WriteTheme("autolight", "desktop = 0xffffff\nshadow_style = auto\n");
  TestCheck(LoadTheme("autolight"), "the light theme loads");
  TestCheckInt((int)gUiStyle.shadowStyle, (int)UI_SHADOW_DITHER,
               "luminance 255 light desktop pins auto to dither shadow");
  TestCheckInt(TestLogWarnCount(), 0, "neither file produced a warning");
  TestEndCase();
}

// 键名归一后连字符、下划线、驼峰三种写法等价。
static void CaseKeyNameNormalized(void) {
  BeginCase(
      "key name normalization: text-dim / text_dim / textDim are equivalent");

  static const char* const kSpellings[] = {"text-dim", "text_dim", "textDim"};
  static const char* const kFiles[] = {"norm1", "norm2", "norm3"};
  int count = (int)(sizeof(kSpellings) / sizeof(kSpellings[0]));

  for (int i = 0; i < count; i++) {
    char content[128];
    snprintf(content, sizeof(content), "%s = 0x0a0b0c\nline = 0x0d0e0f\n",
             kSpellings[i]);
    WriteTheme(kFiles[i], content);

    char what[128];
    snprintf(what, sizeof(what), "%s line accepted", kSpellings[i]);
    TestCheck(LoadTheme(kFiles[i]), what);

    snprintf(what, sizeof(what), "%s maps to textDim", kSpellings[i]);
    TestCheckColor(gUiStyle.textDim, Rgba(0x0a0b0c), what);
  }
  TestCheckInt(TestLogWarnCount(), 0, "none of the three spellings warns");
  TestEndCase();
}

int main(void) {
  // 只留告警级别：Info 级的加载日志不属于断言范围，同时能把 raylib 自身的
  // 初始化噪声压掉。断言输出走 stdout，不受这个等级影响。
  SetTraceLogLevel(LOG_WARNING);
  TestLogBegin();

  // THEME_DIR 是相对路径，先切到项目根目录，和游戏本体走同一条路径解析。
  InitAssetsDirectory();
  MakeTempDir();

  TestBegin("theme parser regression tests");
  CaseUtf8BomFirstLineApplies();
  CaseCrlfAppliesWholeFile();
  CaseEmptyQuotedValueRejected();
  CaseBadHexRejected();
  CaseUnknownKeyRejected();
  CaseForbiddenKeysRejected();
  CaseEnumKeysApply();
  CaseEnumOnlyFileApplies();
  CaseEnumUnknownValueRejected();
  CaseMissingDefaultFallsBackDeterministically();
  CaseEmptyThemeDirKeepsBuiltinStyle();
  CaseAllInvalidFileKeepsStyle();
  CaseAutoShadowResolvedByDesktopLuma();
  CaseKeyNameNormalized();

  int result = TestEnd();

  // 清掉临时目录里的 .cfg，不留任何残留。
  ClearTempDir();
  TestLogEnd();
  return result;
}