#include "utils.h"

#include <raylib.h>
#include <string.h>

void InitAssetsDirectory(void) {
  // 首先切换到程序自身所在的目录
  ChangeDirectory(GetApplicationDirectory());

  // 检查程序身边的 assets 是否存在
  if (DirectoryExists("assets")) {
    TraceLog(LOG_INFO, "正在使用程序身边的 assets 目录：%s/assets",
             GetWorkingDirectory());
  }
  // 如果身边没有，且编译时指定了项目根目录，尝试使用 CMake 指定的路径
#ifdef PROJECT_ROOT_DIR
  else if (DirectoryExists(TextFormat("%s/assets", PROJECT_ROOT_DIR))) {
    ChangeDirectory(PROJECT_ROOT_DIR);
    TraceLog(LOG_INFO,
             "程序身边的 assets 不存在，已回退至开发目录：%s \n "
             "这可能是因为程序正在开发目录中运行，没有打包发行。",
             GetWorkingDirectory());
  }
#endif
  else {
    TraceLog(LOG_ERROR, "未找到 assets 目录！当前工作目录：%s",
             GetWorkingDirectory());
  }
}

// === CSV 读取 ===

// 单个字段的字节上限（含结尾的 '\0'）。超出的字符被丢弃，
// 并在该文件上告警一次：被截掉的字段既进不了图集也不该悄悄消失。
#define CSV_FIELD_MAX 1024

// 一行的列数上限。物品表当前 8 列，取 64 是给表格型数据留的余量；
// 超过上限的表头视为损坏文件，该文件按超限列数解析并告警。
#define CSV_MAX_COLUMNS 64

typedef struct {
  const char* end;
  const char* cursor;
  bool truncated;  // 本文件出现过超长字段
} CsvStream;

// 追加一个字符到字段缓冲。放不下时丢弃并记账，由调用方在文件结束时
// 统一告警，同一文件因此只会报一次。
static void AppendFieldChar(CsvStream* s, char* out, int outSize, int* outLen,
                            char c) {
  if (*outLen < outSize - 1) {
    out[(*outLen)++] = c;
  } else {
    s->truncated = true;
  }
}

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
          AppendFieldChar(s, out, outSize, &outLen, '"');
          p += 2;
          continue;
        }
        p++;  // 闭合引号
        break;
      }
      // 行结束符终止字段。取舍理由：本项目的 CSV 由程序生成，
      // 不存在字段内换行的需求；让引号字段与非引号字段共用同一条
      // 行结束规则，比支持跨行少一类只对部分字段生效的特例。
      if (*p == '\r' || *p == '\n') break;
      AppendFieldChar(s, out, outSize, &outLen, *p);
      p++;
    }
    // 跳过引号结束后的多余字符，直到分隔符或行结束
    while (p < s->end && *p != ',' && *p != '\n' && *p != '\r') p++;
  } else {
    // 非引号字段
    while (p < s->end && *p != ',' && *p != '\n' && *p != '\r') {
      AppendFieldChar(s, out, outSize, &outLen, *p);
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

// 读取一行的各字段到 fields（每列 CSV_FIELD_MAX 字节），
// 列数上限 maxColumns：超出上限的列被丢弃但整行照常消费完，
// 免得剩下的列被当成新行。outCount 报告的是该行的真实列数，
// 调用方据此判断哪些列还有缓冲可用。
// 返回 false 表示流已耗尽。
static bool CsvNextRow(CsvStream* s, char fields[][CSV_FIELD_MAX],
                       int maxColumns, int* outCount) {
  int count = 0;
  bool rowEnd = false;
  char overflow[CSV_FIELD_MAX];
  while (!rowEnd) {
    char* target = (count < maxColumns) ? fields[count] : overflow;
    if (!CsvNextField(s, target, CSV_FIELD_MAX, &rowEnd)) return false;
    count++;
  }
  *outCount = count;
  return true;
}

bool CsvReadFile(const char* path, CsvRowCallback callback, void* userData) {
  int dataSize = 0;
  unsigned char* data = LoadFileData(path, &dataSize);
  if (!data || dataSize == 0) {
    if (data) UnloadFileData(data);
    TraceLog(LOG_WARNING, "Csv: 无法读取文件: %s", path);
    return false;
  }

  // 跳过 UTF-8 BOM
  const char* p = (const char*)data;
  const char* end = p + dataSize;
  if (dataSize >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
    p += 3;
  }

  CsvStream stream = {.end = end, .cursor = p, .truncated = false};

  // 表头决定本文件的列数：先用单列容量探一次拿到列数并回退重读，
  // 再按该列数分配缓冲。固定列数要么截断数据、要么给栈上留几 KB，
  // 两种都不划算。
  char probe[CSV_FIELD_MAX];
  int columnCount = 0;
  const char* firstRow = stream.cursor;
  if (!CsvNextRow(&stream, &probe, 1, &columnCount)) {
    // 只有 BOM 或空文件，没有数据行
    UnloadFileData(data);
    return true;
  }
  stream.cursor = firstRow;

  if (columnCount > CSV_MAX_COLUMNS) {
    TraceLog(LOG_WARNING, "Csv: %s 表头有 %d 列，超过上限 %d，按上限解析", path,
             columnCount, CSV_MAX_COLUMNS);
    columnCount = CSV_MAX_COLUMNS;
  }

  char (*fields)[CSV_FIELD_MAX] =
      MemAlloc((size_t)columnCount * sizeof(fields[0]));
  const char** rowPtrs = MemAlloc((size_t)columnCount * sizeof(rowPtrs[0]));
  if (!fields || !rowPtrs) {
    MemFree(fields);
    MemFree(rowPtrs);
    UnloadFileData(data);
    TraceLog(LOG_ERROR, "Csv: %s 行缓冲分配失败", path);
    return false;
  }

  while (stream.cursor < stream.end) {
    int count = 0;
    if (!CsvNextRow(&stream, fields, columnCount, &count)) break;
    if (count > columnCount) count = columnCount;  // 超限列没有缓冲可用
    if (count == 0) continue;
    for (int i = 0; i < count; i++) rowPtrs[i] = fields[i];
    callback(rowPtrs, count, userData);
  }

  if (stream.truncated) {
    TraceLog(LOG_WARNING, "Csv: %s 中存在超过 %d 字节的字段，已截断", path,
             CSV_FIELD_MAX - 1);
  }

  MemFree(rowPtrs);
  MemFree(fields);
  UnloadFileData(data);
  return true;
}