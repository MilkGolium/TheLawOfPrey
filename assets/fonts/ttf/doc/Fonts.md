字体说明
========

本文件记录仓库实际内置的字体、其授权状态，以及在 raylib 下加载像素字
体时踩过的坑。

当前唯一内置字体
----------------

- 路径：`assets/fonts/ttf/fusion-pixel-12px-proportional-zh_hans.ttf`
- 名称：Fusion Pixel Font 12px Proportional zh-Hans
- 版本：2026.09.25（`TakWolf/fusion-pixel-font` 的 GitHub Release）
- 用途：全局唯一界面字体，`src/font.c` 的 `FONT_PATH` 硬编码指向它
- 字形数：36999
- 文件 sha256：
  `b2ee68647dc257fa697e4d0c78f9b59461bc504a023b9a3b5ad66f358bbb1613`
- `upem = 1200`，原生 12px，本项目按 12 的整数倍（12 / 24 / 36）渲染
- 授权：SIL Open Font License 1.1，许可全文见
  `assets/fonts/ttf/licenses/`

### 为什么不是纯 Ark Pixel Font

原计划直接换用 Ark Pixel Font（同样是 OFL-1.1）。实测后发现它**缺少游
戏实际在用的 7 个字**，会导致这些字在界面上渲染成 `?`：

    恐 浆 筑 蕨 蘑 蛮 餐

缺字位置全部落在 `assets/databases/items.csv` 的真实物品文本里，其中
「恐」出现 5 次、「蕨」3 次、「浆」3 次，不是边缘用例。Ark Pixel 的
7 个语言变体（`zh_hans` / `zh_hant` / `zh_tw` / `zh_hk` / `ja` / `ko` /
`latin`）字形数完全相同（24869），覆盖范围一致，缺的是同一批字，换变
体解决不了。

因此改用 Fusion Pixel Font。它由 TakWolf 制作，**本身就包含 Ark
Pixel**，另外合并了 cubic-11 与 galmuri 两个字体来补足覆盖度——缺的那
7 个字正是从后两者补上的。三者全部是 OFL-1.1，所以许可结论与直接用
Ark Pixel 完全一致。

### 授权构成

`assets/fonts/ttf/licenses/` 保存了全部许可全文，逐一核对结果：

| 组件 | 许可 | 版权声明 | 保留字体名 |
|------|------|----------|------------|
| Fusion Pixel Font（整体） | OFL-1.1 | Copyright (c) 2022, TakWolf | 无 |
| ark-pixel | OFL-1.1 | Copyright (c) 2021, TakWolf | 无 |
| cubic-11 | OFL-1.1（另附 M+/IPA 类宽松许可，可任选） | M+ FONTS PROJECT 2005、COZ 2002-2004 | 无 |
| galmuri | OFL-1.1 | Copyright (c) 2019-2025 Lee Minseo | 无 |

四份文件均无「with Reserved Font Name」条款，即不保留字体名，后续修改
或再分发不受命名限制。OFL 允许把字体随软件打包、再分发乃至随商业产品
销售，只要求不得单独出售字体本身，且必须随分发附带许可全文与版权声
明——上述文件即为满足该要求而保留。

换字体前的历史（供追溯）
------------------------

此前内置的是 `assets/fonts/ttf/zpix.ttf`（Zpix／最像素），已删除。它
是专有商业字体：上游仓库没有 LICENSE 文件，README 以价目表授权，商业
单个产品 USD $1000 / RMB ￥7000，且版权声明禁止「修改、反编译、转换、
拆分等反向操作」。正因为上游禁止转换，连运行时把字形栅格化进图集都存
在合规争议，故直接移除而不是付费采购。

同时删除的还有 `src/raygui/styles/` 下 20 个第三方样式字体（其中 13 个
含字体的样式目录没有任何许可声明）。该目录从未被项目源码引用，raygui
的样式加载接口已列入禁用清单。

行高与像素格（换字体后的已知差异）
----------------------------------

zpix 的 `hhea` 为 `ascent 1000 / descent -200 / lineGap 0`，在 12px 下
行高正好 12.00px。Fusion Pixel 的 `hhea` 为 `ascent 1300 / descent
-300 / lineGap 0`，同样 `upem = 1200`，但行高变成：

| 渲染字号 | zpix 行高 | Fusion 行高 |
|----------|------------|--------------|
| 12px | 12.00px | 16.00px |
| 24px | 24.00px | 32.00px |
| 36px | 36.00px | 48.00px |

**行高不再是 12 的整数倍。** 图集尺寸仍由传给 `LoadFontEx` 的
`fontSize` 决定（12 / 24 / 36），字形本身仍落在 12px 网格上，整数倍
缩放的约束没有被破坏；受影响的是多行文字的纵向步进与控件高度计算。

README 中「修复 raygui 文字贴边」一条原本是照 zpix 在 24px 下 CJK 墨迹
高 22px 写下的，换字体后该数值已不成立，需要重新实测再定
`TEXT_PADDING` 与各控件高度常量。

加载像素字体的已知坑
--------------------

以下与具体字体无关，换任何字体都适用。

缺字会静默变成第一个字形
    `LoadFontEx` 传入的 codepoints 决定了图集内容，漏掉的字符不进图集。
    绘制时 `GetGlyphIndex` 查不到码点会返回索引 0，于是把图集第一个字形
    画到每个缺字位置。一句中文可能整体显示成同一个字，且没有任何告警。
    本项目为此保留了启动时的缺字扫描与 `TraceLog(LOG_WARNING)` 输出
    （见 `src/font.c` 的 `CheckMissingGlyphs`），不要移除。

不要用 `LoadFont` 或默认字体画中文
    `LoadFont` 只生成 ASCII 32..126 的 95 个字形；raylib 默认字体同样
    不含 CJK 字形。`DrawText()` 永远使用内置默认字体，画中文必定是方框。
    raylib 6.0 没有 `SetFontDefault`，只能走 `DrawTextEx` /
    `MeasureTextEx`。

正确加载方式
    先用 `LoadCodepoints` 把 UTF-8 文本解成码点数组，再交给
    `LoadFontEx`，保证实际用到的每个字符都进图集：

    ```c
    int codepointCount = 0;
    int *codepoints = LoadCodepoints(text, &codepointCount);
    Font f = LoadFontEx(path, size, codepoints, codepointCount);
    UnloadCodepoints(codepoints);
    ```

    不要把整个 CJK 区间（如 0x4E00-0x9FFF）传给 `LoadFontEx`，图集会
    被撑大。只需传实际用到的码点。

整数倍缩放
    点阵字体按固定像素网格绘制，渲染字号应为原生尺寸的整数倍，否则各字符
    的像素网格不一致，观感变差。12px 字体对应 12 / 24 / 36 / 48 / 60。
    `LoadFontEx` 的 fontSize 即最终渲染字号，直接传整数倍值，不需要传原生
    尺寸。贴图过滤保持 `TEXTURE_FILTER_POINT`，改线性会失去锐利度。

图集体积
    与传入的码点数量成正比。图集在显存中只应有一份，因此本项目按尺寸懒
    加载并缓存，切换场景时不重载。

覆盖度自查
    换字体后必须重新核对字形覆盖，不同字体的差异可能极大——本次换字体
    就是因为 Ark Pixel 缺了 7 个在用字才改用 Fusion Pixel，两者覆盖度差
    别很大，不能想当然认为同族字体覆盖一致。用 fontTools 读 cmap 与实际
    文本比对：

    ```python
    from fontTools.ttLib import TTFont
    cmap = TTFont(path).getBestCmap()
    missing = [c for c in text if ord(c) not in cmap]
    print('missing:', ''.join(missing) if missing else '无')
    ```

    换字体后跑一次游戏，确认启动日志出现「缺字检查通过」，这比任何静态
    检查都可靠。
