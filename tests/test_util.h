#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <raylib.h>
#include <stdbool.h>

#include "ui.h"

// === 自写测试骨架 ===
//
// 本仓库不引入任何第三方测试框架，断言与日志捕获都在这里实现。
// 约定：
//   - 断言失败不中止当前用例，一次跑完能看完全部失败；
//   - 只要有任何一条失败，TestEnd 返回非零，ctest 据此判定失败；
//   - 失败信息一律走 stdout，不经过 TraceLog，因此调低日志等级也不会
//     把断言输出吞掉。

void TestBegin(const char* suiteName);

// 开一个用例，name 会出现在后续所有断言的前面。
void TestBeginCase(const char* name);
void TestEndCase(void);

// 打印汇总并返回进程退出码（有用例失败时为非零）。
int TestEnd(void);

void TestCheck(bool ok, const char* what);
void TestCheckInt(int actual, int expected, const char* what);
void TestCheckStr(const char* actual, const char* expected, const char* what);
void TestCheckColor(Color actual, Color expected, const char* what);
void TestCheckStyle(const UiStyle* actual, const UiStyle* expected,
                    const char* what);

// 恒失败。用于「走到这里说明前面的判据没拦住」的场合。
void TestFail(const char* what);

// === 日志捕获 ===
//
// SetTraceLogCallback 把 LOG_WARNING 收进内存缓冲，逐用例断言「该告警的
// 路径确实告警了」。只看返回值不够：告警本身就是契约的一部分，社区主题
// 写错键时靠它提示，写坏了却一声不吭才是问题。LOG_ERROR 及以上不收进
// 缓冲，直接转发到 stderr，避免真正的错误被断言噪声淹没。

void TestLogBegin(void);
void TestLogEnd(void);
void TestLogReset(void);
int TestLogWarnCount(void);
bool TestLogHasWarn(const char* needle);
bool TestLogMissingWarn(const char* needle);

// 打印当前捕获到的告警，只用于失败后的诊断。
void TestLogPrint(void);

#endif  // TEST_UTIL_H