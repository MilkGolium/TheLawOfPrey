#ifndef LANG_H
#define LANG_H

#include <stdbool.h>

// 检查语言 ID 是否在支持列表中。NULL 或空字符串返回 false。
bool LangIsKnown(const char* id);

// 设置当前语言。id 为 NULL 时使用默认语言 (zh-hans)。
// 必须在 InitGameFont() 之前调用，因为字号收集阶段会扫描对应语言的目录，
// 若在之后调用会导致图集收集到错误的语言字符。
void LangSet(const char* id);

// 返回当前语言 ID，例如 "zh-hans"。
const char* LangCurrent(void);

// 返回当前语言的文案目录路径，例如 "assets/strings/zh-hans"。
// 返回指向静态缓冲区的指针：调用者不得释放，且不得在多线程中并发修改。
const char* LangStringsDir(void);

// 返回所有已知语言的列表，以 '|' 分隔，例如 "zh-hans|en"。
// 用于 CLI 用法提示。
const char* LangKnownList(void);

#endif  // LANG_H
