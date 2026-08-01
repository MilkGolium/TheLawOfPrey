字体库说明
========

概述
----
本仓库在 assets/fonts/ttf/ark_pixel/ 下内置了"方舟像素字体"（Ark Pixel Font，
项目主页 https://github.com/TakWolf/ark-pixel-font ）的字体文件，共 3 种像素尺寸
× 2 种宽度模式 × 7 种语言字形。该字体使用 SIL OFL-1.1 许可证（各子目录内附
OFL.txt）。

需要特别注意的是：该字体仍处于积极开发阶段，**不同尺寸版本的字符覆盖差异极大**。
本文件记录实测的覆盖情况、一次真实踩坑案例（中文渲染成"个个个……"）的原因、
字体的选用方法，以及渲染字号与整数倍缩放的建议。

目录结构
----
字体按 像素尺寸 / 宽度模式 组织：

    assets/fonts/ttf/ark_pixel/
    ├── 10px-proportional/      10px 比例模式（约 520 KB/文件）
    ├── 10px-monospaced/        10px 等宽模式
    ├── 12px-proportional/      12px 比例模式（约 4.8 MB/文件，完整字库）
    ├── 12px-monospaced/        12px 等宽模式
    ├── 16px-proportional/      16px 比例模式（约 480 KB/文件，基础子集）
    └── 16px-monospaced/        16px 等宽模式

每个子目录内含 7 个语言字形版本：

- latin —— 泛拉丁语（西文标点）
- zh_cn —— 中文-中国大陆（《通用规范汉字表》写法，简体）
- zh_hk —— 中文-香港（《常用字字形表》写法）
- zh_tr —— 中文-传统印刷
- zh_tw —— 中文-台湾（《国字标准字体》写法，繁体）
- ja —— 日语（《常用汉字表》写法）
- ko —— 朝鲜语

字符覆盖情况（实测）
----
对三个尺寸的 zh_cn 比例字体做了 cmap 实测（使用 fontTools 解析 TTF，结果适用于
同尺寸的全部语言变体，各语言版本覆盖数一致，差异仅在字形写法）：

| 尺寸 | cmap 总条目 | CJK 统一表意文字 | CJK 标点 | 示例句子命中 |
|------|------------|-----------------|---------|------------|
| 10px | 4252       | 1076            | 64      | 25/25      |
| 12px | 24433      | 18299           | 64      | 25/25      |
| 16px | 3214       | 97              | 64      | 4/25       |

示例句子为"我们度过的每个平凡的日常，也许就是连续发生的奇迹。"（含标点共 25
个码点，去重后 23 个字形）。结论：

- **12px 为完整字库**，覆盖 18299 个 CJK 统一表意文字，可放心用于任意常用中文
  文本；代价是单文件约 4.8 MB。
- **10px 为中型子集**（1076 个 CJK），覆盖常用汉字，但生僻字可能缺失。
- **16px 为极小子集**（仅 97 个 CJK），只能覆盖极少数常用字，**不适合渲染任意
  中文文本**。

官方 README 亦明确警告该字体"仍然缺少大量汉字"，并建议：8/10/12px 场景可临时
使用缝合像素字体（fusion-pixel-font，https://github.com/TakWolf/fusion-pixel-font
），16px 场景建议使用 Unifont（https://unifoundry.com/unifont/index.html ）。

踩坑案例：中文渲染成"个个个……"
----
在 settings_scene.c 中最初使用 16px 比例字体加载一句话，运行结果变成：

    个个个个个个个个个个日个, 个个个个个个个个个个个

原因有两层：

1. 字体缺字：这句话的 25 个码点中，16px 字体只包含 4 个（个、日、，、。），
   其余 21 个（我、们、度、过……）在字体中不存在。
2. raylib 的兜底行为：`LoadFontData` 加载字形时，**字体里没有的码点会被静默
   跳过**，不进图集；绘制时 `GetGlyphIndex` 查不到该码点，**一律返回索引 0**，
   即把图集里第一个字形画到每个缺字的位置上。本案例中字体命中的 4 个字（个、
   日、，、。）本身能正常显示，但图集第一个字形恰好是"个"，其余 21 个缺字全部
   画成了"个"，于是整句话几乎全部变成"个"。

排查要点：

- 不要用 `LoadFont` 加载中文：它只生成 ASCII 32..126 的 95 个字形，不包含任何
  CJK 字形。
- raylib 默认字体同样没有 CJK 字形，直接 `DrawText` 中文必然出错。
- `LoadFontEx` 的 codepoints 参数决定图集内容；漏掉某个字符就会触发上述兜底。

正确的加载方式
----
先通过 `LoadCodepoints` 把字符串（UTF-8）解成码点数组，再交给 `LoadFontEx`，
保证用到的每个字符都进入图集。代码示例（与 settings_scene.c 中一致）：

    static const char *kQuote =
        "我们度过的每个平凡的日常，也许就是连续发生的奇迹。";
    static Font QuoteFont;

    static void SettingsInit(void) {
      int codepointCount = 0;
      int *codepoints = LoadCodepoints(kQuote, &codepointCount);
      // 16px ark-pixel 是不完整子集；12px 构建覆盖全部 CJK。
      QuoteFont = LoadFontEx(
          "assets/fonts/ttf/ark_pixel/12px-proportional/"
          "ark-pixel-12px-proportional-zh_cn.ttf",
          36, codepoints, codepointCount);
      UnloadCodepoints(codepoints);
    }

绘制时使用 `DrawTextEx`（按 UTF-8 逐码点绘制），配合 `MeasureTextEx` 居中：

    const float fontSize = 36.0f;
    const float spacing = 1.0f;
    const Vector2 textSize = MeasureTextEx(QuoteFont, kQuote, fontSize, spacing);
    const Vector2 textPos = {(GetScreenWidth() - textSize.x) / 2.0f,
                             (GetScreenHeight() - fontSize) / 2.0f};
    DrawTextEx(QuoteFont, kQuote, textPos, fontSize, spacing, WHITE);

卸载场景时记得 `UnloadFont(QuoteFont)`。

如何选用字体
----
1. 选尺寸：需要可靠覆盖常用中文时，**首选 12px**（完整字库）；10px 用于对文件
   体积敏感、且文本字符都较常见的场景；**避免使用 16px 渲染中文**，除非确认
   所有字符都在其 97 个 CJK 字形内。
2. 选语言版本：简体中文用 zh_cn；繁体按地区选 zh_tw / zh_hk / zh_tr；纯西文用
   latin。
3. 选宽度模式：无特殊要求时用 **proportional（比例模式）**，官方说明其基线位置
   合适、中西文混排观感更舒适；等宽模式用于需要严格对齐的场合。

字号与整数倍缩放
----
方舟像素字体是点阵风格字体，字形按固定像素网格绘制。为了保持像素边缘锐利、
避免出现粗细不均的毛边，**渲染字号应为原生尺寸的整数倍**：

| 原生尺寸 | 推荐渲染字号（整数倍） |
|---------|----------------------|
| 10px    | 20（×2）、30（×3）、40（×4）、50（×5） |
| 12px    | 24（×2）、36（×3）、48（×4）、60（×5） |
| 16px    | 32（×2）、48（×3）、64（×4）          |

- `LoadFontEx` 的 fontSize 参数即最终渲染字号（stb_truetype 直接按该尺寸栅格化
  字形），直接传上面的推荐值，不需要传原生尺寸。
- 使用非整数倍（例如 12px 字体配 32px）会导致不同字符的像素网格不一致，画面
  观感变差。
- 当前 settings_scene.c 使用 12px 字体、36px（12×3）渲染，字号接近默认示例的
  32px 且缩放干净。
- 图集体积与传入的码点数量成正比：只需把**实际用到的文本**的码点传给
  `LoadFontEx`，不要传入整个 CJK 区间（如 0x4E00-0x9FFF），否则图集会被撑大，
  尤其 12px 完整字库有 18299 个 CJK 字形。

验证方法
----
1. 检查字体覆盖（需要 python3 + fontTools）：

       python3 -m pip install --user fonttools
       python3 - <<'EOF'
       from fontTools.ttLib import TTFont
       cmap = TTFont('assets/fonts/ttf/ark_pixel/12px-proportional/'
                     'ark-pixel-12px-proportional-zh_cn.ttf').getBestCmap()
       text = '我们度过的每个平凡的日常，也许就是连续发生的奇迹。'
       missing = [c for c in text if ord(c) not in cmap]
       print('missing:', ''.join(missing) if missing else '无')
       EOF

2. 运行时检查：加载字体后打印 `glyphCount`，并对每个码点调用 `GetGlyphIndex`，
   确认全部落在有效索引上（索引 0 属于第一个字形，注意与"缺字兜底"区分——
   兜底的表现是多个不同码点都返回同一个索引 0）。

3. 离屏渲染检查：用 `BeginTextureMode` 渲染到 `RenderTexture2D` 再导出 PNG，可
   避免 macOS 上截图时机导致的黑屏问题，配合系统 OCR 或人工查看确认字形正确。

速查表
----
- 渲染中文 → 用 12px-proportional-zh_cn，字号取 24/36/48（12 的整数倍）。
- 加载方式 → `LoadCodepoints` + `LoadFontEx(..., 36, codepoints, count)`。
- 绘制方式 → `DrawTextEx` / `MeasureTextEx`。
- 千万不要 → 用 `LoadFont` 或默认字体画中文；用 16px 字体画中文；非整数倍缩放。
