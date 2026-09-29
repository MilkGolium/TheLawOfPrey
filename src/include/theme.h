#ifndef THEME_H
#define THEME_H

#include <stdbool.h>

// 主题只负责配色。字体度量由字体模块独占，主题文件无法改动，
// 否则社区配置能静默破坏 24px 像素格对齐与中文渲染。

// 扫描 assets/themes/ 下的 .cfg，并加载 default.cfg；
// 没有 default 时退回列表中第一个主题。
void InitTheme(void);

// 可用主题数量与名称，供设置界面枚举。
int GetThemeCount(void);
const char* GetThemeName(int index);

// 按名称加载主题。失败时保持当前样式不变并返回 false。
bool LoadTheme(const char* name);

// 当前生效的主题名，未加载过任何主题时返回空串。
const char* GetCurrentThemeName(void);

#endif
