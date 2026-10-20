#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "raylib.h"

// === 键表 ===
//
// 键名、类型、默认值与合法范围只在这张表里登记一份。设置界面上线后由
// 同一张表生成控件，因此界面与文件格式不会分叉。
//
// 键名按原样逐字符比较，大小写敏感：这里不套用主题解析器的
// NormalizeKey（那是主题键的规则，见 src/theme.c）。

#define CONFIG_APP_DIR "TheLawOfPrey"
#define CONFIG_FILE_NAME "config.cfg"
#define CONFIG_FALLBACK_DIR "TheLawOfPrey"

#define CONFIG_PATH_MAX 1024
#define CONFIG_DIR_MAX 1024
#define CONFIG_LINE_MAX 1024
#define CONFIG_STRING_MAX 64

typedef enum {
  CONFIG_TYPE_BOOL,
  CONFIG_TYPE_INT,
  CONFIG_TYPE_STRING,
} ConfigValueType;

typedef struct {
  const char* key;
  ConfigValueType type;
  bool boolDefault;
  int intDefault;
  const char* stringDefault;
  int intMin;  // 仅 int 类型有意义
  int intMax;  // 仅 int 类型有意义
} ConfigKeyDef;

static const ConfigKeyDef kConfigKeys[] = {
    {"fullscreen", CONFIG_TYPE_BOOL, true, 0, NULL, 0, 0},
    {"target_fps", CONFIG_TYPE_INT, false, 144, NULL, 30, 240},
    {"theme", CONFIG_TYPE_STRING, false, 0, "default", 0, 0},
    {"lang", CONFIG_TYPE_STRING, false, 0, "zh-hans", 0, 0},
};

#define CONFIG_KEY_COUNT ((int)(sizeof(kConfigKeys) / sizeof(kConfigKeys[0])))

// === 状态 ===

typedef struct {
  bool boolValue;
  int intValue;
  char stringValue[CONFIG_STRING_MAX];
} ConfigValue;

static ConfigValue sValues[CONFIG_KEY_COUNT];

// 每个键是否已经在文件里出现过。重复的键保留先出现的那一条。
static bool sSeen[CONFIG_KEY_COUNT];

// 值是否已经初始化到缺省。加载之前就取值的场合（例如将来设置界面
// 在读盘前先显示缺省）要拿到缺省而不是零值。
static bool sValuesReady = false;

static char sPath[CONFIG_PATH_MAX] = "";
static const char* sPathOverride = NULL;

// === 文本工具 ===

// 复制字符串，超长时截断。用手写拷贝而不是 snprintf：拷贝到固定长度的
// 缓冲区时 snprintf 的截断行为会触发格式截断告警。
static void CopyString(char* out, size_t outSize, const char* text) {
  size_t length = strlen(text);
  if (length >= outSize) length = outSize - 1;
  memcpy(out, text, length);
  out[length] = '\0';
}

// 去掉首尾空白。空白含空格、制表符与回车换行，因此 CRLF 文件不需要
// 另外处理行结束符。
static char* Trim(char* text) {
  while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
    text++;
  }
  size_t length = strlen(text);
  while (length > 0) {
    const char c = text[length - 1];
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
    text[--length] = '\0';
  }
  return text;
}

// 取路径里的目录部分。路径里没有 '/' 时用当前目录。
static void ExtractDir(const char* path, char* out, size_t outSize) {
  const char* slash = strrchr(path, '/');
  if (slash == NULL) {
    CopyString(out, outSize, ".");
    return;
  }
  if (slash == path) {
    CopyString(out, outSize, "/");
    return;
  }
  const size_t length = (size_t)(slash - path);
  if (length >= outSize) {
    CopyString(out, outSize, "");
    return;
  }
  memcpy(out, path, length);
  out[length] = '\0';
}

// === 查表 ===

// 键名逐字符比较，大小写敏感。
static int FindKeyIndex(const char* key) {
  if (key == NULL) return -1;
  for (int i = 0; i < CONFIG_KEY_COUNT; i++) {
    if (strcmp(key, kConfigKeys[i].key) == 0) return i;
  }
  return -1;
}

static void ResetToDefaults(void) {
  for (int i = 0; i < CONFIG_KEY_COUNT; i++) {
    const ConfigKeyDef* def = &kConfigKeys[i];
    sValues[i].boolValue = def->boolDefault;
    sValues[i].intValue = def->intDefault;
    if (def->stringDefault != NULL) {
      CopyString(sValues[i].stringValue, sizeof(sValues[i].stringValue),
                 def->stringDefault);
    } else {
      sValues[i].stringValue[0] = '\0';
    }
    sSeen[i] = false;
  }
  sValuesReady = true;
}

static void EnsureReady(void) {
  if (!sValuesReady) ResetToDefaults();
}

// === 解析 ===

// 只接受十进制整数串，且值须落在键表给的范围内。返回 false 表示非法。
static bool ParseIntValue(const char* text, int min, int max, int* out) {
  if (text[0] == '\0') return false;
  errno = 0;
  char* end = NULL;
  const long parsed = strtol(text, &end, 10);
  if (errno != 0 || end == text || *end != '\0') return false;
  if (parsed < (long)min || parsed > (long)max) return false;
  *out = (int)parsed;
  return true;
}

// 处理一行：去注释、去空白、按 key = value 拆开，落进键表。
// 任何一条坏行只影响它自己：告警之后继续处理后面的行。
static void ProcessLine(const char* raw) {
  char buffer[CONFIG_LINE_MAX];
  CopyString(buffer, sizeof(buffer), raw);

  // '#' 起注释。行内的 '#' 之后同样整段忽略，因此值后面可以跟注释。
  char* comment = strchr(buffer, '#');
  if (comment != NULL) *comment = '\0';

  char* text = Trim(buffer);
  if (text[0] == '\0') return;  // 空行与纯注释行

  char* separator = strchr(text, '=');
  if (separator == NULL) {
    TraceLog(LOG_WARNING, "Config: %s has a line without '=': %s", sPath, text);
    return;
  }
  *separator = '\0';
  char* key = Trim(text);
  char* value = Trim(separator + 1);

  const int index = FindKeyIndex(key);
  if (index < 0) {
    TraceLog(LOG_WARNING, "Config: %s has an unknown key: %s, ignored", sPath,
             key);
    return;
  }

  // 重复的键保留先出现的那一条，坏值也不例外：先出现的那条说了算。
  if (sSeen[index]) {
    TraceLog(LOG_WARNING,
             "Config: %s has a duplicate key: %s, the first value is kept",
             sPath, key);
    return;
  }
  sSeen[index] = true;

  const ConfigKeyDef* def = &kConfigKeys[index];
  switch (def->type) {
    case CONFIG_TYPE_BOOL: {
      if (strcmp(value, "true") == 0) {
        sValues[index].boolValue = true;
      } else if (strcmp(value, "false") == 0) {
        sValues[index].boolValue = false;
      } else {
        TraceLog(LOG_WARNING,
                 "Config: %s: %s has an invalid value: %s, the default is kept",
                 sPath, key, value);
      }
      break;
    }
    case CONFIG_TYPE_INT: {
      int parsed = 0;
      if (!ParseIntValue(value, def->intMin, def->intMax, &parsed)) {
        TraceLog(LOG_WARNING,
                 "Config: %s: %s has an invalid value: %s, the default is kept",
                 sPath, key, value);
        break;
      }
      sValues[index].intValue = parsed;
      break;
    }
    case CONFIG_TYPE_STRING: {
      if (value[0] == '\0') {
        TraceLog(LOG_WARNING,
                 "Config: %s: %s has an empty value, the default is kept",
                 sPath, key);
        break;
      }
      if (strlen(value) >= CONFIG_STRING_MAX) {
        TraceLog(LOG_WARNING, "Config: %s: %s is too long, truncated", sPath,
                 key);
      }
      CopyString(sValues[index].stringValue, sizeof(sValues[index].stringValue),
                 value);
      break;
    }
  }
}

// 逐行读文件。行太长时把余下的部分读掉再告警，否则余下的部分会被
// 当成新的一行解析。
static void ParseFile(FILE* file) {
  char line[CONFIG_LINE_MAX];
  while (fgets(line, sizeof(line), file) != NULL) {
    if (strchr(line, '\n') == NULL && !feof(file)) {
      TraceLog(LOG_WARNING, "Config: %s has an over-long line, truncated",
               sPath);
      int c = 0;
      while ((c = fgetc(file)) != EOF && c != '\n') {
      }
      continue;
    }
    ProcessLine(line);
  }
}

// === 路径 ===

// 三平台的默认位置：
//   macOS           $HOME/Library/Application Support/TheLawOfPrey/config.cfg
//   Linux / FreeBSD ${XDG_CONFIG_HOME:-$HOME/.config}/TheLawOfPrey/config.cfg
// XDG_CONFIG_HOME 为空时按 $HOME/.config 展开；HOME 也取不到时退回当前
// 工作目录并告警，不崩溃。
static void ResolvePath(void) {
  if (sPathOverride != NULL) {
    CopyString(sPath, sizeof(sPath), sPathOverride);
    return;
  }

  const char* home = getenv("HOME");
  const bool haveHome = home != NULL && home[0] != '\0';

#if defined(__APPLE__)
  if (!haveHome) {
    TraceLog(LOG_WARNING,
             "Config: HOME is not set, using the config file in the working "
             "directory");
    snprintf(sPath, sizeof(sPath), "%s/%s", CONFIG_FALLBACK_DIR,
             CONFIG_FILE_NAME);
    return;
  }
  snprintf(sPath, sizeof(sPath), "%s/Library/Application Support/%s/%s", home,
           CONFIG_APP_DIR, CONFIG_FILE_NAME);
#else
  const char* xdg = getenv("XDG_CONFIG_HOME");
  if (xdg != NULL && xdg[0] != '\0') {
    snprintf(sPath, sizeof(sPath), "%s/%s/%s", xdg, CONFIG_APP_DIR,
             CONFIG_FILE_NAME);
    return;
  }
  if (!haveHome) {
    TraceLog(LOG_WARNING,
             "Config: neither XDG_CONFIG_HOME nor HOME is set, using the "
             "config file in the working directory");
    snprintf(sPath, sizeof(sPath), "%s/%s", CONFIG_FALLBACK_DIR,
             CONFIG_FILE_NAME);
    return;
  }
  snprintf(sPath, sizeof(sPath), "%s/.config/%s/%s", home, CONFIG_APP_DIR,
           CONFIG_FILE_NAME);
#endif
}

// === 权限探测 ===

typedef struct {
  bool fileExists;
  bool readable;
  bool writable;
  bool dirExists;
  bool dirWritable;
} ConfigPermissions;

// 用 access(2) 探测权限：文件是否存在、是否可读、是否可写，
// 以及所在目录是否存在、是否可写。
//
// 这是尽力而为的探测，写入方仍须处理写入失败：判 W_OK 不保证真正
// 写入时成功（磁盘满、只读挂载、文件被别的进程换掉、setuid 位等）。
static void ProbePermissions(const char* path, ConfigPermissions* out) {
  out->fileExists = access(path, F_OK) == 0;
  out->readable = out->fileExists && access(path, R_OK) == 0;
  out->writable = out->fileExists && access(path, W_OK) == 0;

  char dir[CONFIG_DIR_MAX];
  ExtractDir(path, dir, sizeof(dir));
  out->dirExists = access(dir, F_OK) == 0;
  out->dirWritable = out->dirExists && access(dir, W_OK) == 0;
}

// === 公开接口 ===

void ConfigSetPathOverride(const char* path) {
  if (path == NULL) {
    sPathOverride = NULL;
    return;
  }
  if (path[0] == '\0') {
    TraceLog(LOG_ERROR,
             "Config: the config path is empty, using the default location");
    sPathOverride = NULL;
    return;
  }
  sPathOverride = path;
}

bool ConfigLoad(void) {
  EnsureReady();
  ResetToDefaults();
  ResolvePath();

  // 显式指定的路径打不开是硬错误：那是用户打错字的典型场景，
  // 静默退回默认值会把打错的路径藏起来。平台默认位置的文件不存在
  // 则是首次运行的正常状态。
  const bool explicitPath = sPathOverride != NULL;

  if (access(sPath, F_OK) != 0) {
    if (explicitPath) {
      TraceLog(LOG_ERROR, "Config: the given config file does not exist: %s",
               sPath);
      return false;
    }
    TraceLog(LOG_INFO, "Config: no config file at %s, using the default values",
             sPath);
    return true;
  }

  FILE* file = fopen(sPath, "rb");
  if (file == NULL) {
    if (explicitPath) {
      TraceLog(LOG_ERROR, "Config: the given config file cannot be opened: %s",
               sPath);
      return false;
    }
    TraceLog(LOG_WARNING,
             "Config: failed to open the config file: %s, using the default "
             "values",
             sPath);
    return true;
  }

  ParseFile(file);
  fclose(file);
  return true;
}

const char* ConfigPath(void) { return sPath; }

bool ConfigCanWrite(void) {
  ConfigPermissions permissions;
  ProbePermissions(ConfigPath(), &permissions);
  if (permissions.fileExists) return permissions.writable;
  // 文件不在：写得进去的前提是目录在，且目录可写
  return permissions.dirExists && permissions.dirWritable;
}

bool ConfigGetBool(const char* key) {
  EnsureReady();
  const int index = FindKeyIndex(key);
  if (index < 0) {
    TraceLog(LOG_ERROR, "Config: unknown key queried: %s",
             key != NULL ? key : "(null)");
    return false;
  }
  if (kConfigKeys[index].type != CONFIG_TYPE_BOOL) {
    TraceLog(LOG_ERROR, "Config: the key %s is not a boolean", key);
    return false;
  }
  return sValues[index].boolValue;
}

int ConfigGetInt(const char* key) {
  EnsureReady();
  const int index = FindKeyIndex(key);
  if (index < 0) {
    TraceLog(LOG_ERROR, "Config: unknown key queried: %s",
             key != NULL ? key : "(null)");
    return 0;
  }
  if (kConfigKeys[index].type != CONFIG_TYPE_INT) {
    TraceLog(LOG_ERROR, "Config: the key %s is not an integer", key);
    return 0;
  }
  return sValues[index].intValue;
}

const char* ConfigGetString(const char* key) {
  EnsureReady();
  const int index = FindKeyIndex(key);
  if (index < 0) {
    TraceLog(LOG_ERROR, "Config: unknown key queried: %s",
             key != NULL ? key : "(null)");
    return "";
  }
  if (kConfigKeys[index].type != CONFIG_TYPE_STRING) {
    TraceLog(LOG_ERROR, "Config: the key %s is not a string", key);
    return "";
  }
  return sValues[index].stringValue;
}

void ConfigLogEffective(const char* appliedTheme) {
  EnsureReady();

  ConfigPermissions permissions;
  ProbePermissions(ConfigPath(), &permissions);

  const char* dirState = "missing";
  if (permissions.dirExists) {
    dirState = permissions.dirWritable ? "ok" : "not-writable";
  }

  TraceLog(LOG_INFO,
           "Config: path=%s fullscreen=%s target_fps=%d theme=%s lang=%s "
           "exists=%s readable=%s writable=%s dir=%s",
           ConfigPath(), ConfigGetBool("fullscreen") ? "true" : "false",
           ConfigGetInt("target_fps"),
           appliedTheme != NULL ? appliedTheme : "-",
           ConfigGetString("lang"), permissions.fileExists ? "yes" : "no",
           permissions.readable ? "yes" : "no",
           permissions.writable ? "yes" : "no", dirState);

  // 四种情形分开写，别混成一句：文件不存在是首次运行的正常状态，
  // 读不了要退回默认值，不可写说明将来写设置会失败，
  // 目录不存在说明首次写入时得先把它建出来。
  if (permissions.fileExists && !permissions.readable) {
    TraceLog(LOG_WARNING,
             "Config: the config file exists but is not readable: %s, using "
             "the default values",
             ConfigPath());
  }
  if (permissions.fileExists && permissions.readable && !permissions.writable) {
    TraceLog(LOG_WARNING,
             "Config: the config file exists but is not writable: %s, writing "
             "the settings will fail",
             ConfigPath());
  }
  if (!permissions.dirExists) {
    char dir[CONFIG_DIR_MAX];
    ExtractDir(ConfigPath(), dir, sizeof(dir));
    TraceLog(LOG_INFO,
             "Config: the config directory does not exist yet: %s, it will be "
             "created on the first write",
             dir);
  }
}
