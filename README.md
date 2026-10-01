# The Law Of Prey

## TODO

当前阶段的目标：做出一个最小游玩闭环，即能走动、能捡东西、能看见饥饿值。程序生成地图仍不在本阶段考虑范围内，只需要保证游玩界面能读取并解析既定格式的地图文件。

场景结构约定：`Init()`、`Update()`、`Draw()`、`Unload()`。

需要拍板的条目统一放在「待定」。

### 地图与数值

- 定义地图文件格式：给出一版可读的网格格式说明（例如用字符区分草地、树木、石块、水源），作为解析与出图的共同依据。
- 起草游玩界面布局：背包格子、状态栏（饥饿 / 体力）、交互按钮的位置与操作方式。
- 梳理 `assets/database/items.csv` 字段：确认现有列是否够用（如堆叠上限、重量、食用恢复量），补齐需要的字段，交给代码解析。
- 编写第一版数值：饥饿消耗速度、食物恢复量、基础移动速度。

### 界面与字体

界面层为自建（`src/ui.c` 与 `src/include/ui.h`），不依赖任何第三方 GUI 库；度量约定见「字体与界面方案约束」。以下为待处理项。

- 字体加载失败要可恢复：`GetUIFont` 在 `texture.id == 0` 时仍会置 `fontsLoaded[idx] = true` ，坏 Font 会被整个会话一直返回。改为记录失败状态并回退。
- `GetUIFont` 在 `InitGameFont` 之前被调用时要告警并拒绝加载：此时 `collectedCps` 为 NULL ，`LoadFontEx` 只会拿到 ASCII ，中文静默变方框且无任何提示。
- 统一增补平面（U+10000 以上）的处理：`MarkCodepoint` 静默丢弃，收集范围是 BMP 而检查范围是全码点，这类字无论怎么在数据文件里补都加载不进来。改为收集与检查两端一致，并在收集阶段一次性告警。Fusion Pixel 有 1429 个码点在 BMP 之上（U+16FF2 - U+30F91），目前数据文件没有用到，所以 `BMP_RANGE` 仍够用；但「字体不含 BMP 外字形」已不再是可依赖的前提。
- CSV 字段超过 `CSV_FIELD_MAX` （1024）时要告警。当前被截掉的 codepoint 既不进图集也不报缺字（收集与检查走同一条截断路径），漏字完全无声。
- 移除 `font.c` 中的 `uiChars[]` 硬编码，并把 `assets/strings/` 接入 codepoint 收集。两条是耦合的：先建 `assets/strings/main_scene.csv` （key,text 格式）迁入界面用字，再删掉 `uiChars[]` 及其循环，`CollectAllCodepoints` 增扫该目录。
- 界面文本改为从 CSV 按 key 加载：`main_scene.c` 目前把「显示消息」「消息框」等展示文本写死在 C 字面量里，违反「字符串放数据文件」的约束。
- 收集阶段缓存文本引用，缺字检查复用，去掉对 `assets/database/*.csv` 的重复磁盘读取；同时让「字段被截断」变成可观测。
- 补 `font.c` 健壮性：`MemAlloc` 返回值判空；引号字段内遇到 `\r\n` 的处理与普通行保持一致。
- 为 `src/ui.c` 的文本居中与行距公式补断言或测试：文字墨迹框必须落在控件内区。当前只靠人工看截图，改动度量后没有回归保护。
- 验收：启动时无缺字告警；除 `src/font.c` 的封装内部外，代码中不直接调用 raylib 的 `DrawText` 、 `DrawTextEx` 、 `MeasureText` 、 `MeasureTextEx` ，一律走 `DrawUIText` 与 `MeasureUIText` ；显存中每个字体尺寸只有一张图集；Release 与 Debug 两个配置均构建并启动正常。

### 主题

配色系统已实现并接入 `InitGameFont()` 之后的启动流程，公开接口为 `InitTheme()` 、 `GetThemeCount()` 、 `GetThemeName()` 、 `LoadTheme()` 、 `GetCurrentThemeName()` ；当前 8 个配色键，格式说明见 `assets/themes/default.cfg` 。以下为待处理项。

- 主题选择：`InitTheme()` 启动时载入的始终是 `default.cfg` ，其余 `.cfg` 只被扫描枚举（`GetThemeCount` / `GetThemeName` ），也可由 `LoadTheme` 按名加载。**留到设置界面再做**：确定玩家侧的主题切换方式，并在做设置界面的同一批改动里落地。在此之前不要另加命令行参数、环境变量或 `active.cfg` 标记文件，以免多出一条会与界面互相打架的选择通路。
- 主题解析器的回归测试：BOM 、 CRLF 、 `""` 转义、缺 `default.cfg` 时的确定性回退、坏十六进制、未知键、禁用键告警，这些路径目前只靠人工验证。

### 游玩闭环

- 游玩场景骨架：新增 `src/scenes/game_scene.c` ，引入 Camera2D 、按地图文件渲染地砖、摄像机跟随玩家。
- 玩家移动与碰撞：补全 `src/include/player.h` 的坐标与速度，WASD 移动，被不可通行地砖阻挡。
- 物品数据层：解析 `assets/database/items.csv` ，按需加载物品图标并做缓存，处理路径前缀与扩展名大小写不一致的问题。
- 配置外置：去掉 `src/main.c` 中硬编码的全屏与目标帧数（分辨率已改由 `GRAPH_DESIGN_*` 提供）。
- 保持 `scripts/build.sh` 的 Release / Debug 双配置均可构建。

### 素材

- 物品图标重做：`assets/database/items.csv` 有 104 条记录、103 个不同的 texture 路径，目前没有任何图标。原有 20 个 64*64 图标因不符合规范已删除，需按 48*48 重做。
- 统一图标规范：底色、命名与扩展名大小写，并与 CSV 的 texture 字段一一对应。
- 出一版地砖图块集：与地图格式逐项对应，含草地、树木、石块等。

### 待定

- `items.csv` 的 texture 字段为 `res/textures/...` ，而图标曾放在 `assets/sprites/item_icons/resource/...` ；目录前缀与扩展名大小写以哪一边为准需要拍板。
- 上面三块骨架（游玩场景、玩家移动、物品数据层）的落地顺序，等地图格式定稿后确定。


## 字体与界面方案约束

以下为已定结论，实现时不要反向引入。

- 字体全域只有一份，`LoadFontEx` 只能在 `InitGameFont` 里调用，场景中禁止重复加载，场景切换也不重载。
- 图集尺寸只能是 12 的倍数（12 、 24 、 36），像素格才对得齐。`upem = 1200` ，缩放 24px 图集会破坏像素格对齐；背包格子需要 12px 数字时另加载一张 12px 图集，而不是缩放。
- 文字一律走 `DrawUIText` 与 `MeasureUIText` ，两者内部封装 `DrawTextEx` 与 `MeasureTextEx` 。raylib 6.0 没有 `SetFontDefault` ，`DrawText()` 永远使用内置默认字体，画中文必定是方框，没有替代做法。
- 图集贴图保持 `TEXTURE_FILTER_POINT` ，不要改成线性过滤。
- 字符串放数据文件，不写死在 C 字面量里，否则精确收集会漏字，日后做多语言也无从下手。
- 缺字自愈不能省：`GetGlyphIndex` 找不到字形时会静默回退到 `'?'` 。保留启动时的缺字扫描与 `TraceLog(LOG_WARNING)` 输出，缺字在开发期补进字符串文件即可收敛。
- `src/font.c` 对外只暴露 `InitGameFont()` 、 `UnloadGameFont()` 、 `GetUIFont(size)` 、 `DrawUIText()` 、 `MeasureUIText()` 、 `FontHasGlyph(cp)` ，接口名一律大驼峰。
- 不引入任何第三方 GUI 库，`src/raygui/` 已整体删除。历史原因：raygui 的控件高度按默认 `TEXT_SIZE = 10` 硬编码，`InitGameFont` 把字号提到 24 之后这些常量没有跟着放大；叠加像素字在 24px 下 CJK 墨迹底部贴死行盒底边，控件内高不足时 `TEXT_ALIGNMENT_VERTICAL` 的居中计算会把文字推出边框。
- 界面尺寸只取 12 的倍数。非网格尺寸一律用 `UI_ALIGN12()` 向上取整，内部留白统一取 `UI_PADDING` ，按钮高度取 `UI_BUTTON_HEIGHT()` ，度量不写死数值。
- 多行文本由 `src/ui.c` 逐行绘制，行距为 `UI_LINE_STEP(size)` （等于字号）。raylib 的 `MeasureTextEx` 行间步进是「字号 + 2」（实测 12/24/36 对应 14/26/38），既不是字号也不对齐像素格，且 `spacing` 参数只管行内字符的水平间隔，调不动它。换行只认显式 `'\n'` ，不做自动折行，也不做宽度上限截断。
- 按钮的焦点是唯一的变色来源：持有焦点时前景与背景互换，鼠标悬停与按下不产生任何视觉变化。焦点归属由调用方管理，原语本身不保存跨帧状态。
- 主题只管配色，不管字体度量。`assets/themes/*.cfg` 中的 `text_size` 、 `text_spacing` 、 `text_line_spacing` 、 `text_alignment` 、 `text_alignment_vertical` 、 `text_padding` 、 `border_width` 由程序独占，写了会被忽略并告警；配色统一从 `src/ui.c` 的 `gUiStyle` 取。
- 以下为有意设计，不要改回朴素实现：codepoint 用位图去重 + 升序数组 + 二分查询；按尺寸懒加载 + `initialized` 幂等 + `UnloadGameFont` 可重入；`GetCodepointNext` 返回 0 时的防死循环保护。
- 明确不做：不使用 SDF 字体（需要自定义 shader，且会让像素字失去锐利度，本方案只做整数倍缩放，用不到它的平滑缩放优势）；不预载常用 3500 字（精确收集已足够，除非真的出现无法预先收集的动态文字如玩家输入或随机文本，届时再加入，成本约 13MB）；暂不做分页字体（按 codepoint 区间切分的 `Font` 数组，触发条件明确，等真的出现动态文字再补，不影响现有架构）。

## 渲染分辨率与整数缩放

以下为已定结论，实现时不要反向引入。

- 设计分辨率固定为 `640*480`（`GRAPH_DESIGN_WIDTH` / `GRAPH_DESIGN_HEIGHT` ）。游戏逻辑与界面一律画在这张画布内，`DrawCanvas()` 再整数倍居中放大到窗口，未覆盖部分为黑边。整数倍是硬要求，非整数倍会让像素字在液晶上被插值模糊。
- 必须设置 `FLAG_WINDOW_HIGHDPI` ，且必须在 `InitWindow` 之前。Retina 屏的逻辑分辨率是物理像素的一半，不开这个标志时帧缓冲只有窗口点尺寸，系统会把它平滑放大到物理像素，文字在应用之外就已经糊了，采样过滤救不回来。实测 4K 屏（逻辑 1920*1080）开启后帧缓冲为 3840*2160 ，与面板 1:1。
- 画布贴图保持 `TEXTURE_FILTER_POINT` 。实测同一份字体图集在 2 倍放大下，点采样与线性插值的中间灰度占比为 52% 对 80% ，线性会直接糊掉像素字。
- 窗口默认无边框窗口全屏（`ToggleBorderlessWindowed` ）。16:9 屏上 4:3 内容两侧留黑边，这是已接受的取舍。
- 界面层鼠标命中测试必须走 `GetCanvasMouse()` ，不能用 `GetMousePosition()` ：后者是窗口坐标，画布放大居中后偏置与倍数都不对。同理 `UiMessageBox` 居中用设计分辨率，不能用 `GetScreenWidth()` ，后者返回窗口尺寸而非画布尺寸。

## 平台支持
确保支持 FreeBSD 、 Linux ，尽量支持 OpenBSD 、 macOS 、 Windows 。  
因为弄不到证书，所以当前 Windows 上编译运行可能会遇到 SmartScreen 拦截；OpenBSD 在构建项目的时候遇到了一个头文件缺失问题，但是据说可以传入路径解决，而且此路径下的确发现了缺失的头文件。  
具体情况正等待测试。

2026-09-18 19:18 因为没有 Windows 电脑可供测试，无法确定 `scripts/build.ps1` 与 `scripts/build.bat` 是否有效，两个文件已改名为 `scripts/build.ps1.bak` 与 `scripts/build.bat.bak` 废弃，请不要盲目使用。

## 项目简介
正在构建程序基础，游戏内容待定。

因游戏内容待定，所以项目名称为临时设置，未来可能会修改。

## 设计规范
普通物品最大堆叠 60 ，特殊物品最大堆叠 10 。

## 代码及文档规范
- 交流与注释中的路径表示约定：默认从项目根目录起始，例如根目录下的 `.clang-format` 表示为 `.clang-format` ，`main.c` 在 项目根目录 `/src/main.c` ，所以表示为 `src/main.c` 。
- 代码中不要使用 emoji ，注释不要包含 12345 类似的编号（ AI 经常这样做）；允许 ASCII 和中文汉字。
- C 代码使用 Google Style ，限宽 80 字符，配置已经写在了 `.clang-format` 里面。
- 文件名使用蛇形规范，例如 `main_scene.c`, `scene_manager.c` 。
- 函数、结构使用大驼峰命名，例如 `MainScene->Init()`, `SceneManagerGetCurrent()`, `MainScene` 。

## 素材规范
物品图标为 48*48 。

对于全屏图片，解析度不得高于 `1280*960` ，以免影响性能和体积。

方屏可选择的解析度：
- `320*200` CGA
- `320*240` QVGA
- `512*384` 单色 Macintosh
- `640*480` VGA, MCGA
- `800*600` SVGA
- `1024*768` XGA
- `1152*864` XGA+
- `1280*960` SXGA-

宽屏可选择的解析度：
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
追溯的素材包来源。这些文件因不符合当前素材规范已被删除，目录当前为空，
但授权结论不变：买断解决的是权利归属，对外授权由本仓库统一声明，与项目
源代码使用同一份根目录 `LICENSE` ，同为 BSD 2-Clause。

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

Oct-01-2026 原有 20 个 64*64 物品图标因不符合 48*48 规范清空，待重做。

Oct-01-2026 修复 Retina 屏文字模糊：开启 `FLAG_WINDOW_HIGHDPI` ，渲染改为
固定 `640*480` 设计画布加整数倍放大（`src/graphsettings.c` ）。此前帧缓冲
小于物理像素，被系统平滑放大，文字在应用之外就已经糊了。

## 致谢
本项目在开发过程中使用了 AI 编码工具 [oh-my-openagent](https://github.com/code-yeongyu/oh-my-openagent) 辅助代码编写与文档整理。
