# The Law Of Prey

## TODO

当前阶段的目标：做出一个最小游玩闭环，即能走动、能捡东西、能看见饥饿值。程序生成地图仍不在本阶段考虑范围内，只需要保证游玩界面能读取并解析既定格式的地图文件。

场景结构约定：`Init()`, `Update()`, `Draw()`, `Unload()` 。

已完成：场景管理器重制、Raygui 导入、构建脚本修复。

任务按分工列出；需要多人共同拍板的条目统一放在「待定」。

### 策划（兼场景设计）

- 定义地图文件格式：给出一版可读的网格格式说明（例如用字符区分草地、树木、石块、水源），作为程序解析与美术出图的共同依据。
- 起草游玩界面布局：背包格子、状态栏（饥饿 / 体力）、交互按钮的位置与操作方式。
- 梳理 items.csv 字段：确认现有列是否够用（如堆叠上限、重量、食用恢复量），补齐需要的字段，交给程序解析。
- 编写第一版数值：饥饿消耗速度、食物恢复量、基础移动速度。

### 程序

#### 界面与字体

- **【用户可见】修复 raygui 文字贴边。** 根因不是 `TEXT_PADDING` 为 0，而是 raygui 的控件高度按默认 `TEXT_SIZE = 10` 硬编码，`InitGameFont` 把 `TEXT_SIZE` 提到 24 之后这些常量没有跟着放大；同时 zpix 在 24px 的 CJK 墨迹高 22px、底部贴死行盒底边，控件内高不足时 `TEXT_ALIGNMENT_VERTICAL` 的居中计算会把文字推到边框之外。
  - 新建 `src/include/gui_config.h` ，在 `#define RAYGUI_IMPLEMENTATION` 之前覆盖 `RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT` 、 `RAYGUI_WINDOWBOX_CLOSEBUTTON_HEIGHT` 、 `RAYGUI_MESSAGEBOX_BUTTON_HEIGHT` 、 `RAYGUI_MESSAGEBOX_BUTTON_PADDING` 、 `RAYGUI_TEXTINPUTBOX_BUTTON_HEIGHT` 、 `RAYGUI_TEXTINPUTBOX_HEIGHT` 。
  - 新增 `ApplyUiMetrics()` 统一样式度量：`BORDER_WIDTH` 收 1（按 2 配 24px 像素字过粗，且白占 4px 垂直空间）、 `TEXT_LINE_SPACING` 设 0（多行才落在像素格上）、 `TEXT_PADDING` 设 4（水平留白，zpix 的 CJK 左边距为 0）。**不要靠加大 `TEXT_PADDING` 解决**：它在 `GetTextBounds` 里同时缩 x 和 y，控件内高不变时会把文字整体上推，反而加重上下贴边。
  - 消息框尺寸从 `400*100` 放大，24px 字体下这个高度装不下「标题栏 + 正文 + 按钮 + 间距」。
  - `TEXT_PADDING` 定 4 还是 6 需要看实际截图定，zpix 的像素格单位是 12。
- 把「不贴边」变成可断言的约束：把 `GetTextBounds` 与垂直居中的定位公式提炼成项目内的纯函数，断言文字墨迹框落在控件内区，纳入 `scripts/build.sh` 的检查。
- 字体加载失败要可恢复：`GetUIFont` 在 `texture.id == 0` 时不要置 `fontsLoaded[idx] = true` ，改为记录失败状态并回退，避免整个会话一直返回一个坏 Font 。
- `GetUIFont` 在 `InitGameFont` 之前被调用时要告警并拒绝加载：此时 `collectedCps` 为 NULL ，`LoadFontEx` 只会拿到 ASCII ，中文静默变方框且无任何提示。
- 统一增补平面（U+10000 以上）的处理：`MarkCodepoint` 静默丢弃，收集范围是 BMP 而检查范围是全码点，导致这类字无论怎么在数据文件里补都加载不进来。改为收集与检查两端一致，并在收集阶段一次性告警。zpix 的 22240 个字形全在 BMP 内，不要为此引入全码点索引。
- CSV 字段超过 `CSV_FIELD_MAX` (1024) 时要告警。当前被截掉的 codepoint 既不进图集也不报缺字（收集与检查走同一条截断路径），漏字完全无声。
- 移除 `font.c` 中的 `uiChars[]` 硬编码，并把 `assets/strings/` 接入 codepoint 收集。两条是耦合的：先建 `assets/strings/main_scene.csv` （key,text 格式）迁入界面用字，再删掉 `uiChars[]` 及其循环，`CollectAllCodepoints` 增扫该目录。
- `main_scene.c` 在 `Init` 时从 CSV 按 key 加载字符串，不把展示文本写死在 C 字面量里。
- ~~`Font_HasGlyph` 改名 `FontHasGlyph` ：其余接口都是大驼峰，该名字是唯一例外。~~ 已完成。注意它答的是「是否被收集」而不是「是否能渲染」，调用方别当后者用。
- 收集阶段缓存文本引用，缺字检查复用，去掉对 `assets/database/*.csv` 的重复磁盘读取；同时让「字段被截断」变成可观测。
- 补 `font.c` 健壮性：`MemAlloc` 返回值判空；引号字段内遇到 `\r\n` 的处理与普通行保持一致。
- 为 CSV 解析补测试，覆盖 BOM 、 `""` 转义、 CRLF 、引号内换行、文件末尾无换行、超长字段。这些路径目前只靠人工推演，改解析时没有回归保护。
- 验收：启动时无缺字告警；代码中不存在直接调用 `DrawText` 与 `MeasureText` 的地方，一律走 `DrawUIText` 与 `MeasureUIText` ；显存中每个字体尺寸只有一张图集；Release 与 Debug 两个配置均构建并启动正常。

#### 游玩闭环

- 游玩场景骨架：新增 `src/scenes/game_scene.c` ，引入 Camera2D 、按地图文件渲染地砖、摄像机跟随玩家。
- 玩家移动与碰撞：补全 `src/include/player.h` 的坐标与速度，WASD 移动，被不可通行地砖阻挡。
- 物品数据层：解析 `assets/database/items.csv` ，按需加载物品图标并做缓存，处理路径前缀与扩展名大小写不一致的问题。
- 配置外置：去掉 `src/main.c` 中硬编码的分辨率、全屏、目标帧数。
- 保持 `scripts/build.sh` 的 Release / Debug 双配置均可构建。

### 美术

- 补齐物品图标：CSV 已有 100 条记录，`assets/sprites/item_icons/` 下只有 20 个图标。
- 统一图标规范：尺寸、底色、命名与扩展名大小写，并与 CSV 的 texture 字段一一对应。
- 出一版地砖图块集：与策划定义的地图格式逐项对应，含草地、树木、石块等。
- 修正超大背景图：`assets/sprites/main_scene/*.png` 为 4500*3000 ，违反本项目的美术素材规范，需缩放到规范内。

### 待定

- items.csv 中的 texture 路径为 `res/textures/...` ，实际文件在 `assets/sprites/item_icons/resource/...` ，以哪一边为准需要三方确认。
- 上面三块骨架（游玩场景、玩家移动、物品数据层）的落地顺序，等策划的地图格式定稿后确定。
- 界面控件走哪条路：MessageBox 是工具型控件，固定 24px 标题栏与按钮在 24px 字体下必然偏紧。是覆盖宏凑合用，还是游戏 UI 改用 `GuiPanel` + `GuiLabel` 自拼，需要先定方向。


## 字体与界面方案约束

以下为已定结论，实现时不要反向引入。

- 字体全域只有一份，`LoadFontEx` 只能在 `InitGameFont` 里调用，场景中禁止重复加载，场景切换也不重载。
- 图集尺寸只能是 12 的倍数（12 、 24 、 36），像素格才对得齐。`upem = 1200` ，缩放 24px 图集会破坏像素格对齐；背包格子需要 12px 数字时另加载一张 12px 图集，而不是缩放。
- 文字一律走 `DrawTextEx` 与 `MeasureTextEx` 。raylib 6.0 没有 `SetFontDefault` ，`DrawText()` 永远使用内置默认字体，画中文必定是方框，没有替代做法。
- 图集贴图保持 `TEXTURE_FILTER_POINT` ，不要改成线性过滤。
- 字符串放数据文件，不写死在 C 字面量里，否则精确收集会漏字，日后做多语言也无从下手。
- 缺字自愈不能省：`GetGlyphIndex` 找不到字形时会静默回退到 `'?'` 。保留启动时的缺字扫描与 `TraceLog(LOG_WARNING)` 输出，缺字在开发期补进字符串文件即可收敛。
- `src/font.c` 对外只暴露 `InitGameFont()` 、 `UnloadGameFont()` 、 `GetUIFont(size)` 、 `DrawUIText()` 、 `MeasureUIText()` 、 `FontHasGlyph(cp)` ，把「raylib 没有 `SetFontDefault` 」这件事封装在模块内部。接口名一律大驼峰，`Font_HasGlyph` 是待改名的遗留写法，不要再新增调用。
- 禁止在 `ApplyUiMetrics()` 之后调用 `GuiLoadStyle()` 或再次 `GuiLoadStyleDefault()` ：那会把 `TEXT_SIZE` 等度量刷回样式文件里的 10 像素基线，且不报错。`src/raygui/styles/` 下的内置样式是诱因。
- 以下为有意设计，不要改回朴素实现：codepoint 用位图去重 + 升序数组 + 二分查询；按尺寸懒加载 + `initialized` 幂等 + `UnloadGameFont` 可重入；`GetCodepointNext` 返回 0 时的防死循环保护。
- 明确不做：不使用 SDF 字体（需要自定义 shader，且会让像素字失去锐利度，本方案只做整数倍缩放，用不到它的平滑缩放优势）；不预载常用 3500 字（精确收集已足够，除非真的出现无法预先收集的动态文字如玩家输入或随机文本，届时再加入，成本约 13MB）；暂不做分页字体（按 codepoint 区间切分的 `Font` 数组，触发条件明确，等真的出现动态文字再补，不影响现有架构）。

## 平台支持
确保支持 FreeBSD 、 Linux ，尽量支持 OpenBSD 、 macOS 、 Windows 。  
因为弄不到证书，所以当前 Windows 上编译运行可能会遇到 SmartScreen 拦截；OpenBSD 在构建项目的时候遇到了一个头文件缺失问题，但是据说可以传入路径解决，而且此路径下的确发现了缺失的头文件。  
具体情况正等待测试。

2026-09-18 19:18 因为没有 Windows 电脑可供测试，无法确定 `script.ps1` 和 `script.bat` 是否有效，目前已经将文件废弃，请不要盲目使用。

## 项目简介
正在构建程序基础，游戏内容待定。

因游戏内容待定，所以项目名称为临时设置，未来可能会修改。

## 策划规范（自己修改名字）
xxx 最大堆叠 60，特殊物品最大堆叠 10 。

## 代码及文档规范
- 交流与注释中的路径表示约定：默认从项目根目录起始，例如根目录下的 `.clang-format` 表示为 `.clang-format` ，`main.c` 在 项目根目录 `/src/main.c` ，所以表示为 `src/main.c` 。
- 代码中不要使用 emoji ，注释不要包含 12345 类似的编号（ AI 经常这样做）；允许 ASCII 和中文汉字。
- C 代码使用 Google Style ，限宽 80 字符，配置已经写在了 `.clang-format` 里面。
- 文件名使用蛇形规范，例如 `main_scene.c`, `scene_manager.c` 。
- 函数、结构使用大驼峰命名，例如 `MainScene->Init()`, `SceneManagerGetCurrent()`, `MainScene` 。

## 美术素材规范
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

## Git LFS
本仓库已经启用了 Git LFS ，全新拉取仓库以后可以执行 `git lfs pull` 拉取字体、图片等二进制文件。

## 大事记
Jul-24-2026 转为开源。

## 致谢
本项目在开发过程中使用了 AI 编码工具 [oh-my-openagent](https://github.com/code-yeongyu/oh-my-openagent) 辅助代码编写与文档整理。
