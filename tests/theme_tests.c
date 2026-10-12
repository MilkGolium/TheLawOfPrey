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
  if (!DirectoryExists(THEME_DIR)) TestFail("临时主题目录无法创建");
}

// 每条用例前清空临时目录：上一条用例留下的 .cfg 会被下一次 InitTheme
// 扫到，不清干净就成了互相污染的根源。raylib 6.0 没有删文件的接口，
// 删除走标准库的 remove。
static void ClearTempDir(void) {
  FilePathList files = LoadDirectoryFiles(THEME_DIR);
  for (unsigned int i = 0; i < files.count; i++) {
    if (!IsFileExtension(files.paths[i], ".cfg")) continue;
    if (remove(files.paths[i]) != 0) TestFail("临时主题文件无法删除");
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
    TestFail("临时主题文件无法创建");
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
  BeginCase("UTF-8 BOM 之后首行键照常生效");
  WriteThemeBom("bom", "text = 0x112233\nline = 0x445566\n");

  TestCheck(LoadTheme("bom"), "LoadTheme(\"bom\") 返回 true");
  TestCheckColor(gUiStyle.text, Rgba(0x112233), "BOM 后首行的 text 生效");
  TestCheckColor(gUiStyle.line, Rgba(0x445566), "第二行的 line 生效");
  TestCheck(TestLogMissingWarn("未知主题键"),
            "没有出现「未知主题键」告警（BOM 没有粘到键名上）");
  TestCheckInt(TestLogWarnCount(), 0, "整份文件没有产生任何告警");
  TestEndCase();
}

// 行结束规则同时认 \n 与 \r。若只认 \n，\r 会留在值末尾让十六进制解析失败。
static void CaseCrlfAppliesWholeFile(void) {
  BeginCase("CRLF 行结束：全文件生效且没有无法解析的行");
  WriteTheme("crlf",
             "# 注释行\r\n"
             "text = 0x010203\r\n"
             "lineDim = 0x040506\r\n"
             "border_style = single\r\n");

  TestCheck(LoadTheme("crlf"), "LoadTheme(\"crlf\") 返回 true");
  TestCheckColor(gUiStyle.text, Rgba(0x010203), "CRLF 文件的 text 生效");
  TestCheckColor(gUiStyle.lineDim, Rgba(0x040506), "CRLF 文件的 lineDim 生效");
  TestCheckInt((int)gUiStyle.borderStyle, (int)UI_BORDER_SINGLE,
               "CRLF 文件的枚举键 border_style 生效");
  TestCheck(TestLogMissingWarn("无法解析的行"), "没有出现「无法解析的行」告警");
  TestCheckInt(TestLogWarnCount(), 0, "整份文件没有产生任何告警");
  TestEndCase();
}

// 写成 text = "" 时取值非法。不假定解析器会剥引号，只钉住实际契约：
// 该行告警且不生效，同文件的其它行不受牵连。
static void CaseEmptyQuotedValueRejected(void) {
  BeginCase("空引号取值判非法：告警、不生效、不牵连其它行");
  WriteTheme("emptyval", "text = \"\"\nline = 0x0a0b0c\n");

  TestCheck(LoadTheme("emptyval"), "同文件的合法键让 LoadTheme 返回 true");
  TestCheckColor(gUiStyle.line, Rgba(0x0a0b0c), "同文件的 line 仍生效");
  TestCheckColor(gUiStyle.text, kDefaultUiStyle.text, "text 保持默认色");
  TestCheck(TestLogHasWarn("取值非法"), "出现「取值非法」告警");
  TestCheck(TestLogHasWarn("\"\""), "告警回显的是原始取值 \"\"");
  TestCheckInt(TestLogWarnCount(), 1, "只有空引号这一行告警");
  TestEndCase();
}

// 只接受 6 位或 8 位十六进制：非十六进制字符、3 位、5 位都要被拒。
static void CaseBadHexRejected(void) {
  BeginCase("坏十六进制三种写法：告警、不生效、不牵连其它行");
  WriteTheme("badhex",
             "text = 0xGGGGGG\n"
             "line = 0x12345\n"
             "background = 123\n"
             "titleBar = 0x0a0b0c\n");

  TestCheck(LoadTheme("badhex"), "同文件的合法键让 LoadTheme 返回 true");
  TestCheckColor(gUiStyle.titleBar, Rgba(0x0a0b0c), "titleBar 仍生效");
  TestCheckColor(gUiStyle.text, kDefaultUiStyle.text, "0xGGGGGG 未生效");
  TestCheckColor(gUiStyle.line, kDefaultUiStyle.line, "0x12345 未生效");
  TestCheckColor(gUiStyle.background, kDefaultUiStyle.background,
                 "十进制 123 未生效");
  TestCheck(TestLogHasWarn("0xGGGGGG"), "告警回显了 0xGGGGGG");
  TestCheck(TestLogHasWarn("0x12345"), "告警回显了 0x12345");
  TestCheck(TestLogHasWarn("取值非法：123"), "告警回显了十进制的 123");
  TestCheckInt(TestLogWarnCount(), 3, "三条坏行各自告警一次");
  TestEndCase();
}

static void CaseUnknownKeyRejected(void) {
  BeginCase("未知键：告警且不生效");
  WriteTheme("unknown", "nosuchkey = 0xff00ffff\ntext = 0x0d0e0f\n");

  TestCheck(LoadTheme("unknown"), "同文件的合法键让 LoadTheme 返回 true");
  TestCheckColor(gUiStyle.text, Rgba(0x0d0e0f), "同文件的 text 生效");
  TestCheck(TestLogHasWarn("未知主题键"), "出现「未知主题键」告警");
  TestCheck(TestLogHasWarn("nosuchkey"), "告警点名了未知键");
  TestCheckInt(TestLogWarnCount(), 1, "只有未知键这一行告警");
  TestEndCase();
}

// 字体度量与边框宽度由程序独占，写了必须被忽略并告警。
static void CaseForbiddenKeysRejected(void) {
  BeginCase("程序独占键：告警且不生效，同文件合法键仍生效");
  WriteTheme("forbidden",
             "text_size = 20\nborder_width = 5\nline = 0x0a0b0c\n");

  TestCheck(LoadTheme("forbidden"), "同文件的合法键让 LoadTheme 返回 true");
  TestCheckColor(gUiStyle.line, Rgba(0x0a0b0c), "同文件的 line 生效");
  TestCheck(TestLogHasWarn("由程序独占"), "出现「由程序独占」告警");
  TestCheck(TestLogHasWarn("textsize"), "告警点名了归一后的 textsize");
  TestCheck(TestLogHasWarn("borderwidth"), "告警点名了归一后的 borderwidth");
  TestCheckInt(TestLogWarnCount(), 2, "两个独占键各告警一次");
  TestEndCase();
}

static void CaseEnumKeysApply(void) {
  BeginCase("枚举键合法标识符生效");
  WriteTheme("enumok", "border_style = triple\nshadow_style = solid\n");

  TestCheck(LoadTheme("enumok"), "LoadTheme(\"enumok\") 返回 true");
  TestCheckInt((int)gUiStyle.borderStyle, (int)UI_BORDER_TRIPLE,
               "border_style = triple 生效");
  TestCheckInt((int)gUiStyle.shadowStyle, (int)UI_SHADOW_SOLID,
               "shadow_style = solid 生效");
  TestCheckInt(TestLogWarnCount(), 0, "没有产生任何告警");
  TestEndCase();
}

// 只含枚举键的文件必须被判定为有效。若 applied 计数漏了枚举分支，
// applied 会是 0，整份文件被当成无效而丢弃。
static void CaseEnumOnlyFileApplies(void) {
  BeginCase("只含枚举键的 cfg 生效：applied 计数必须计入枚举键");
  WriteTheme("enumbody", "border_style = single\n");

  TestCheck(LoadTheme("enumbody"), "LoadTheme(\"enumbody\") 返回 true");
  TestCheck(TestLogMissingWarn("未包含任何有效配色"),
            "没有出现「未包含任何有效配色或装饰设置」告警");
  TestCheckInt((int)gUiStyle.borderStyle, (int)UI_BORDER_SINGLE,
               "gUiStyle.borderStyle 变成 UI_BORDER_SINGLE");
  TestCheckInt(TestLogWarnCount(), 0, "没有产生任何告警");
  TestEndCase();
}

// 枚举取值按标识符原样比对，不做大小写归一，因此 TRIPLE 与 hatch 都是
// 未知值。auto 没有命中时不会被定死，仍按默认桌面色落成实心。
static void CaseEnumUnknownValueRejected(void) {
  BeginCase("枚举未知标识符（大写与自造值）判非法：告警且不生效");
  WriteTheme("enumbad",
             "border_style = TRIPLE\n"
             "shadow_style = hatch\n"
             "line = 0x0a0b0c\n");

  TestCheck(LoadTheme("enumbad"), "同文件的合法键让 LoadTheme 返回 true");
  TestCheckColor(gUiStyle.line, Rgba(0x0a0b0c), "同文件的 line 生效");
  TestCheckInt((int)gUiStyle.borderStyle, (int)UI_BORDER_NONE,
               "大写的 TRIPLE 没有命中 border_style");
  TestCheckInt((int)gUiStyle.shadowStyle, (int)UI_SHADOW_SOLID,
               "hatch 没有命中 shadow_style（默认暗底 auto 定死为实心）");
  TestCheck(TestLogHasWarn("取值非法：TRIPLE"), "告警回显了 TRIPLE");
  TestCheck(TestLogHasWarn("取值非法：hatch"), "告警回显了 hatch");
  TestCheckInt(TestLogWarnCount(), 2, "两个坏枚举值各告警一次");
  TestEndCase();
}

// 缺 default.cfg 时退回列表首项，且退回的是排序后的首项而非目录返回的
// 第一个，因此同一份目录每次启动的结果一致。
static void CaseMissingDefaultFallsBackDeterministically(void) {
  BeginCase("缺 default.cfg 时确定性回退到排序后的首个主题");

  WriteTheme("zzz", "text = 0x0a0b0c\n");
  InitTheme();
  TestCheckStr(GetCurrentThemeName(), "zzz", "第一次 InitTheme 回退到 zzz");
  TestCheckInt(GetThemeCount(), 1, "主题列表只有一项");
  TestCheckStr(GetThemeName(0), "zzz", "列表首项是 zzz");
  TestCheckColor(gUiStyle.text, Rgba(0x0a0b0c), "zzz 的 text 生效");
  TestCheck(TestLogHasWarn("无法读取主题文件"),
            "缺 default.cfg 触发了「无法读取主题文件」告警");

  ClearTempDir();
  WriteTheme("zzz", "text = 0x0a0b0c\n");
  InitTheme();
  TestCheckStr(GetCurrentThemeName(), "zzz", "重复执行回退结果一致");

  ClearTempDir();
  WriteTheme("zzz", "text = 0x0a0b0c\n");
  WriteTheme("aaa", "text = 0x0d0e0f\n");
  InitTheme();
  TestCheckInt(GetThemeCount(), 2, "两个主题都被枚举到");
  TestCheckStr(GetThemeName(0), "aaa", "列表按名称排序");
  TestCheckStr(GetCurrentThemeName(), "aaa",
               "缺 default.cfg 时回退到排序后的首项 aaa");
  TestEndCase();
}

// 空目录没有任何 .cfg：告警并保留内置默认配色。这里分两步：先用哨兵值
// 确认 InitTheme 根本没有改动 gUiStyle，再回到默认配色确认对外可见的结果。
static void CaseEmptyThemeDirKeepsBuiltinStyle(void) {
  BeginCase("空目录：告警并使用内置默认配色");

  ClearTempDir();
  UiStyle sentinel = kDefaultUiStyle;
  sentinel.text = Rgba(0x010203);
  gUiStyle = sentinel;
  InitTheme();
  TestCheckInt(GetThemeCount(), 0, "主题列表为空");
  TestCheck(TestLogHasWarn("没有找到主题文件"), "出现「没有找到主题文件」告警");
  TestCheckStyle(&gUiStyle, &sentinel, "InitTheme 没有改动 gUiStyle");

  gUiStyle = kDefaultUiStyle;
  InitTheme();
  TestCheckStyle(&gUiStyle, &kDefaultUiStyle, "gUiStyle 与内置默认配色相等");
  TestEndCase();
}

// 整份文件没有一行有效设置：整体放弃，绝不留下半套配色。
static void CaseAllInvalidFileKeepsStyle(void) {
  BeginCase("整份文件无有效行：LoadTheme 失败且样式原样不动");

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

  TestCheck(!LoadTheme("garbage"), "LoadTheme(\"garbage\") 返回 false");
  TestCheck(TestLogHasWarn("未包含任何有效配色"),
            "出现「未包含任何有效配色或装饰设置」告警");
  TestCheckStyle(&gUiStyle, &sentinel,
                 "gUiStyle 保持调用前的值，没有留下半套配色");
  TestEndCase();
}

// auto 必须在写回 gUiStyle 之前按桌面亮度定死：暗底实心、亮底抖动。
// 两份文件都不含 auto 之外的有效枚举，因此 applied 计数靠配色键兜底。
static void CaseAutoShadowResolvedByDesktopLuma(void) {
  BeginCase("auto 阴影按桌面亮度定死：暗底实心、亮底抖动");

  WriteTheme("autodark", "desktop = 0x000000\nshadow_style = auto\n");
  TestCheck(LoadTheme("autodark"), "暗底主题加载成功");
  TestCheckInt((int)gUiStyle.shadowStyle, (int)UI_SHADOW_SOLID,
               "亮度 0 的暗底把 auto 定死为实心阴影");

  WriteTheme("autolight", "desktop = 0xffffff\nshadow_style = auto\n");
  TestCheck(LoadTheme("autolight"), "亮底主题加载成功");
  TestCheckInt((int)gUiStyle.shadowStyle, (int)UI_SHADOW_DITHER,
               "亮度 255 的亮底把 auto 定死为抖动阴影");
  TestCheckInt(TestLogWarnCount(), 0, "两份文件都没有产生告警");
  TestEndCase();
}

// 键名归一后连字符、下划线、驼峰三种写法等价。
static void CaseKeyNameNormalized(void) {
  BeginCase("键名归一：text-dim / text_dim / textDim 三种写法等价");

  static const char* const kSpellings[] = {"text-dim", "text_dim", "textDim"};
  static const char* const kFiles[] = {"norm1", "norm2", "norm3"};
  int count = (int)(sizeof(kSpellings) / sizeof(kSpellings[0]));

  for (int i = 0; i < count; i++) {
    char content[128];
    snprintf(content, sizeof(content), "%s = 0x0a0b0c\nline = 0x0d0e0f\n",
             kSpellings[i]);
    WriteTheme(kFiles[i], content);

    char what[128];
    snprintf(what, sizeof(what), "%s 这一行被接受", kSpellings[i]);
    TestCheck(LoadTheme(kFiles[i]), what);

    snprintf(what, sizeof(what), "%s 命中 textDim", kSpellings[i]);
    TestCheckColor(gUiStyle.textDim, Rgba(0x0a0b0c), what);
  }
  TestCheckInt(TestLogWarnCount(), 0, "三种写法都没有告警");
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

  TestBegin("主题解析器回归测试");
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