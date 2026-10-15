#ifndef STRINGS_H
#define STRINGS_H

// 界面文案表。数据在 assets/strings/<lang>/<scene_name>.csv，格式为带表头的
// key,text 两列，按场景一个文件（见 README「素材规范」）。
//
// 字符串必须放数据文件而不写死在 C 字面量里：字体的 codepoint 是精确
// 收集的，写死的中文不在收集范围内就会变方框，且日后做多语言也无从下手。

// 扫描当前语言目录（assets/strings/<lang>）下的全部 CSV 并载入内存。可重入，重复调用会
// 先清空旧表。必须在 InitAssetsDirectory 之后调用。
void InitStrings(void);

// 释放全部文案。幂等、可重入，必须在程序退出前调用。
void UnloadStrings(void);

// 按 key 取文案。找不到时告警一次（同一条 key 不重复告警）并返回 key
// 本身，让漏配在界面上直接可见；任何情况下都不返回 NULL。
const char* GetString(const char* key);

#endif  // STRINGS_H