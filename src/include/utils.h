#ifndef UTLIS_H
#define UTLIS_H

#include <stdbool.h>

void InitAssetsDirectory(void);

// === CSV 读取 ===
//
// 字体收集与界面文案加载共用，全仓库只此一份解析实现，
// 不允许各自再写一个解析器。
//
// 行为约定：
//   - UTF-8，自动跳过 BOM；
//   - 行结束符接受 LF 与 CRLF，裸 CR 同样视作行结束；
//   - 引号字段内的 "" 解码为单个 '"'，内嵌逗号原样保留；
//   - 引号字段不跨行：字段内遇到行结束符即结束该字段，与非引号
//     字段走同一条规则。取舍理由见 utils.c 中的注释；
//   - 字段超过内部上限时截断，并在该文件上告警一次。

// 单行回调。fields 指向本行各字段，fieldCount 为字段数。
// 回调返回后 fields 立即失效，需要留存时应自行拷贝。
typedef void (*CsvRowCallback)(const char* const* fields, int fieldCount,
                               void* userData);

// 逐行读取 path。表头行与数据行走同一个回调，调用方自行区分。
// 读取失败返回 false。重复调用安全，每次调用独立解析。
bool CsvReadFile(const char* path, CsvRowCallback callback, void* userData);

#endif