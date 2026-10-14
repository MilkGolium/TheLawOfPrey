#ifndef APP_H
#define APP_H

#include <stdbool.h>

// 程序的退出请求。只记录「有人要求退出」，具体何时收尾由主循环决定：
// 场景与按钮不直接关窗口（关窗口会打乱卸载顺序，让字体、文案与画布
// 的释放落在 GL 上下文销毁之后），也不调用 CloseWindow。
// 约定见 README「键位与输入约定」。

// 提出退出请求。可以重复调用，无副作用。
void AppRequestQuit(void);

// 主循环是否应当退出。系统级关窗（Alt+F4 / Command+Q）不看这里，
// 那条路让 WindowShouldClose 变真。
bool AppShouldQuit(void);

#endif  // APP_H