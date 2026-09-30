#ifndef FONT_H
#define FONT_H

#include <raylib.h>
#include <stdbool.h>

// 初始化游戏字体：扫描数据文件收集 codepoint，加载图集，
// 并执行缺字检查。必须在 InitAssetsDirectory 之后、主循环之前调用。
void InitGameFont(void);

// 卸载所有字体资源。必须在 CloseWindow 之前调用。
void UnloadGameFont(void);

// 取得指定尺寸的字体。支持的尺寸为 12、24、36。
// 不支持的尺寸会回退到 24。
Font GetUIFont(int size);

// 查询某 codepoint 是否被收集进图集；不代表字体真有其字形，缺字仍会回退为 '?'。
bool FontHasGlyph(int codepoint);

// 绘制文本，封装 DrawTextEx。
void DrawUIText(int posX, int posY, const char* text, int size, Color tint);

// 测量文本尺寸，封装 MeasureTextEx。
Vector2 MeasureUIText(const char* text, int size);

#endif  // FONT_H
