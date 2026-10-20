# The Law Of Prey

## 项目简介
正在构建程序基础，游戏内容待定。

因游戏内容待定，所以项目名称为临时设置，未来可能会修改。

## TODO

当前阶段的目标：搭建游戏底层，为未来制作 MVP 打基础。
当前正在完善 UI 层面的事情。

场景结构约定：`Init()`、`Update()`、`Draw()`、`Unload()`。

场景不得在回调里切换自己：只允许调 `SceneManager_RequestSceneSwitch()` 提出请求，由主循环在任何场景回调之外统一执行。退出程序同样只提请求（`AppRequestQuit()`），不在场景里直接关窗口。

需要确认的条目统一放在「待定」。

### 地图与数值

- 确定世界生成方案：地图由程序噪声随机生成（原理参照《饥荒联机版》），不存在手写关卡文件。需拍板三件事：① 生成参数与对外接口（seed 、世界尺寸、分块大小、生成阶段顺序）；② 图块枚举与可通行属性表，作为解析与出图的共同依据；③ 存盘格式（只记 seed 与玩家改动增量，不存整张地图）。
- 起草游玩界面布局：背包格子、状态栏（饥饿 / 体力）、交互按钮的位置与操作方式。
- 梳理 `assets/databases/items.csv` 字段：确认现有列是否够用（如堆叠上限、重量、食用恢复量），补齐需要的字段，交给代码解析。
- 编写第一版数值：饥饿消耗速度、食物恢复量、基础移动速度。

### 界面与字体

界面层为自建（`src/ui.c` 与 `src/include/ui.h`），不依赖任何第三方 GUI 库；度量约定见「字体与界面方案约束」。当前没有待处理项，以下为变更后必须保持的不变式。

变更后必须保持：启动时无缺字告警；除 `src/font.c` 的封装内部外，代码中不直接调用 raylib 的 `DrawText` 、 `DrawTextEx` 、 `MeasureText` 、 `MeasureTextEx` ，一律走 `DrawUIText` 与 `MeasureUIText` ；显存中每个字体尺寸只有一张图集；Release 与 Debug 两个配置均构建并启动正常；行距与居中公式由 `tests/ui_tests.c` 的用例守住（见「测试」）。

### 主题

配色系统已实现并接入 `InitGameFont()` 之后的启动流程，公开接口为 `InitTheme()` 、 `GetThemeCount()` 、 `GetThemeName()` 、 `LoadTheme()` 、 `GetCurrentThemeName()` ；当前 11 个配色键与 2 个枚举键（`border_style` 、 `shadow_style` ），格式说明见 `assets/themes/default.cfg` 。解析器的行为（BOM 、 CRLF 、坏十六进制、未知键、禁用键、枚举键、缺 `default.cfg` 的回退等）已由 `tests/theme_tests.c` 覆盖，见「测试」。以下为待处理项。

- 主题选择：`InitTheme()` 启动时载入的始终是 `default.cfg` ，其余 `.cfg` 只被扫描枚举（`GetThemeCount` / `GetThemeName` ），也可由 `LoadTheme` 按名加载。**留到设置界面再做**：确定玩家侧的主题切换方式，并在做设置界面的同一批改动里落地。在此之前不要**为选择主题**另开命令行参数、环境变量或 `active.cfg` 标记文件，以免多出一条会与界面互相打架的选择通路；`--scene` 一类的启动参数是开发工具（见「命令行参数（开发工具）」），不参与主题选择。配置文件的 `theme` 键是那条通路的**读侧**（见「配置文件」）：启动时按名字校验，认不出就告警并保持缺省，写回等设置界面上线再定。

### 游玩闭环

- 场景清单：`title_scene.c`（主界面，启动场景）、`settings_scene.c`（设置界面，当前为占位）、`test_scene.c`（测试场景）、`game_scene.c`（游玩界面，未开始）。`test_scene.c` 由原临时示例场景 `main_scene.c` 改名而来，保留 UI 原语与消息框演示；在游玩界面出现之前，它一直是「开始游戏」的落点与跳转测试目标，需要临时验证什么就直接加在它里面。
- 游玩场景骨架：新增 `src/scenes/game_scene.c` ，引入 Camera2D 、按世界生成的分块数据渲染地砖、摄像机跟随玩家。
- 玩家移动与碰撞：补全 `src/include/player.h` 的坐标与速度，WASD 移动，被不可通行地砖阻挡。
- 物品数据层：解析 `assets/databases/items.csv` ，按需加载物品图标并做缓存。
- 保持 `scripts/build.sh` 的 Release / Debug 双配置均可构建。

### 素材

- 物品图标：按 `48*48` 制作，路径与命名规范见「素材规范」。当前已到位一部分，其余待美术补齐；缺失与大小写不符可用 `python3 scripts/check_assets.py` 核查。
- 统一图标规范：底色未定；命名与扩展名大小写已定，见「素材规范」。
- 出一版地砖图块集：与图块枚举逐项对应，含草地、树木、石块等。

### 待定

- 上面三块骨架（游玩场景、玩家移动、物品数据层）的落地顺序，等世界生成方案（图块枚举与可通行属性表）定稿后确定。


## 字体与界面方案约束

以下为已定结论，实现时不要反向引入。

- 字体全域只有一份，`LoadFontEx` 只能在 `InitGameFont` 里调用，场景中禁止重复加载，场景切换也不重载。
- 图集尺寸只能是 12 的倍数（12 、 24 、 36），像素格才对得齐。注意 raylib 的 `LoadFontEx` 按 hhea 行高（本字体 `asc-desc = 1600`）而非 `upem = 1200` 缩放字形，故传入值要乘 `4/3`：对外字号 12/24/36 对应传 16/32/48；直接传 12/24/36 会得到每设计像素 0.75/1.5/2.25 个像素，栅格化落在半像素上，图集带灰边。`src/font.c` 以 `kFontSizes`（对外）与 `kFontLoadSizes`（传 raylib）区分两套数值，绘制与度量一律用 `Font.baseSize` 使缩放系数保持 `1` 。缩放图集会破坏像素格对齐；背包格子需要 12px 数字时另加载一张 12px 图集，而不是缩放。
- 文字一律走 `DrawUIText` 与 `MeasureUIText` ，两者内部封装 `DrawTextEx` 与 `MeasureTextEx` 。raylib 6.0 没有 `SetFontDefault` ，`DrawText()` 永远使用内置默认字体，画中文必定是方框，没有替代做法。
- 图集贴图保持 `TEXTURE_FILTER_POINT` ，不要改成线性过滤。
- 字符串放数据文件，不写死在 C 字面量里，否则精确收集会漏字，日后做多语言也无从下手。
- 缺字自愈不能省：`GetGlyphIndex` 找不到字形时会静默回退到 `'?'` 。保留启动时的缺字扫描与 `TraceLog(LOG_WARNING)` 输出，缺字在开发期补进字符串文件即可收敛。
- `src/font.c` 对外只暴露 `InitGameFont()` 、 `UnloadGameFont()` 、 `GetUIFont(size)` 、 `DrawUIText()` 、 `MeasureUIText()` 、 `FontHasGlyph(cp)` ，接口名一律大驼峰。
- 不引入任何第三方 GUI 库，`src/raygui/` 已整体删除。历史原因：raygui 的控件高度按默认 `TEXT_SIZE = 10` 硬编码，`InitGameFont` 把字号提到 24 之后这些常量没有跟着放大；叠加像素字在 24px 下 CJK 墨迹底部贴死行盒底边，控件内高不足时 `TEXT_ALIGNMENT_VERTICAL` 的居中计算会把文字推出边框。
- 界面尺寸一律为整像素；**坐标与尺寸的默认节距是 4 ，不是 12**。12 是字体的像素格单位（一个汉字格 12×12），只在「需要装整数个汉字」的地方要求（背包格、正文区、状态栏这类按字符格算的场合）。理由是算术事实：480 = 12×40 可被 12 整除，640 不能（640/12 = 53⅓），gcd(640, 12) = 4 ，横向天然节距只有 4 ，所以「严格居中」与「12 对齐」在 640 宽下不可兼得——居中类布局以数学解为准。12 的倍数天然是 4 的倍数，故既有取值全部继续合法。内部留白统一取 `UI_PADDING` ，按钮高度取 `UI_BUTTON_HEIGHT()` ，`UI_ALIGN12()` 只用于向字符格收敛，度量不写死数值。
- 多行文本由 `src/ui.c` 逐行绘制，行距为 `UI_LINE_STEP(size)` （等于字号）。raylib 的 `MeasureTextEx` 行间步进是「字号 + 2」（本封装传入的 raylib 字号为 16/32/48，对应 18/34/50），既不是字号也不对齐像素格，且 `spacing` 参数只管行内字符的水平间隔，调不动它。换行只认显式 `'\n'` ，不做自动折行，也不做宽度上限截断。
- 垂直居中一律按**主体块**算（`src/ui.c` 的 `TextBodyBounds`）：从墨迹上缘量到**基线**，**不含下伸部**。下伸部（拉丁 `g`、`y`）会把整串的墨迹盒往下撑长，按墨迹盒居中会让字母主体偏上——英文标题 `Message Box` 按旧算法（整串墨迹盒居中）时主体是 2 行黑在上、6 行在下，正好偏一个像素方块。基线取字形底边的**众数**：下伸字形是少数，取最大值会被它们拉低整串的视觉中心。没有下伸部的串（中文、`OK`、`Close`）主体块与墨迹盒完全一致，版面一个像素都不动。
- **空白字形不参与**主体块：本字体的空格在对外字号 24 下带一个整行盒大小的矩形（`offsetY=0`、高 32），只要文案里有一个空格，量出来的盒就会被撑成整个行盒。两条规则都写在 `TextBodyBounds` 里，改这个函数时不要去掉。
- 按钮的焦点是唯一的变色来源：持有焦点时前景与背景互换，鼠标悬停与按下不产生任何视觉变化。焦点归属由调用方管理，原语本身不保存跨帧状态。
- 主题只管配色与装饰形态，不管字体度量。`assets/themes/*.cfg` 中的 `text_size` 、 `text_spacing` 、 `text_line_spacing` 、 `text_alignment` 、 `text_alignment_vertical` 、 `text_padding` 、 `border_width` 由程序独占，写了会被忽略并告警；配色统一从 `src/ui.c` 的 `gUiStyle` 取。`border_style` 是装饰，不触碰度量，因此允许主题设置，这与 `border_width` 被禁的理由不同。
- 边框圈数由 `border_style` 决定（`none` / `single` / `triple`），三色固定为 `line` / `lineDim` / `line` 的浅-深-浅，不支持反向的深-浅-深。**圈数同时是文字要避让的垂直内缩量**：标题栏只占框顶 `UI_TITLEBAR_HEIGHT(size)` 行，框底三圈与它不相交，只有顶部的 ring0 会压到标题，因此内区是 `标题栏高 - 圈数` 而不是 `- 2 * 圈数`；`BorderRingCount()` 由画边框与两处文字居中公式共用。居中若按整块高度算（`y + (h - inkHeight) / 2`），墨迹上缘必然落进 ring0，且标题文字画在边框之后，会反过来把 ring0 啃出缺口。按渲染回读实测，「标题测试」四字的墨迹并集高在 12/24/36 号下为 11/22/33，三色边框后余量为 0/1/2px：12 号零余量、字顶字底都贴边，故 `triple` 建议只在 24 号及以上使用（放得下，只是没有呼吸空间）。墨迹高随标题用字而变，换更密的字可能更高，所以内缩量不能写死成常量。不要为此调大 `UI_TITLEBAR_HEIGHT`，否则 1px 与 none 两种边框会平白多出留白。
- 阴影形态由 `shadow_style` 决定（`auto` / `solid` / `dither`），偏移固定为 `UI_SHADOW_OFFSET`（8 设计像素），作用范围是 rect、button、box 三类原语。`auto` 在 `ApplyThemeFile` 写回 `gUiStyle` 之前按 `desktop` 亮度定死成 `solid` 或 `dither`（阈值 128，`Luminance` 取整数近似），渲染层只做 `switch` 不再判断。抖动阴影是一张 2×2 棋盘贴图配 `TEXTURE_WRAP_REPEAT`，颜色由 `tint` 给出而不烘进贴图，因此主题换色不必重建贴图；`SetTextureWrap` 必须先于 `SetTextureFilter(POINT)`，否则过滤被重置为线性而抖动被重采样糊掉。贴图懒加载，`UiUnloadStyle()` 幂等可重入，须在 `CloseWindow` 之前调用。
- 阴影偏移 8 不需要满足任何整除约束：它既不移动字形也不改控件尺寸，而整数倍缩放配 `TEXTURE_FILTER_POINT` 下每个设计像素都映射成干净的 N×N 块，实测偏移 1 至 12 均产生 0 个混合像素。上限是 `UI_PADDING`（12）而非审美偏好——超过它会让阴影越出消息框边框并压到相邻控件上。
- 阴影会被画布裁掉，而 `UiMessageBox` 原先没有任何尺寸上限。居中公式 `x = (640 - boxWidth) / 2` 要求 `x + boxWidth + 8 <= 640` 解得 `boxWidth <= 624`，高度同理得 456，两者都是 12 的整数倍（`kMaxBoxWidth` / `kMaxBoxHeight`）。clamp 只保证阴影不被裁掉，不截断正文：按上条「不做宽度上限截断」的约定，超长文本由调用方负责分行。
- 以下为有意设计，不要改回朴素实现：codepoint 用位图去重 + 升序数组 + 二分查询；按尺寸懒加载 + `initialized` 幂等 + `UnloadGameFont` 可重入；`GetCodepointNext` 返回 0 时的防死循环保护。
- 判字体是否可用只能用 `FontIsUsable(Font)`。`LoadFontEx` 有两种失败形态：文件读不到时返回零值 `Font`（`recs` 为 NULL ，再访问 `recs` / `glyphs` 会越界——`ui.c` 的墨迹测量曾因此在首帧段错误），TTF 解析失败时 raylib 会**静默退回内置默认字体**（那份 `Font` 的 `texture.id` 不为 0）。只查 `texture.id` 会漏掉后一种，把默认字体当图集用、中文照样变方框；任何消费 `Font` 内部字段的代码都必须先过这个判据。同理，codepoint 尚未收集完成时 `GetUIFont` 拒绝加载并告警一次，否则 `LoadFontEx` 只拿到 ASCII 。
- 收集范围固定为 BMP（`BMP_RANGE`）。Fusion Pixel 有 1429 个码点在 BMP 之上（U+16FF2 - U+30F91），当前数据文件没有用到，所以该上界仍够用；但「字体不含 BMP 外字形」已不再是可依赖的前提。越界码点由收集阶段一次性告警，且**不得**计入缺字——这类字无论怎么补数据都进不了图集，报成缺字会把排查带偏。
- 界面文本一律放 `assets/strings/<lang>/<scene_name>.csv`（key,text 两列带表头），本地化结构见「本地化（界面文案）」。字体 codepoint 是精确收集的，写死在 C 字面量里的中文不在收集范围内，直接变方框且无提示；`uiChars[]` 那类硬编码已删除，不要重新引入。取文案用 `GetString()` ，缺 key 时回退为 key 本身而不是空串，让漏配在界面上直接可见。
- CSV 解析全仓只有 `src/utils.c` 的 `CsvReadFile` 一份实现，不要再写第二个解析器。字段超过 1024 字节会被截断并告警：收集与检查曾共用同一条截断路径，被截掉的字既不进图集也不报缺字，漏字完全无声。引号字段不跨行，与非引号字段共用同一条行结束规则（本项目 CSV 由程序生成，没有字段内换行的需求）。
- 明确不做：不使用 SDF 字体（需要自定义 shader，且会让像素字失去锐利度，本方案只做整数倍缩放，用不到它的平滑缩放优势）；不预载常用 3500 字（精确收集已足够，除非真的出现无法预先收集的动态文字如玩家输入或随机文本，届时再加入，成本约 13MB）；暂不做分页字体（按 codepoint 区间切分的 `Font` 数组，触发条件明确，等真的出现动态文字再补，不影响现有架构）。

### 本地化（界面文案）

- 文案目录 `assets/strings/<lang>/<scene_name>.csv` ：**每个语言一个目录，目录内按场景一个 CSV**，同一语言内文件名与 key 必须完全一致，只有内容不同。
- `<lang>` 用 BCP-47 的小写形态（当前 `zh-hans` / `en`），**标识与目录名必须逐字符一致**：macOS 的文件系统大小写不敏感，写错了在本机跑得通，到 Linux/CI 上会直接找不到目录。合法标识只在 `src/lang.c` 里存一份。
- 文案目录路径的唯一来源是 `LangStringsDir()` （`src/include/lang.h`）：运行时载入文案（`strings.c`）与字体码点收集（`font.c`）都问它。**这两处曾经各写一份 `#define STRINGS_DIR`，这种复制必须避免**——一旦岔开就会出现「图集按中文收集、界面按英文渲染」。
- **语言决定字体图集的码点收集范围**，所以 `LangSet()` 必须在 `InitGameFont()` 之前调用。运行中切换语言需要重载图集（`UnloadGameFont()` + `InitGameFont()`），第一版不支持；设置界面的语言切换与主题切换同批做，并与持久化配置共享通路。
- 语言目录缺失不是致命错误：文案回退成 key 本身，缺字检查照常告警。
- 数据表（`assets/databases/`）不进这套结构，物品名的本地化是独立话题。
- `assets/strings/en/` 下的英文目前是占位稿，日后补正。
- **控件尺寸必须容得下自己的文案，不许写死**：界面层不折行也不截断，写死的宽度在换语言后会让文字从两侧溢出边框（英文普遍比中文长）。算宽度的样板是 `UiMessageBox` 摆按钮行时用的那条：`UI_ALIGN12(UiMeasureText(text, size).width + 2 * UI_PADDING)`。菜单一类要求按钮等宽时属于设计选择，取**最宽的那条文案**即可。
- **英文文案要按画布宽度反推长度**：界面层不做自动折行也不做宽度截断（既有约定），而同一句话的英文通常比中文长 2 至 4 倍。实测设置页那句占位文案在 24 号字下已经占掉 640px 画布的约九成宽，再长就会顶出屏幕。按 24 号字约每字符 12px 估算，单行上限约 50 个 ASCII 字符（含空格）。

## 键位与输入约定

以下为已定结论，实现时不要反向引入。

- **游戏窗口不允许被游戏内键位关闭。** `SetExitKey(KEY_NULL)` 保持禁用，退出一律靠程序自身逻辑或系统级方式（Alt+F4 、Command+Q）。
- **Ctrl+Q 是「返回」**：设置界面 → 标题、测试场景 → 标题（若当前有游戏内窗口打开，先关它）、标题 → 正常退出。实现为 `IsKeyPressed(KEY_Q)` 配合左右任一 `IsKeyDown` 。
- **ESC 是「打断当前行为 / 关闭游戏内 UI 窗口」**，不参与场景导航，也永远不关游戏窗口。`UiMessageBox` 早已如此（按 ESC 返回 -1）。
- 焦点用上下键循环移动，回车激活；鼠标点击只在按下并松开于同一控件内时激活，**不改变焦点**（见「字体与界面方案约束」里按钮焦点那条）。
- **Tab 前进、Shift+Tab 后退，方向键另有空间语义，两者并存。** 方向键跟着控件在屏幕上的排布走（竖排用上下、横排用左右），Tab 按焦点链走一圈、与排布方向无关，两组都是循环移动。判据收在 `UiFocusNextPressed()` / `UiFocusPrevPressed()`（`src/ui.c`），左右任一 Shift 都算 Shift+Tab，按住 Shift 的那一次 Tab 边沿只算后退、两个判据不会同时为真。循环取模与焦点归属都在调用方：标题场景用上下键加 Tab（`src/scenes/title_scene.c` ），消息框用左右键加 Tab（`src/ui.c` 的 `UiMessageBox` ）。设置场景没有可聚焦控件（只有 Ctrl+Q 返回），不接这两组键。
- **一次按键边沿在同一帧里对所有控件都可见**：打开模态框的那一帧不要再运行框本身，否则框里的默认按钮会吃掉同一次按键，框刚开就被关掉（表现为「闪一下就没了」）。场景侧的做法见 `src/scenes/test_scene.c` ：`showMessageBox = true;` 之后直接 `return`，下一帧再画框。
- 程序内退出（Ctrl+Q 或按钮）与系统关窗（Alt+F4 / Command+Q）是两条路：前者走正常卸载顺序并返回 0 ，后者让 `WindowShouldClose()` 变真；连同「帧导出完成」，三个出口汇合到同一套卸载顺序。

## 渲染分辨率与整数缩放

以下为已定结论，实现时不要反向引入。

- 设计分辨率固定为 `640*480`（`GRAPH_DESIGN_WIDTH` / `GRAPH_DESIGN_HEIGHT` ）。游戏逻辑与界面一律画在这张画布内，`DrawCanvas()` 再整数倍居中放大到窗口，未覆盖部分为黑边。整数倍是硬要求，非整数倍会让像素字在液晶上被插值模糊。
- 必须设置 `FLAG_WINDOW_HIGHDPI` ，且必须在 `InitWindow` 之前。Retina 屏的逻辑分辨率是物理像素的一半，不开这个标志时帧缓冲只有窗口点尺寸，系统会把它平滑放大到物理像素，文字在应用之外就已经糊了，采样过滤救不回来。实测 4K 屏（逻辑 1920*1080）开启后帧缓冲为 3840*2160 ，与面板 1:1。
- 画布贴图保持 `TEXTURE_FILTER_POINT` 。实测同一份字体图集在 2 倍放大下，点采样与线性插值的中间灰度占比为 52% 对 80% ，线性会直接糊掉像素字。
- 窗口默认无边框窗口全屏（`ToggleBorderlessWindowed` ）。16:9 屏上 4:3 内容两侧留黑边，这是已接受的取舍。
- 界面层鼠标命中测试必须走 `GetCanvasMouse()` ，不能用 `GetMousePosition()` ：后者是窗口坐标，画布放大居中后偏置与倍数都不对。同理 `UiMessageBox` 居中用设计分辨率，不能用 `GetScreenWidth()` ，后者返回窗口尺寸而非画布尺寸。

### 目标显示设备与老显卡底线

以下同为已定结论，实现时不要反向引入。原始决策记录见 `~/Downloads/RENDER.md` 。

- **素材面向液晶创作，CRT 交给显示层。** CRT 没有像素网格，它是连续的模拟面，分子就是光点尺寸，天生是模拟缩放器；整数缩放纪律是液晶专属概念。历史上「为 CRT 作画」唯一真实存在的技术是有限调色板下的抖动混色，本项目 24 位全色 + 干净像素画，没有调色板约束，该需求不存在。因此素材与 UI 一律按干净整数像素网格创作，CRT 观感（扫描线、辉光、弧面、三色栅）全部由显示层后处理提供。
- **CRT 实机以原生 640×480 输出**，由管子自己放大，是最原汁原味的 DOS 呈现；也可以让 GPU 先算 1.6× 再输出，模拟滤波会抹平瑕疵。「支持 1024×768」只需显示链路不崩溃——那是显示模式而非画布，游戏仍以 640×480 运行。
- **不做 CRT 滤镜**（2026-10-01 拍板）。液晶上本来就正常、不会露馅，CRT 原生输出本来就完美，没有玩家提出诉求则不立项。若日后改变主意再做：mask 分型是着色器的一个 uniform 参数而非两份 shader，用 `CrtMaskType { MASK_APERTURE_GRILLE, MASK_SLOT, MASK_SHADOW }` 枚举出预设即可（RetroArch 系 crt-royale / crt-guest 即此做法）。
- **显示模式必保 640×480 、 800×600 、 1024×768**。1024×768 不能砍，它是许多家庭普通 CRT 普遍支持的分辨率。高于 1024×768 的档位（1280×960 、 1280×1024 、 1600×1200 、 1856×1392 、 1920×1440 、 2048×1536）大多冷门，且非整数缩放问题会随倍数放大而更明显，嫌麻烦可砍。
- **老显卡底线**：目标老显卡为 PCIe 的 GT620 （Fermi，GL 4.2）与 GT730 （Kepler，GL 4.5），均高于 raylib 的 OpenGL 3.3 底线，**无需 NGGL 分支**。nearest 过滤 + 像素贴图是显卡最原始的能力，素材为小尺寸像素图，无压力。
- **禁忌：不开 MSAA 、不用 mipmap。** 两者都会模糊像素边缘，与像素画哲学直接冲突。

### 两个坐标系陷阱（改渲染代码前必读）

- `GetRenderWidth()` / `GetRenderHeight()` 返回**当前 render target** 的尺寸，不是恒定的物理尺寸：`BeginDrawing` 下是 backbuffer 物理尺寸，`BeginTextureMode` 下是 RT 自身尺寸（本项目为 640×480 逻辑像素）。**整数倍缩放必须用逻辑的 `GetScreenWidth()` / `GetScreenHeight()` 计算**——误用 `GetRenderWidth()` 会把 scale 多算一个 renderScale 倍，dest 矩形画出一倍并溢出画面。
- `BeginTextureMode` **不改变** `GetScreenWidth()` / `GetScreenHeight()` ，只让 `GetRenderWidth()` / `GetRenderHeight()` 变成 RT 尺寸。RT 上下颠倒，`DrawTexturePro` 的 source 矩形高度须取负值翻正。
- 几何探针不能代表文字：矩形与字形走的是两条不同的光栅化路径。Retina 糊字曾有两个互相独立的根因，只修一个仍会看到糊。

## 平台支持
只确保 UNIX-LIKE：FreeBSD 、 Linux 、 macOS 。

**Windows 已移除支持**，原因是两条：没有 Windows 机器可供测试，脚本与产物都没法验证；弄不到代码签名证书，编译出来的可执行文件会被 SmartScreen 拦截，交付出去别人也跑不起来。原先的 `scripts/build.bat` 与 `scripts/build.ps1` 已删除。

OpenBSD 在构建项目时遇到一个头文件缺失问题，据说可以通过传入路径解决，而且该路径下的确发现了缺失的头文件；**具体情况正等待分析**——既没有列为支持，也没有列为明确不支持，等有机器能测时再定。

### 构建前置要求

`scripts/build.sh` 与 `scripts/run_tests.sh` 在动手之前会先跑一遍前置检查（实现在 `scripts/preflight.sh` ），按 cmake → ninja → C 编译器 的顺序逐项检查，任一项不达标就打印缺什么与按发行版分组的安装办法，然后以非零码退出。需要的工具：

| 工具 | 要求 |
|------|------|
| cmake | ≥ 3.16 ，推荐 3.25 以上 |
| ninja | 任意版本；脚本固定使用 Ninja 生成器 |
| C 编译器 | `cc` / `clang` / `gcc` 任一在 `PATH` 里即可 |
| raylib | 当前在 raylib 6.0 上开发 |

- cmake 的最低版本按「发行版自带的就够用」定档：Ubuntu 20.04 、 Debian 11 、 RHEL 9 这一档自带的 cmake 都已到 3.16 以上，而本项目只用到 3.16 之前就有的功能。发行版仓库里的 cmake 往往太旧，达不到这个下限。
- ninja 是硬要求，**不做 Make 回退**：`build/` 目录一旦被某个生成器配置过，换生成器会直接报 generator mismatch，与其逼人删目录，不如直接报错。
- raylib **不在前置检查里**：`CMakeLists.txt` 已有一条带安装办法的 `FATAL_ERROR` 提示，重复一遍必然与它分叉。
- **脚本不会自动安装任何依赖**，也不会代跑 `sudo`；提示里给出的命令请自行执行。

## 设计规范
普通物品最大堆叠 60 ，特殊物品最大堆叠 10 。

## 代码及文档规范
- 交流与注释中的路径表示约定：默认从项目根目录起始，例如根目录下的 `.clang-format` 表示为 `.clang-format` ，`main.c` 在 项目根目录 `/src/main.c` ，所以表示为 `src/main.c` 。
- 代码中不要使用 emoji ，注释不要包含 12345 类似的编号（ AI 经常这样做）；允许 ASCII 和中文汉字。
- `scripts/` 下的脚本（sh 与 py）**输出与注释一律英文**：FreeBSD / Linux 的 tty 默认没有中文字形，中文会显示成方块或乱码。C 代码的注释不受此限，因为它不进入终端。
- C 代码使用 Google Style ，限宽 80 字符，配置已经写在了 `.clang-format` 里面。
- Google Style 中常量/只读变量以 k 开头（Constant），全局变量以 g 开头（Global），静态变量以 s 开头（Static）。
- 文件名使用蛇形规范，例如 `main_scene.c`, `scene_manager.c` 。
- 函数、结构使用大驼峰命名，例如 `MainScene->Init()`, `SceneManagerGetCurrent()`, `MainScene` 。

## 测试

回归测试在 `tests/` ，用 CMake 的 `ctest` 跑，自写断言，不引入任何第三方测试框架。

```bash
bash scripts/run_tests.sh    # 构建 Debug 并跑全部用例
```

当前三个测试程序：

| 目标 | 覆盖 |
|------|------|
| `theme_tests` | 主题解析器：BOM 、 CRLF 、坏十六进制、未知键、禁用键、枚举键、缺 `default.cfg` 的回退等 |
| `config_tests` | 配置文件解析器：缺文件、正常文件、未知键、坏 bool 、坏 int 、越界 int 、重复键、注释与空行与 CRLF 、显式路径不存在、运行期用错键名、权限探测、只读不回写 |
| `ui_tests` | 界面度量与居中：行距公式、边框不吃文字、按钮与标题的墨迹居中 |

约定与坑点：

- 测试与本体共用 `CMakeLists.txt` 里同一套编译选项（`configure_project_target`），不要给测试单独放宽警告；双配置都要零警告。
- **断言必须能失败**。新增或修改断言后，故意改坏对应实现确认它真的红，再还原；只跑一遍「通过」不算验证。
- **判断位置要用对称量或绝对值，不能只判「没有越界」**。例：按钮文字的内缩量算错时墨迹整体下移，既不进 ring0 、像素数也不变，只判这两条会全部放过；改判「墨迹在边框内区的上下余量之差不超过 1px」才拦得住。
- **回读像素时扫描窗口要比目标区域放宽**。窗口正好等于目标区域时，位置算错的墨迹会被窗口截断，量出一个偏小的尺寸反过来掩盖错误。
- **`SetTraceLogCallback` 拿到的是原始格式串与 va_list**（raylib 6.0 如此，且 `logType < logTypeLevel` 的过滤发生在回调之前），捕获告警的回调必须自己 `vsnprintf` 。
- **raylib 6.0 没有 `RemoveFile`** ，测试清理临时文件用 C 标准库的 `remove()` 。
- 测试只在 `tests/tmp/` 下写临时文件（已 gitignore），不得在 `assets/` 下留任何东西；`THEME_DIR` 的编译宏覆盖只用于测试目标，游戏本体仍只认 `assets/themes` 。配置文件的位置由接口给出（`ConfigSetPathOverride`），配置测试因此同样只在 `tests/tmp/` 下造文件。

## 配置文件

持久化设置放在一个 `key = value` 的文本文件里。**本阶段程序只读，绝不写回**：
回写与注释保留等设置界面上线时再定，因此手改这个文件不会丢。

### 位置

| 平台 | 路径 |
|------|------|
| macOS | `$HOME/Library/Application Support/TheLawOfPrey/config.cfg` |
| Linux / FreeBSD | `${XDG_CONFIG_HOME:-$HOME/.config}/TheLawOfPrey/config.cfg` |

`XDG_CONFIG_HOME` 为空时按 `$HOME/.config` 展开；`HOME` 也取不到时退回当前工作
目录并打一条告警，不会崩溃。`--config=<path>` 可以指定别的文件。

### 键

键名、类型、默认值与合法范围只在 `src/config.c` 的键表里登记一份，设置界面
上线后由同一张表生成控件，界面与文件格式因此不会分叉。键名**大小写敏感**，
与键表逐字符比较。

| 键 | 类型 | 默认 | 范围 / 取值 |
|---|---|---|---|
| `fullscreen` | bool | `true` | `true` / `false` |
| `target_fps` | int | `144` | 30..240 |
| `theme` | string | `default` | 主题名（应用时校验） |
| `lang` | string | `zh-hans` | `LangIsKnown` 认可的语言标识（应用时校验） |

`theme` 与 `lang` 在**应用时**校验：`theme` 要能在扫描到的主题列表里找到，
`lang` 要过 `LangIsKnown`；认不出就告警并保持缺省，不静默改写。

### 格式

- 每行一条 `键 = 值`，`#` 起注释（行内的 `#` 之后同样整段忽略）。
- 允许空行与行尾空白；行结束符 LF 与 CRLF 都认。
- 样例见 `assets/config/config.sample.cfg` 。那份文件只是给人看的样本，
  **程序既不创建它也不读取它**。

### 优先级

**启动参数 > 配置文件 > 代码默认值。**
例如 `--lang` 覆盖配置里的 `lang`，`--windowed` 覆盖配置里的 `fullscreen`。
`--config` 只决定读哪个文件，不改取值。

启动时的两行日志分工不同：`Config: path=... fullscreen=... target_fps=...
theme=... lang=...` 打的是**配置文件里的值**（缺键即缺省），`startup options:
... lang=...` 打的是**启动参数覆盖后的最终生效值**。

### 失败路径

| 情况 | 处理 |
|---|---|
| 文件不存在 | 一行日志，全部用默认值（首次运行的正常状态） |
| 文件存在但打不开 | 告警，全部用默认值 |
| 未知键 | 告警，忽略该行 |
| 值非法（bool 不是 `true`/`false` 、 int 非数字或越界） | 告警，**该键**退回默认值 |
| 同一键重复 | 告警，保留先出现的 |

显式指定的路径（`--config=`）打不开时**报错并非零退出**，不静默退回默认值：
路径打错字是典型场景，静默兜底会把错误藏起来。

### 权限探测

`ConfigCanWrite()` 用 `access(2)` 探测：文件是否存在、是否可读、是否可写，
以及**所在目录**是否可写。启动时那行日志把这些状态与路径、四个键的生效值
一起打出来，四种情形分开写：

- **文件不存在**：首次运行的正常状态，不是问题。
- **存在但读不了**：告警，用默认值。
- **存在但不可写**：告警，将来写设置会失败。
- **目录不存在**：首次写入时需要先把它建出来。

`access(2)` 只是**尽力而为**的探测：判 `W_OK` 不保证真正写入时成功（磁盘满、
只读挂载、文件被别的进程换掉等），因此写入方仍须处理写入失败。

## 命令行参数（开发工具）

给开发与自动化用的启动参数。持久化配置走另一条通路（见「配置文件」），两者
不揉在一起：`--config` 只决定读哪个配置文件，取值的优先级仍是
启动参数 > 配置文件 > 代码默认值。

- 一律写作 `--key=value` 。**不支持短选项、不支持 `/` 、不支持位置参数**；未知参数、缺 `=` 、同一参数重复给出一律报错退出并打印用法。
- 启动时打一行日志记录本次生效的参数，便于取证。

| 参数 | 语义 |
|------|------|
| `--scene=<name>` | 指定启动场景，取值为 `title` / `settings` / `test` （与场景文件名一致）。未知取值报错退出，不静默回落到标题场景。 |
| `--export-frame=<path>` | 渲染 `--frames` 帧后，把**当前帧的设计画布**（640×480）导出成图片后退出。导出的是画布纹理而不是窗口后备缓冲，因此**与窗口尺寸和 DPI 无关**，也不是操作系统层面的屏幕截图。格式由扩展名决定（`png` / `bmp` / `tga` / `jpg` / `qoi` 等，常用 `.png` ），扩展名不受支持或目录不存在时导出失败，必须报错并以非零码退出。导出期间窗口隐藏，不占用桌面。 |
| `--frames=<n>` | 与 `--export-frame` 同用的帧数，缺省 2 ，必须 ≥ 1 ；单独给出时视为用法错误。 |
| `--windowed` | 本次不进全屏。与 `--export-frame` 同时给出时该参数不生效，会打一条日志说明（不静默）。 |
| `--config=<path>` | 指定配置文件的路径，不给就用平台默认位置。空值是用法错误；路径不存在或打不开时报错并以非零码退出，不静默退回默认值。 |
| `--lang=<id>` | 指定界面文案语言，取值为 `zh-hans` / `en` （缺省 `zh-hans`）。未知取值报错退出。这是开发工具：玩家侧的语言选择属于设置界面（与主题选择同批做），届时与持久化配置共享通路，不由启动参数负责。 |

退出路径与正常退出共用同一套卸载顺序（字体、文案、画布都在 `CloseWindow` 之前释放）。

## 素材规范
物品图标为 `48*48` 。

物品图标路径为 `assets/sprites/item_icons/<name>.png` ，相对仓库根目录。`<name>` 一律全小写蛇形（snake_case），扩展名 `.png` 为小写。`assets/databases/items.csv` 的 `texture` 字段必须与该路径逐字一致，比较时大小写敏感：macOS 等大小写不敏感的文件系统上直接用 `os.path.exists` 判断会假阳性，须取真实文件名精确比对，核查脚本 `scripts/check_assets.py` 即按此实现。

界面文本**每个语言一个目录，目录内按场景一个 CSV**：`assets/strings/<lang>/<scene_name>.csv` 。同一语言内文件名与 key 必须完全一致，只有内容不同，所以选语言只换目录前缀，不必逐个文件挑。

### 历史分辨率/图片尺寸参考

方屏：
- `320*200` CGA
- `320*240` QVGA
- `512*384` 单色 Macintosh
- `640*480` VGA, MCGA
- `800*600` SVGA
- `1024*768` XGA
- `1152*864` XGA+
- `1280*960` SXGA-

宽屏：
- `256*144`
- `480*272` WQVGA
- `640*360` nHD
- `854*480` FWVGA, 480p
- `960*540` qHD
- `1024*576` WSVGA
- `1280*720` 720p, HD

## 许可
本仓库采用分层授权，不存在覆盖全仓库的单一许可。根目录 `LICENSE` 声明的
BSD 2-Clause 适用于项目自身的源代码与美术资源；内置字体作为第三方组件，
沿用其上游许可。逐项范围如下。

| 范围 | 许可 | 全文位置 |
|------|------|----------|
| 本项目源代码（`src/` 、 `CMakeLists.txt` 、 `scripts/` 等） | BSD 2-Clause | 根目录 `LICENSE` |
| 内置字体 Fusion Pixel Font | SIL OFL 1.1 | `assets/fonts/ttf/licenses/` |
| `assets/sprites/` 美术资源 | BSD 2-Clause | 根目录 `LICENSE` |

字体授权细节

- 内置字体为 Fusion Pixel Font 12px Proportional zh-Hans，OFL-1.1。它由
  Ark Pixel、cubic-11、galmuri 三个 OFL-1.1 字体合并而成，三者的许可全文
  均保留在 `assets/fonts/ttf/licenses/` 对应子目录。四份文件都没有声明
  保留字体名（RFN），即不限制修改与再分发时的命名。
- OFL 允许把字体随软件打包、再分发乃至随商业产品销售，条件是不得单独出售
  字体本身，且必须随分发附带许可全文与版权声明。满足该要求的文件已在仓库内。
- 换字体前内置的 Zpix（最像素）是专有商业字体（上游无 LICENSE，商业授权
  USD $1000），已删除。历史原因见 `assets/fonts/ttf/doc/Fonts.md` 。

美术资源授权

`assets/sprites/` 下的素材（物品图标与场景图）全部由团队出资委托绘制，
并已买断版权，著作权归项目所有，不受任何第三方授权限制，也不存在需要
追溯的素材包来源。原有 20 个 64*64 图标因不符合规范已删除，现已按 48*48
规范重新交付部分图标，`assets/sprites/` 不是空目录。授权结论不变：买断解决
的是权利归属，对外授权由本仓库统一声明，与项目源代码使用同一份根目录
`LICENSE` ，同为 BSD 2-Clause。

## Git LFS
本仓库已经启用了 Git LFS ，全新拉取仓库以后可以执行 `git lfs pull` 拉取字体、图片等二进制文件。

## 大事记
Jul-24-2026 转为开源。

Sep-29-2026 授权整理：移除专有商业字体 Zpix 与 `src/raygui/styles/` ，
改用 Fusion Pixel Font（OFL-1.1）并添加根目录 BSD 2-Clause `LICENSE` ，
代码与第三方资源改为分层授权；`assets/sprites/` 素材的来源与授权同日确认，
全部由团队出资委托绘制并买断版权，来源追溯问题关闭。详见「许可」一节。

Sep-30-2026 移除第三方 GUI 库 `src/raygui/` ，界面层改为自建（`src/ui.c`
与 `src/include/ui.h` ）；删除 `main_scene` 超大背景图（4500*3000 ，违反
素材规范）。

Oct-01-2026 原有 20 个 64*64 物品图标因不符合 `48*48` 规范清空，待重做。

Oct-01-2026 修复 Retina 屏文字模糊：开启 `FLAG_WINDOW_HIGHDPI` ，渲染改为
固定 `640*480` 设计画布加整数倍放大（`src/graphsettings.c` ）。此前帧缓冲
小于物理像素，被系统平滑放大，文字在应用之外就已经糊了。

Oct-01-2026 界面加入阴影与边框：`UiStyle` 增加 `desktop` 、 `lineDim` 、
`shadow` 三个配色键与 `border_style` 、 `shadow_style` 两个枚举键，默认配色
换成 VGA 16 色的 Turbo C 方案（深蓝桌面、蓝面板、青框、黑底黄字标题栏）。
边框支持 none / single / triple 三色（浅-深-浅），阴影支持 auto / solid /
dither，偏移 8 设计像素。`border_style` 属装饰故允许主题设置，而
`border_width` 仍由程序独占。配色从 8 键扩到 11 键，键说明见
`assets/themes/default.cfg` 。原语侧三处硬编码的 `DrawRectangleLines`
收口到 `UiDrawFrame()` ，顺序固定为阴影、填充、边框、内容。

Oct-01-2026 修复像素字仍带灰边：`LoadFontEx` 按 hhea 行高（本字体
`asc-desc = 1600`）而非 `upem = 1200` 缩放，此前直接传对外字号 12/24/36 ，
实际每设计像素只有 0.75/1.5/2.25 个像素，栅格化落在半像素上。`src/font.c`
改为对外字号 12/24/36 对应传入 16/32/48，绘制与度量改用 `Font.baseSize` ，
图集恢复为纯二值。

Oct-10-2026 数据表目录统一为 `assets/databases/`（原 `assets/database/`），
物品图标文件名统一为小写蛇形并与 `items.csv` 的 texture 字段逐字一致，
新增 `scripts/check_assets.py` 做大小写敏感核查；README 的素材、待定与
许可三节按现状更新。

Oct-10-2026 界面文案外置：新增 `src/strings.c` 与 `assets/strings/main_scene.csv` ，
主场景不再有中文字面量，`font.c` 的 `uiChars[]` 硬编码删除；CSV 解析收口到
`utils.c` 的 `CsvReadFile` ，收集与检查共用 `IsCollectable` ，越界码点与超长
字段改为一次性告警，缺字检查复用收集阶段的文本快照。`GetUIFont` 加载失败不再
当作已加载，可用性判据改为 `FontIsUsable()` ，并修掉字体文件缺失时首帧在
`ui.c` 墨迹测量处越界崩溃的问题（实测 SIGSEGV）。

Oct-12-2026 新增回归测试：`tests/theme_tests.c` 14 条、`tests/ui_tests.c` 5 条，
覆盖主题解析器的 BOM / CRLF / 坏十六进制 / 未知键 / 禁用键 / 枚举键 / 缺
`default.cfg` 回退等路径，以及界面层行距公式、边框不吃文字、按钮与标题的
墨迹居中；接入 `ctest` 与 `scripts/run_tests.sh` ，CMakeLists 的编译选项
与依赖收口到 `configure_project_target` ，测试与本体共用同一套。原「界面
与字体」「主题」两节里靠人工验证的待办随之关闭。

Oct-14-2026 场景骨架与开发用命令行参数：新增 `title_scene`（主界面，启动场景）、
`settings_scene`（设置界面占位）与 `test_scene`（原 `main_scene` 改名，保留 UI 原语
与消息框演示，游玩界面出现前作为跳转目标）；`scene_manager` 增加请求式切换
（`RequestSceneSwitch` / `ProcessPendingSwitch`，场景不得在回调里切换自己），
新增 `app.c` 的退出请求与 `cli.c` 的参数解析，支持 `--scene` / `--export-frame` /
`--frames` / `--windowed` ；初始场景切换挪到 `InitCanvas()` 之后。界面网格约定改为
「默认节距 4 ，12 只用于字符格对齐」——640 不能被 12 整除，紧居中与 12 对齐不可兼得。
同日补界面文案的本地化结构：文案改放 `assets/strings/<lang>/<scene_name>.csv` ，新增
`src/lang.c` 收口文案目录路径（原先 `strings.c` 与 `font.c` 各写一份 `#define STRINGS_DIR`，
字体码点收集与文案载入因此可能岔开），并新增 `--lang` 开发参数；`en/` 下的英文是占位稿。

Oct-15-2026 构建前置检查与平台策略收敛：`cmake_minimum_required` 从 4.4.2 降到
3.16（发行版自带的 cmake 就够用），新增前置检查库 `scripts/preflight.sh` ，由
`build.sh` 与 `run_tests.sh` 用 `source` 引入，按 cmake → ninja → C 编译器 的顺序
逐项检查，缺哪项就打印缺什么与按发行版分组的安装办法，再以非零码退出；脚本绝不
自动安装任何依赖，也不代跑 `sudo` 。raylib 仍由 `CMakeLists.txt` 自己的
`FATAL_ERROR` 提示负责，不在前置检查里重复。平台支持收敛为只确保 UNIX-LIKE
（FreeBSD / Linux / macOS），Windows 因无机器可测且无签名证书会被 SmartScreen
拦截而移除支持，OpenBSD 保持「等待分析」；`scripts/build.bat.bak` 与
`scripts/build.ps1.bak` 已删除。`.gitattributes` 增加 `*.sh` 与 `*.py` 的
`text eol=lf`。同批新增持久化配置：新增 `src/config.c` 与
`src/include/config.h` （键表、平台默认路径、`access(2)` 权限探测），命令行
加 `--config=<path>` ，`main.c` 的全屏与目标帧数改从配置读，优先级是
启动参数 > 配置文件 > 代码默认值；本阶段程序只读配置不回写，样例见
`assets/config/config.sample.cfg` ；新增 `tests/config_tests.c` 12 条。

## 致谢
本项目在开发过程中使用了 AI 编码工具 OpenCode 辅助代码编写与文档整理。
