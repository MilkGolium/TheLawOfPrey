#include "lang.h"

#include <stdio.h>
#include <string.h>

#include "raylib.h"

// === 语言表 ===
//
// 标识用 BCP-47 的小写形态，且与文案目录名必须逐字符一致：macOS 的文件
// 系统大小写不敏感，写错了在本机跑得通，到大小写敏感的 Linux/CI 上会直接
// 找不到目录。目录结构见 README「字体与界面方案约束」。

#define LANG_DIR_MAX 128

static const char* const kLanguages[] = {"zh-hans", "en"};
static const int kLanguageCount = 2;
static const char* const kDefaultLanguage = "zh-hans";

static const char* sCurrent = kDefaultLanguage;

// 缓存的文案目录，形如 assets/strings/zh-hans
static char sStringsDir[LANG_DIR_MAX] = "";

bool LangIsKnown(const char* id) {
  if (id == NULL || id[0] == '\0') return false;
  for (int i = 0; i < kLanguageCount; i++) {
    if (strcmp(id, kLanguages[i]) == 0) return true;
  }
  return false;
}

void LangSet(const char* id) {
  if (id != NULL) {
    if (!LangIsKnown(id)) {
      // 调用方负责校验，走到这里说明校验被绕过：保持原语言，不静默改
      TraceLog(LOG_ERROR, "Lang: 不支持的语言 %s，保持 %s", id, sCurrent);
      return;
    }
    sCurrent = id;
  }

  snprintf(sStringsDir, sizeof(sStringsDir), "assets/strings/%s", sCurrent);

  // 目录缺失不是致命错误：文案会回退成 key 本身，缺字检查照常告警。
  // 打一条提醒是为了打包漏带语言目录时不至于一头雾水。
  if (!DirectoryExists(sStringsDir)) {
    TraceLog(LOG_WARNING, "Lang: 文案目录不存在：%s", sStringsDir);
  }
}

const char* LangCurrent(void) { return sCurrent; }

const char* LangStringsDir(void) {
  // 缺省语言下可能没人显式调过 LangSet，这里补一次
  if (sStringsDir[0] == '\0') LangSet(NULL);
  return sStringsDir;
}

const char* LangKnownList(void) {
  // 只在第一次被问到的时候拼一次，省得每帧调用都动格式化函数
  static char list[LANG_DIR_MAX] = "";
  if (list[0] == '\0') {
    size_t used = 0;
    for (int i = 0; i < kLanguageCount; i++) {
      const int written = snprintf(list + used, sizeof(list) - used, "%s%s",
                                   i == 0 ? "" : "|", kLanguages[i]);
      if (written < 0 || (size_t)written >= sizeof(list) - used) break;
      used += (size_t)written;
    }
  }
  return list;
}
