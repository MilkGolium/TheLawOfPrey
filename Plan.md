# 中文字体加载方案

目标：为游戏内所有中文文字提供一套兼顾性能、效率与轻量资源占用的字体加载方案。

## 关键事实

以下三点由实际检查得出，直接决定方案形状。

- `assets/fonts/ttf/zpix.ttf` 是全量字体，共 22240 个字形，其中 CJK 基本区 20902 个、
  半形与符号 687 个，文件大小 6.9MB ，`unitsPerEm = 1200` 。
- 项目可见文字的来源有限且可枚举。 `assets/database/items.csv` 整文件共有 593 个
  唯一字元；其中三栏（ `display_name` 、 `description` 、 `inspect_text` ）合计 539 个。
  两种口径下 CJK 均为 528 个，不影响后续估算。
- raylib 6.0 没有 `SetFontDefault` ，只有 `GetFontDefault` 。因此 `DrawText()` 永远使用
  内置默认字体，画中文必定显示为方框。所有文字必须走 `DrawTextEx` 与 `MeasureTextEx` ，
  没有替代做法。

## 成本对比

估算值，按 padding=4 计算，分「贴图紧密排列」与「强制 2 冪次」两种情况。

| 方案 | 字形数 | 12px 图集 | 24px 图集 |
| --- | --- | --- | --- |
| 全量加载 | 22240 | 约 21 MB（4096²） | 约 66 MB，强制 2 冪次需 256 MB |
| 常用 3500 字加 ASCII | 约 4200 | 约 4 MB | 约 13 MB |
| 精确收集 1000 至 1500 | 1000 至 1500 | 约 1 至 1.5 MB | 约 3 至 4.5 MB |

全量加载还有第二代代价：stbtt 光栅化 22240 个字形会让启动卡住 0.5 至 2 秒，且 8192×8192
的单张贴图并非所有 GPU 都保证支持。精确收集只需几十毫秒。

## 方案

### 收集

启动时扫描所有玩家可见文字来源，取唯一 codepoint ：

- `assets/database/*.csv` 的 `display_name` 、 `description` 、 `inspect_text` 三栏
- 未来的 locale 与字符串文件
- ASCII 32 至 126 以及常用标点作为缓冲

UTF-8 解码直接使用 raylib 现成的 `LoadCodepoints` 与 `GetCodepointNext` ，不必自行实现。

### 加载

- `LoadFontEx("assets/fonts/ttf/zpix.ttf", 24, cps, count)` 加载一次
- `SetTextureFilter(TEXTURE_FILTER_POINT)` 保持像素锐利
- `GuiSetFont()` 同步给 raygui ，此步不可遗漏
- 尺寸取 12 的倍数（12、24、36）。`upem = 1200` ，12 的倍数正好对齐字型的像素格
- 1920×1080 下正文用 24px 。若背包格子需要 12px 数字，再加载一个 12px 图集（面积仅为
  24px 的四分之一），而不是缩放 24px 图集，缩放会破坏像素格对齐

### 缺字自愈

`GetGlyphIndex` 找不到字形时会静默回退到 `'?'` ，漏字不会自动暴露。因此：

- 将收集到的 codepoint 保存为排序数组，提供 `Font_HasGlyph(cp)` 查询
- 启动时对每个字符串来源扫描一遍，缺字以 `TraceLog(LOG_WARNING)` 输出
- 开发期补进字符串文件即可，流程会自行收敛

## 性能红利

- 单一 draw batch：raylib 按贴图分批，整个 UI 共用一张图集，等于一次贴图切换
- 常驻一份：Font 全域加载一次，场景切换不重载，退出时 `UnloadFont`
- 内存固定：约 3 至 5MB 显存，与 zpix 文件大小无关。6.9MB 的字体文件只在加载瞬间读一次

## 明确不做的事

- 不使用 SDF 字体（raylib `LoadFontData` 的 SDF type）：需要自定义 shader，且会让像素字
  失去锐利度，而本方案只做整数倍缩放，用不到它的平滑缩放优势
- 暂不做分页字体（以 codepoint 区间切分的 `Font` 数组）：只有当出现无法预先收集的动态
  文字（玩家输入、随机文本）才需要。现在实现属于过度设计，且触发条件明确，之后补上不影响
  架构
- 不预载常用 3500 字：除非动态文字需求真的出现，届时再加入，成本约 13MB

## 注意事项

- raygui 的 `GuiSetStyle(DEFAULT, TEXT_SIZE, ...)` 默认为 10 。换用 24px 字体后必须同步
  调整，否则按钮文字会过小或被裁切
- 中文字符串应放在数据文件而非写死在 C 字面量中，否则精确收集会遗漏，且日后做多语言会很
  困难
- 不要在场景中重复调用 `LoadFontEx` ，字体必须全域共用一份

## 模块落点

新增 `src/font.c` 与 `src/include/font.h` ，对外只暴露：

- `InitGameFont()` ：收集与加载，含缺字检查
- `GetUIFont(size)` ：取得指定尺寸的字体
- `DrawUIText()` 、 `MeasureUIText()` ：封装 `DrawTextEx` 与 `MeasureTextEx`
- `Font_HasGlyph(cp)` ：缺字查询

这样可以把「raylib 没有 SetFontDefault 」这件事封装在模块内部，其余代码一律使用本模块的
接口。

## 实施步骤

- 建立 `font.h` 与 `font.c` ，实现收集、加载、查询与绘制封装
- 在 `src/main.c` 的 `InitAssetsDirectory` 之后调用 `InitGameFont()` ，退出前
  `UnloadFont`
- 同步调整 raygui 的 `TEXT_SIZE`
- 将现有中文字符串移到数据文件，用缺字检查验证覆盖率
- 在 `scripts/build.sh` 的 Release 与 Debug 两个配置下确认构建与启动均正常

## 验收标准

- 启动时无缺字警告
- 代码中不存在直接调用 `DrawText` 与 `MeasureText` 的地方
- 显存中只有一张（或按尺寸各一张）字体图集
- 启动到显示画面的耗时无可感知的停顿
