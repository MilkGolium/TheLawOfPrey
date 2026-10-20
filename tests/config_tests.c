#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "test_util.h"
#include "utils.h"

// 配置文件解析器的回归测试。
//
// 配置文件的位置由接口给出（ConfigSetPathOverride），因此测试在自己的
// 临时目录里造文件，不会往 assets/ 下留任何东西，游戏本体那边仍然只认
// 平台默认位置。

#define CONFIG_TMP_DIR "tests/tmp/config"
#define CONFIG_TMP_FILE CONFIG_TMP_DIR "/config.cfg"
#define CONFIG_TMP_MISSING CONFIG_TMP_DIR "/nosuchfile.cfg"

// 平台默认位置跟着 HOME 与 XDG_CONFIG_HOME 走。把它们指到临时目录下，
// 「默认位置没有配置文件」这条路径才是确定的，不受机器上是否已经存在
// 同名文件的影响。这个目录不会被创建。
#define CONFIG_TMP_HOME "tests/tmp/config_home"

// === 临时目录 ===

// raylib 6.0 的 MakeDirectory 会连父目录一起建。
static void MakeTempDir(void) {
  if (DirectoryExists(CONFIG_TMP_DIR)) return;
  MakeDirectory(CONFIG_TMP_DIR);
  if (!DirectoryExists(CONFIG_TMP_DIR))
    TestFail("the temp config directory cannot be created");
}

// 每条用例前清空临时目录：上一条用例留下的文件会被下一条读到，
// 不清干净就成了互相污染的根源。raylib 6.0 没有删文件的接口，
// 删除走标准库的 remove。
static void ClearTempDir(void) {
  FilePathList files = LoadDirectoryFiles(CONFIG_TMP_DIR);
  for (unsigned int i = 0; i < files.count; i++) {
    if (DirectoryExists(files.paths[i])) continue;
    if (remove(files.paths[i]) != 0)
      TestFail("the temp config file cannot be removed");
  }
  UnloadDirectoryFiles(files);
  MakeTempDir();
}

// 按字节原样写入配置文件。不补换行、不做行尾转换，用例自己掌握每一份
// 输入的确切字节——CRLF 只有这样才写得出来。
static void WriteConfigBytes(const char* path, const char* content) {
  FILE* file = fopen(path, "wb");
  if (file == NULL) {
    TestFail("the temp config file cannot be created");
    return;
  }
  if (content != NULL) fwrite(content, 1, strlen(content), file);
  fclose(file);
}

// === 每条用例的前置复位 ===

static void BeginCase(const char* name) {
  TestBeginCase(name);
  TestLogReset();
  ClearTempDir();
}

// 运行期用错键名是程序错误，要打 LOG_ERROR。测试框架只收告警，
// 因此这一条用例临时换成自己的回调来数错误条数。
static int sErrorCount = 0;

static void CountErrors(int level, const char* text, va_list args) {
  (void)text;
  (void)args;
  if (level == LOG_ERROR) sErrorCount++;
}

// === 用例 ===

// 平台默认位置没有配置文件：不算失败，四个键都是缺省值。
static void CaseMissingFileUsesDefaults(void) {
  BeginCase("missing config file: all defaults, ConfigLoad returns true");
  ConfigSetPathOverride(NULL);

  TestCheck(ConfigLoad(), "ConfigLoad returns true when the file is missing");
  TestCheck(ConfigGetBool("fullscreen"), "fullscreen is the default (true)");
  TestCheckInt(ConfigGetInt("target_fps"), 144, "target_fps is the default");
  TestCheckStr(ConfigGetString("theme"), "default", "theme is the default");
  TestCheckStr(ConfigGetString("lang"), "zh-hans", "lang is the default");
  TestEndCase();
}

// 正常文件：四个键都按文件里的取值。
static void CaseNormalFileApplies(void) {
  BeginCase("a normal config file: all four keys take the file's values");
  WriteConfigBytes(CONFIG_TMP_FILE,
                   "fullscreen = false\n"
                   "target_fps = 60\n"
                   "theme = dark\n"
                   "lang = en\n");

  ConfigSetPathOverride(CONFIG_TMP_FILE);
  TestCheck(ConfigLoad(), "ConfigLoad returns true");
  TestCheckStr(ConfigPath(), CONFIG_TMP_FILE, "the path is the override");
  TestCheck(!ConfigGetBool("fullscreen"), "fullscreen is false");
  TestCheckInt(ConfigGetInt("target_fps"), 60, "target_fps is 60");
  TestCheckStr(ConfigGetString("theme"), "dark", "theme is dark");
  TestCheckStr(ConfigGetString("lang"), "en", "lang is en");
  TestCheckInt(TestLogWarnCount(), 0, "no warning was produced");
  TestEndCase();
}

// 未知键：只忽略那一行，同文件里的其他行照常生效。
// 键名大小写敏感：Target_FPS 与 target_fps 是两个键，前者是未知键。
static void CaseUnknownKeyIgnored(void) {
  BeginCase("an unknown key: that line is ignored, the other lines apply");
  WriteConfigBytes(CONFIG_TMP_FILE,
                   "unknown_key = 1\n"
                   "Target_FPS = 90\n"
                   "target_fps = 60\n");

  ConfigSetPathOverride(CONFIG_TMP_FILE);
  TestCheck(ConfigLoad(), "ConfigLoad returns true");
  TestCheckInt(ConfigGetInt("target_fps"), 60, "target_fps still applies");
  TestCheck(TestLogHasWarn("unknown key"), "'unknown key' warning appeared");
  TestCheck(TestLogHasWarn("unknown_key"), "the warning names the unknown key");
  TestCheck(TestLogHasWarn("Target_FPS"),
            "a key spelled with a different case is an unknown key too");
  TestCheckInt(TestLogWarnCount(), 2,
               "the unknown-key and the wrong-case lines each warn once");
  TestEndCase();
}

// 坏 bool：只有这一个键退回缺省，同文件里的其他键不受牵连。
static void CaseBadBoolFallsBack(void) {
  BeginCase("a bad bool value: that key falls back to the default");
  WriteConfigBytes(CONFIG_TMP_FILE,
                   "fullscreen = yes\n"
                   "target_fps = 60\n");

  ConfigSetPathOverride(CONFIG_TMP_FILE);
  TestCheck(ConfigLoad(), "ConfigLoad returns true");
  TestCheck(ConfigGetBool("fullscreen"),
            "fullscreen falls back to the default (true)");
  TestCheckInt(ConfigGetInt("target_fps"), 60,
               "target_fps in the same file still applies");
  TestCheck(TestLogHasWarn("invalid value: yes"),
            "the warning echoes the bad value");
  TestCheckInt(TestLogWarnCount(), 1, "only the bad line warns");
  TestEndCase();
}

// 坏 int：非数字退回缺省。
static void CaseBadIntFallsBack(void) {
  BeginCase("a non-numeric int value: that key falls back to the default");
  WriteConfigBytes(CONFIG_TMP_FILE,
                   "target_fps = abc\n"
                   "theme = dark\n");

  ConfigSetPathOverride(CONFIG_TMP_FILE);
  TestCheck(ConfigLoad(), "ConfigLoad returns true");
  TestCheckInt(ConfigGetInt("target_fps"), 144,
               "target_fps falls back to the default");
  TestCheckStr(ConfigGetString("theme"), "dark",
               "theme in the same file still applies");
  TestCheck(TestLogHasWarn("invalid value: abc"),
            "the warning echoes the bad value");
  TestCheckInt(TestLogWarnCount(), 1, "only the bad line warns");
  TestEndCase();
}

// 越界 int：高于上限与低于下限都退回缺省。两个值分两次装载，
// 否则同一个键写两遍会先撞上「重复键」那条规则。
static void CaseOutOfRangeIntFallsBack(void) {
  BeginCase("out-of-range int values: that key falls back to the default");

  WriteConfigBytes(CONFIG_TMP_FILE, "target_fps = 1000\n");
  ConfigSetPathOverride(CONFIG_TMP_FILE);
  TestLogReset();
  TestCheck(ConfigLoad(), "ConfigLoad returns true for the over-high value");
  TestCheckInt(ConfigGetInt("target_fps"), 144,
               "target_fps = 1000 falls back to the default");
  TestCheckInt(TestLogWarnCount(), 1, "the over-high value warns once");

  WriteConfigBytes(CONFIG_TMP_FILE, "target_fps = 10\n");
  TestLogReset();
  TestCheck(ConfigLoad(), "ConfigLoad returns true for the over-low value");
  TestCheckInt(ConfigGetInt("target_fps"), 144,
               "target_fps = 10 falls back to the default");
  TestCheckInt(TestLogWarnCount(), 1, "the over-low value warns once");
  TestEndCase();
}

// 重复键：保留先出现的那一个。
static void CaseDuplicateKeyKeepsFirst(void) {
  BeginCase("a duplicate key: the first value is kept");
  WriteConfigBytes(CONFIG_TMP_FILE,
                   "target_fps = 60\n"
                   "target_fps = 120\n");

  ConfigSetPathOverride(CONFIG_TMP_FILE);
  TestCheck(ConfigLoad(), "ConfigLoad returns true");
  TestCheckInt(ConfigGetInt("target_fps"), 60, "the first value is kept");
  TestCheck(TestLogHasWarn("duplicate key"),
            "'duplicate key' warning appeared");
  TestCheckInt(TestLogWarnCount(), 1, "the duplicate line warns once");
  TestEndCase();
}

// 注释、空行、行尾空白与 CRLF 都要能正常解析。
static void CaseCommentsBlankLinesAndCrlf(void) {
  BeginCase(
      "comments, blank lines, trailing spaces and CRLF: all parse cleanly");
  WriteConfigBytes(CONFIG_TMP_FILE,
                   "# a comment line\r\n"
                   "\r\n"
                   "   \r\n"
                   "  target_fps = 75   \r\n"
                   "fullscreen = false\t\r\n"
                   "# theme = dark\r\n"
                   "lang = en  # an inline comment\r\n");

  ConfigSetPathOverride(CONFIG_TMP_FILE);
  TestCheck(ConfigLoad(), "ConfigLoad returns true");
  TestCheckInt(ConfigGetInt("target_fps"), 75, "target_fps applies");
  TestCheck(!ConfigGetBool("fullscreen"), "fullscreen applies");
  TestCheckStr(ConfigGetString("lang"), "en",
               "the inline comment is stripped from the value");
  TestCheckStr(ConfigGetString("theme"), "default",
               "the commented-out line does not apply");
  TestCheckInt(TestLogWarnCount(), 0, "no warning was produced");
  TestEndCase();
}

// 显式指定的路径不存在：硬错误，ConfigLoad 返回 false。
static void CaseMissingExplicitPathFails(void) {
  BeginCase("an explicit path that does not exist: ConfigLoad returns false");
  ConfigSetPathOverride(CONFIG_TMP_MISSING);

  TestCheck(!ConfigLoad(), "ConfigLoad returns false");
  TestEndCase();
}

// 运行期用错键名是程序错误：打 LOG_ERROR，并返回该类型的缺省值，
// 不能静默返回猜测值。
static void CaseUnknownKeyAtRuntime(void) {
  BeginCase("an unknown key queried at runtime: an error, not silence");
  ConfigSetPathOverride(NULL);
  ConfigLoad();

  TestLogEnd();
  sErrorCount = 0;
  SetTraceLogCallback(CountErrors);

  TestCheck(!ConfigGetBool("nosuchkey"),
            "an unknown boolean key returns false");
  TestCheckInt(ConfigGetInt("nosuchkey"), 0, "an unknown int key returns 0");
  TestCheckStr(ConfigGetString("nosuchkey"), "",
               "an unknown string key returns an empty string");

  TestLogBegin();
  TestCheckInt(sErrorCount, 3, "each unknown key query logs one error");
  TestEndCase();
}

// 权限探测：只读的文件读得到、写不了；文件不在时能不能写取决于目录。
static void CasePermissionProbe(void) {
  BeginCase(
      "permission probing: a read-only file reads fine but cannot be written");
  WriteConfigBytes(CONFIG_TMP_FILE, "target_fps = 60\n");
  ConfigSetPathOverride(CONFIG_TMP_FILE);
  ConfigLoad();

  // access(2) 对 root 一律判可写，因此这段断言只在普通用户下做
  if (geteuid() != 0) {
    chmod(CONFIG_TMP_FILE, 0444);
    TestCheck(!ConfigCanWrite(), "a read-only config file is not writable");
    TestCheck(ConfigLoad(), "the read-only file still loads");
    TestCheckInt(ConfigGetInt("target_fps"), 60, "the values still apply");
    chmod(CONFIG_TMP_FILE, 0644);
  }

  ConfigSetPathOverride(CONFIG_TMP_MISSING);
  ConfigLoad();
  TestCheck(ConfigCanWrite(),
            "a missing file in a writable directory can be created");
  TestEndCase();
}

// 本阶段程序只读配置：装载之后文件内容必须一字未改。
static void CaseLoadDoesNotWriteBack(void) {
  BeginCase("loading never writes the config file back");
  const char* content = "target_fps = 60\n";
  WriteConfigBytes(CONFIG_TMP_FILE, content);
  ConfigSetPathOverride(CONFIG_TMP_FILE);
  ConfigLoad();

  char readBack[256] = "";
  FILE* file = fopen(CONFIG_TMP_FILE, "rb");
  if (file == NULL) {
    TestFail("the temp config file cannot be reopened");
  } else {
    const size_t length = fread(readBack, 1, sizeof(readBack) - 1, file);
    readBack[length] = '\0';
    fclose(file);
  }
  TestCheckStr(readBack, content,
               "the file content is unchanged after loading");
  TestEndCase();
}

int main(void) {
  // 只留告警级别：Info 级的日志不属于断言范围，同时能把 raylib 自身的
  // 初始化噪声压掉。断言输出走 stdout，不受这个等级影响。
  SetTraceLogLevel(LOG_WARNING);
  TestLogBegin();

  // 相对路径先切到项目根目录，和游戏本体走同一条路径解析。
  InitAssetsDirectory();
  MakeTempDir();

  setenv("HOME", CONFIG_TMP_HOME, 1);
  unsetenv("XDG_CONFIG_HOME");

  TestBegin("config parser regression tests");
  CaseMissingFileUsesDefaults();
  CaseNormalFileApplies();
  CaseUnknownKeyIgnored();
  CaseBadBoolFallsBack();
  CaseBadIntFallsBack();
  CaseOutOfRangeIntFallsBack();
  CaseDuplicateKeyKeepsFirst();
  CaseCommentsBlankLinesAndCrlf();
  CaseMissingExplicitPathFails();
  CaseUnknownKeyAtRuntime();
  CasePermissionProbe();
  CaseLoadDoesNotWriteBack();

  int result = TestEnd();

  // 清掉临时目录里的文件，不留任何残留。
  ClearTempDir();
  TestLogEnd();
  return result;
}
