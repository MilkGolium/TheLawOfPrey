#include "strings.h"

#include <raylib.h>
#include <string.h>

#include "lang.h"
#include "utils.h"

// 文案目录由当前语言决定（assets/strings/<lang>），本文件里只此一处引用，
// 路径的唯一来源是 lang.h，与字体码点收集共用同一条路径。

typedef struct {
  char* key;
  char* text;
} StringEntry;

static StringEntry* sEntries = NULL;
static int sEntryCount = 0;
static int sEntryCapacity = 0;

// 已经告警过的缺失 key。GetString 在绘制路径上每帧都会被调用，
// 不记账的话一条漏配能把日志刷爆。
static char** sWarnedKeys = NULL;
static int sWarnedCount = 0;
static int sWarnedCapacity = 0;

static char* CopyText(const char* text) {
  size_t size = (size_t)strlen(text) + 1;
  char* copy = (char*)MemAlloc(size);
  if (!copy) {
    TraceLog(LOG_ERROR, "Strings: 文案分配失败: %s", text);
    return NULL;
  }
  memcpy(copy, text, size);
  return copy;
}

static void WarnOnce(const char* key) {
  for (int i = 0; i < sWarnedCount; i++) {
    if (strcmp(sWarnedKeys[i], key) == 0) return;
  }
  if (sWarnedCount == sWarnedCapacity) {
    int capacity = sWarnedCapacity ? sWarnedCapacity * 2 : 8;
    char** grown =
        (char**)MemRealloc(sWarnedKeys, (size_t)capacity * sizeof(char*));
    if (!grown) {
      // 记账失败只会导致告警重复，不影响取用文案
      TraceLog(LOG_WARNING, "Strings: 告警表扩容失败: %s", key);
      return;
    }
    sWarnedKeys = grown;
    sWarnedCapacity = capacity;
  }
  char* copy = CopyText(key);
  if (!copy) return;
  sWarnedKeys[sWarnedCount++] = copy;
  TraceLog(LOG_WARNING, "Strings: 缺少文案 %s，界面上回退为 key 本身", key);
}

static void AddEntry(const char* key, const char* text) {
  for (int i = 0; i < sEntryCount; i++) {
    if (strcmp(sEntries[i].key, key) != 0) continue;
    if (strcmp(sEntries[i].text, text) == 0) return;
    TraceLog(LOG_WARNING, "Strings: key %s 重复定义，保留先出现的文案", key);
    return;
  }

  if (sEntryCount == sEntryCapacity) {
    int capacity = sEntryCapacity ? sEntryCapacity * 2 : 16;
    StringEntry* grown = (StringEntry*)MemRealloc(
        sEntries, (size_t)capacity * sizeof(StringEntry));
    if (!grown) {
      TraceLog(LOG_ERROR, "Strings: 文案表扩容失败，丢弃 %s", key);
      return;
    }
    sEntries = grown;
    sEntryCapacity = capacity;
  }

  char* keyCopy = CopyText(key);
  char* textCopy = CopyText(text);
  if (!keyCopy || !textCopy) {
    MemFree(keyCopy);
    MemFree(textCopy);
    return;
  }
  sEntries[sEntryCount].key = keyCopy;
  sEntries[sEntryCount].text = textCopy;
  sEntryCount++;
}

// 表头行在每个文件里占第一行，直接跳过；键为空的行同样跳过
// （文件末尾的换行会解析出一行空字段）。
static void OnCsvRow(const char* const* fields, int fieldCount,
                     void* userData) {
  bool* isHeaderRow = (bool*)userData;
  if (*isHeaderRow) {
    *isHeaderRow = false;
    return;
  }
  if (fieldCount < 2 || fields[0][0] == '\0') return;
  AddEntry(fields[0], fields[1]);
}

void InitStrings(void) {
  // 可重入：重复调用先清干净
  UnloadStrings();

  const char* dir = LangStringsDir();
  FilePathList files = LoadDirectoryFiles(dir);
  for (unsigned int i = 0; i < files.count; i++) {
    if (!IsFileExtension(files.paths[i], ".csv")) continue;
    bool isHeaderRow = true;
    CsvReadFile(files.paths[i], OnCsvRow, &isHeaderRow);
  }
  UnloadDirectoryFiles(files);

  if (sEntryCount == 0) {
    TraceLog(LOG_WARNING, "Strings: %s 下没有可用文案", dir);
  } else {
    TraceLog(LOG_INFO, "Strings: 载入 %d 条界面文案", sEntryCount);
  }
}

void UnloadStrings(void) {
  for (int i = 0; i < sEntryCount; i++) {
    MemFree(sEntries[i].key);
    MemFree(sEntries[i].text);
  }
  MemFree(sEntries);
  sEntries = NULL;
  sEntryCount = 0;
  sEntryCapacity = 0;

  for (int i = 0; i < sWarnedCount; i++) MemFree(sWarnedKeys[i]);
  MemFree(sWarnedKeys);
  sWarnedKeys = NULL;
  sWarnedCount = 0;
  sWarnedCapacity = 0;
}

const char* GetString(const char* key) {
  if (key == NULL) return "";
  for (int i = 0; i < sEntryCount; i++) {
    if (strcmp(sEntries[i].key, key) == 0) return sEntries[i].text;
  }
  WarnOnce(key);
  return key;
}