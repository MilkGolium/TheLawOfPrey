# 界面阴影与边框样式改动计划

## 一、已定决策

| 项 | 取值 | 说明 |
|---|---|---|
| 配色 | 方案 A（Turbo C） | 蓝底 + 青框 + 黑底黄字标题栏 |
| `shadow` 阴影色 | `#000000` | 已验收预览用的就是这个值 |
| 阴影形态 | `auto`（默认）/ 可强制 | 暗底自动实心、亮底自动抖动 |
| 阴影偏移 | 8 设计像素 | 已看并排预览后选定；硬上限是 `UI_PADDING`（12），8 是观感取舍（占按钮高 36 的 22%） |
| 阴影范围 | box + button + rect | 三类原语统一 |
| 边框样式 | 默认 `none`，可选 `none` / `single` / `triple` | 暗底下 `none` 与 `triple` 都可接受，取 `none` 为默认 |
| 三色边框顺序 | 浅-深-浅：`line` / `lineDim` / `line` | 只做这一种顺序，反序暂不支持 |
| A4 填充对比 | **不改** | 面板 `#0000AA` 对桌面 `#00008B`，实机肉眼差异显眼 |
| 边框 `line` 键 | **保留** | `none` 时不再画边框，但键要留给分隔线与将来使用 |
| VSYNC | 不动 | 留到与设置、持久化一起做 |

## 二、配色（写入 `default.cfg` 与 `kDefaultUiStyle`）

VGA 16 色。两者互为权威副本，改一侧必须同步另一侧。

| 键 | 值 | 用途 |
|---|---|---|
| `desktop` | `#00008B` | 桌面底色，阴影落在它上面 |
| `background` | `#0000AA` | 面板/内容区底色 |
| `line` | `#00AAAA` | 单线边框、分隔线、三色边框的外圈与内圈 |
| `lineDim` | `#000055` | 三色边框中间圈 |
| `text` | `#AAAAAA` | 正文 |
| `textDim` | `#5555FF` | 次要文字 |
| `button` | `#00AAAA` | 按钮底色 |
| `buttonText` | `#000000` | 按钮文字 |
| `titleBar` | `#000000` | 标题栏底色 |
| `titleText` | `#FFFF55` | 标题栏文字 |
| `shadow` | `#000000` | 阴影色 |

枚举键：

| 键 | 取值 | 默认 |
|---|---|---|
| `border_style` | `none` / `single` / `triple` | `none` |
| `shadow_style` | `auto` / `solid` / `dither` | `auto` |

`lineDim` 用显式键而不是从 `line` 派生：派生会得到 `#005555`（偏青），与已验收预览里的 `#000055`（偏蓝）不是同一个颜色。

## 三、数据模型改动（`src/include/ui.h`）

`UiStyle` 新增 5 个字段：

```
Color desktop;                    // 桌面底色
Color shadow;                     // 阴影色
Color lineDim;                    // 三色边框中间色
UiBorderStyle borderStyle;
UiShadowStyle shadowStyle;
```

两个枚举：

```
typedef enum UiBorderStyle {
  UI_BORDER_NONE = 0,
  UI_BORDER_SINGLE,
  UI_BORDER_TRIPLE,
} UiBorderStyle;

typedef enum UiShadowStyle {
  UI_SHADOW_AUTO = 0,
  UI_SHADOW_SOLID,
  UI_SHADOW_DITHER,
} UiShadowStyle;
```

新增宏：

```
#define UI_SHADOW_OFFSET 8
```

`UI_SHADOW_OFFSET` 是纯装饰，既不移动字形也不改变控件尺寸，因此**不需要满足任何整除约束**。实测偏移 1/2/3/4/5/6/7/8/10/11/12 全部产生 0 个混合像素、边缘硬边：整数缩放配 `TEXTURE_FILTER_POINT` 下每个设计像素都映射成干净的 N×N 块，与偏移的奇偶无关。

取值 8 的约束来自两条硬边界，都不以整除为条件：

- 上限是 `UI_PADDING`（12）。偏移达到 12 会让按钮阴影正好落在消息框的框边上，超过 12 则出框并压到相邻控件。8 是在这条上限内取的值。
- 抖动阴影在偏移 8 时 L 形带较宽，会读成质感带而非阴影；这是可接受的，因为深蓝桌面下 `auto` 解析为实心，只有浅色主题才会切抖动。

**`auto` 的解析时机与依据**：在 `ApplyThemeFile` 写回 `gUiStyle` 之前解析成具体值，之后渲染只做 `switch`，不做分支判断。依据是**桌面色 `desktop`** 而不是面板色 `background`——绝大多数阴影落在桌面上（独立按钮、消息框本身），只有消息框内的按钮阴影落在面板上。深桌面配浅面板时，按钮阴影虽按桌面判定为实心，但实心黑在浅面板上观感同样成立，故不引入第二套判定。

亮度用整数近似，避免引入浮点：

```
static int Luminance(Color c) {
  return (c.r * 30 + c.g * 59 + c.b * 11) / 100;
}
```

阈值 128（50% 亮度）：`#00008B` 得 15，判暗；`#F5F5F5` 得 245，判亮。

## 四、按文件的改动清单

| 文件 | 改动 | 量级 |
|---|---|---|
| `src/include/ui.h` | `UiStyle` 加 5 字段、2 个枚举、`UI_SHADOW_OFFSET`、`UiUnloadStyle()` 声明、更新 `UiStyle` 注释 | 约 30 行 |
| `src/ui.c` | `kDefaultUiStyle` 换 11 个色值 + 2 个枚举默认值；新增 `DrawShadow()`、`BorderRingCount()`、`DrawBorderRings()`、`UiDrawFrame()`、`UiUnloadStyle()`、抖动贴图的懒加载；`UiDrawRect`/`UiButton`/`UiDrawBox` 三处硬编码 `DrawRectangleLines` 改为调 `UiDrawFrame`，两处文字居中改为按 `BorderRingCount()` 内缩 | 约 90 行 |
| `src/theme.c` | `kColorKeys` 加 `desktop`/`shadow`/`lineDim` 三项；新增枚举键表与标识符表；`ProcessLine` 加枚举分支；`ResolveAutoStyles()` | 约 70 行 |
| `assets/themes/default.cfg` | 11 个色值 + 2 个枚举键；重写键说明段 | 约 20 行 |
| `src/main.c` | `ClearBackground(DARKBLUE)` 改为 `ClearBackground(gUiStyle.desktop)`；主循环结束时调 `UiUnloadStyle()` | 2 行 |
| `README.md` | 「字体与界面方案约束」补阴影与边框约定；「渲染分辨率与整数缩放」补阴影裁剪边界；大事记补一条 | 约 15 行 |

README 有一条现成约束需要改措辞：现在写的是「**主题只管配色，不管字体度量**」，并列举了 `text_size`、`border_width` 等程序独占键。新增的 `border_style` / `shadow_style` 是**外观**而非度量，原句照抄会自相矛盾。要改成「主题管配色与装饰形态，不管字体度量」，并明确 `borderwidth` 被禁的理由（能改坏像素格对齐与中文渲染）不适用于 `border_style`（不触碰度量，只在控件边缘内缩 3px）。

## 五、绘制实现

### 5.1 顺序

阴影 → 填充 → 边框 → 内容。阴影必须在填充之前画，露出的 L 形才是阴影。

### 5.2 边框：一个函数，三种圈数

```
DrawBorderRings(x, y, w, h, colors, count)
```

每圈用 4 个**填充矩形**（上、下、左、右）绘制，`count` 为 0 / 1 / 3。

**不要用 `DrawRectangleLinesEx`**：它走 `rlBegin(RL_LINES)` / `rlEnd` 立即模式，会打断 raylib 的批次。填充矩形全部走同一张 1×1 白贴图，会被批成 1 次 draw call。已实测：3 圈共 12 个矩形，四角闭合，三层颜色精确。

### 5.3 阴影：两种形态，各 1 次绘制

实心：`DrawRectangle(x + dx, y + dy, w, h, gUiStyle.shadow)`。

抖动：一张 **2×2 贴图**，`(0,0)` 与 `(1,1)` 为不透明白、`(1,0)` 与 `(0,1)` 全透明，配 `TEXTURE_WRAP_REPEAT`，用 1 次 `DrawTexturePro` 平铺到整个阴影区域。绘制时 `tint` 传 `gUiStyle.shadow`，颜色由 tint 决定，**贴图本身不需要随主题重建**。

已实测：200 像素宽的采样窗口内 199 次相邻像素转换全部横纵交替，且不漏进面板内部。

### 5.4 `UiDrawFrame()` 收口

三个原语当前各自硬编码 `DrawRectangleLines`。统一收口到 `UiDrawFrame(x, y, w, h, titleBarHeight, fillColor)`，内部按 `borderStyle` 决定画 0 / 1 / 3 圈。这是**净减少**重复代码，不是新增一层。

实现上与本节原稿有三处偏差，都以实现为准回写规格：

- **`UiDrawFrame` 保持 `static`，不进 `ui.h`。** 只有 `src/ui.c` 内部的三个原语调用它；导出即等于对外承诺布局参数是公共 API。
- **签名多两个参数。** 原稿的 `(x, y, w, h)` 不够用：`fillColor` 必须由调用方给出，因为按钮底色取 `button`、聚焦时还要与 `buttonText` 互换，而 `rect` 与 `box` 用 `background`；`titleBarHeight` 为 0 表示不画标题栏（`rect` / `button`）。硬塞默认值反而会引入调用点与实际取值不一致的分支。
- **阴影是单个 `DrawShadow()`，不是 `DrawShadowSolid()` + `DrawShadowDither()` 两个函数。** 二者体量各只有几行、且共享同一段偏移与矩形计算，拆成两个函数只是把同一份坐标算两遍。「渲染层只做 `switch`」的要求仍然成立。

### 5.5 `BorderRingCount()` 同时用作文字内缩量

边框圈数就是标题栏与按钮文字要避让的垂直内缩量：`ring0` 就画在 `y` 行起，文字若按整块高度居中，墨迹上缘必然落进边框里。`BorderRingCount()` 由 `DrawBorderRings()` 与 `UiDrawBox()` / `UiButton()` 的居中公式共用，避免「画几圈」和「让几圈」两处各自硬编码而漂移。详见坑点第 13 条。

## 六、坑点

1. **`SetTextureWrap()` 会把过滤重置成 `LINEAR`**。必须先 `SetTextureWrap` 再 `SetTextureFilter(POINT)`，否则抖动被重采样糊掉。顺序要写进注释。
2. **`applied` 计数必须把枚举键算进去**。`ApplyThemeFile` 里 `applied == 0` 会导致整个文件被放弃；如果 `ProcessLine` 的枚举分支不返回 `true`，只写 `border_style = none` 一行的主题会被判为「无有效配色」而整体不生效。这条极易漏。
3. **枚举值要按坏十六进制同样的方式容错**：标识符不认识就 `LOG_WARNING` 并忽略该行，不影响其他行。
4. **枚举键名经 `NormalizeKey` 归一**（剥 `-`/`_`、转小写），表里必须存扁平小写：`borderstyle`、`shadowstyle`、`linedim`。
5. **`kForbiddenKeys` 保持原样**，新增键不得与 `textsize`、`borderwidth` 等冲突。`border_style` 是装饰，不触碰度量，允许主题设置；这与 `borderwidth` 被禁的理由不同。
6. **`LoadImageFromTexture()` 返回的图上下翻转**。将来写断言回读时，采样坐标必须做 `y -> 480 - 1 - y`。
7. **不要用 `TakeScreenshot()` 做验证**。本机实测在 macOS 上抓到的是未定义帧缓冲（整片 `#000000`）。验证一律用 `LoadImageFromTexture()` 回读断言。
8. **抖动贴图的生命周期**：懒加载 + 幂等 + 可重入卸载，照抄 `font.c` 的 `fontsLoaded[]` 模式。与配色无关，主题切换时不需要重建。
9. **阴影会被画布裁掉，而 `UiMessageBox` 目前完全没有尺寸上限**。偏移 8px 要求 `x + boxWidth + 8 <= 640` 且 `y + boxHeight + 8 <= 480`。宽度代入居中公式 `x = (640 - boxWidth) / 2` 得 `(640 + boxWidth)/2 + 8 <= 640`，解得 `boxWidth <= 624`（恰好是 12 的整数倍，624/12=52）。高度同理代入 `y = (480 - boxHeight) / 2` 得 `(480 + boxHeight)/2 + 8 <= 480`，解得**数学上界 `boxHeight <= 464`**；464 不是 12 的整数倍，为对齐像素格再向下取到 **456**（456/12=38，468 已越界）。故 `kMaxBoxWidth = 624`、`kMaxBoxHeight = 456`。当前 `src/ui.c:212-216` 只做 `UI_ALIGN12` 向上取整，**没有任何 clamp**，正文一行超过 25 个汉字（24px 满宽 600px）就会超限。必须加 `if (boxWidth > kMaxBoxWidth) boxWidth = kMaxBoxWidth;` 与高度同理。

    clamp 只保证阴影不被画布裁掉，**不截断正文**。文字溢出按 README 既有约定「不做宽度上限截断」由调用方负责，`UiMessageBox` 拿到超长文本时不会替调用方分行。

    现有内容实测：`UiMessageBox("消息框", "你好！这是一条测试消息。", …)` 得到 `boxWidth 312`、`boxHeight 132`、位置 `(164, 174)`，右边缘 `164+312+8=484`、下边缘 `174+132+8=314`，离限制极远。所以 clamp 是安全网，不是当前就要修的问题。

    控件层面同样要查：`src/scenes/main_scene.c:19` 的按钮在 `(24, 24, 120, 36)`，阴影到 `(152, 68)`，远在画布内。
10. **阴影偏移的上限是 `UI_PADDING`（12），这是硬约束而非审美偏好**。两条理由：相邻控件的标准间距就是 `UI_PADDING`，偏移超过它会让阴影压到邻居上；`src/ui.c:225` 的 `buttonY = y + boxHeight - UI_PADDING - buttonHeight` 让按钮底边距框底恰好 12px，偏移达到 12 会正好落在框边框上，超过就出框。

    实测各偏移的实心带宽度精确等于设定值，抖动在带内保持 50% 交替（32 像素里 16 暗 16 亮），无混合像素。
11. **嵌套阴影**：`UiMessageBox` 里的按钮也会画阴影，落在面板底色上。按钮底边距框底 `UI_PADDING`（12），偏移 8 会在框内留 4px 余量，不越框。框内会同时出现「框的阴影」和「按钮的阴影」两层，偏移 8 时较明显，需在实机上确认不糊。
12. **焦点按钮与阴影同色**。焦点时按钮底色取 `buttonText`（`#000000`），与 `shadow` 同色，视觉上按钮会和自己的阴影连成一片。属可接受，但要在实机上确认不糊。
13. **三色边框吃掉 3px 内区，内缩必须同时作用于文字居中**。原表格有两处错：其一，墨迹高按单个汉字量得（9/18/27），而标题栏要容纳的是整串标题的墨迹并集；其二，「3px 边框后内区」按 `标题栏高 - 2*3` 计算，等于把框底的边框也算进了标题栏——但标题栏只占框顶 `size+2` 行，框底三圈在框底，与标题栏不相交，只有顶部的 ring0 会压到它。改按渲染回读实测（「标题测试」四字并集）：

    | 对外字号 | 墨迹高（并集） | 标题栏高 `size+2` | 1px 后内区 / 余量 | 3px 后内区 / 余量 |
    |---|---|---|---|---|
    | 12 | 11 | 14 | 13 / 2 | 11 / **0** |
    | 24 | 22 | 26 | 25 / 3 | 23 / 1 |
    | 36 | 33 | 38 | 37 / 4 | 35 / 2 |

    内区 = 标题栏高 - 圈数，余量 = 内区 - 墨迹高。

    **真正的坑不在放不放得下，而在居中公式**：原 `UiDrawBox` / `UiButton` 都按整块高度居中墨迹（`y + (h - inkHeight)/2`），完全没跳过边框，于是墨迹上缘必然落进 ring0——12/24/36 号分别压进 2/1/1 行。又因标题文字画在 `UiDrawFrame`（内含画边框）**之后**，不是边框压住字，而是字反过来把 ring0 啃出缺口。实测三档全部失败，不是只有 12 号。修法是把圈数作为垂直内缩量：`y + inset + (h - inset - inkHeight)/2 - inkTop`，`inset` 由 `BorderRingCount()` 统一给出，`DrawBorderRings` 与两处文字居中共用同一个函数，避免漂移。

    修好后 `triple` 在三档字号都放得下，但 **12 号余量恰为 0**，字顶字底都贴着内区边界，观感上没有呼吸空间。因此「`triple` 只在 24 号及以上使用」的建议保留，但理由改成「12 号零余量」，不是「放不下」。`UiDrawBox` 不强制拦截这个组合（会与「主题不管度量」的约定冲突），由主题作者与调用方各自负责。另注：墨迹高随标题用字而变，上表是「标题测试」这一串的实测值，换更密的字可能更高，故内缩量不能写死成常量。
14. **附带发现（既有问题，非本次引入）**：ASCII 文本在对外字号 24 下墨迹高是 **32**（本字体拉丁字形撑满整个行盒），已经超过标题栏的 26，`UiDrawBox` 会把标题画到框外 3px。当前界面标题全是中文，所以一直没暴露。本次改动不修它，但**不要顺手调低标题栏高度**去「迁就」它。
15. **`ClearBackground(DARKBLUE)` 目前是未提交改动**（原为 `YELLOW`）。本次改成读主题，`DARKBLUE` 不再硬编码。
16. **`line` 键不删**。`border_style = none` 时只是不画边框，键本身留给分隔线和将来使用；删键会破坏社区主题兼容。
17. **不要顺手改字体与度量**。`UI_PADDING`、`UI_LINE_STEP`、`UI_BUTTON_HEIGHT`、`UI_ALIGN12` 一律不动；`UI_TITLEBAR_HEIGHT` 也**不动**（原因见第 13、14 条）。

## 七、验证

1. **回读断言**（主要手段）。临时校验程序编译真实仓库源码（`ui.c`/`theme.c`/`font.c`/`graphsettings.c`）+ 一个自写 driver，渲染进渲染纹理后回读断言。放在临时目录，不入库。断言清单：
   - `desktop = #00008B` 时 `shadow_style` 解析为实心，阴影 L 形为 `#000000`，且实测偏移恰为 8px（框右下角外第 9 列才开始透明）
   - `desktop = #F5F5F5` 时解析为抖动，阴影行/列黑白各半、逐像素跳变
   - `border_style = none` 时四边外沿无任何非面板色像素
   - `border_style = single` 时外圈 1px 为 `line`
   - `border_style = triple` 时三圈依次为 `line`/`lineDim`/`line`，四角闭合，且 `lineDim` 不出现在内区
   - 抖动不漏进面板内部
   - 标题与按钮文字墨迹完整落在内区：`triple` 下 `ring0` 三行内没有任何标题色/按钮色像素，且墨迹像素数与 `none` 边框时逐档相等（12/24/36 三档字号都要测，见坑点第 13 条）
   - 消息框四边的阴影都完整可见，未被画布裁掉（`boxWidth <= 624`、`boxHeight` 数学上界 464、像素格取整后 `456`）
   - 加载 `default.cfg` 后 11 个色值与 2 个枚举值与第二节表格逐项一致；其中 `shadow_style = auto` 必须在深色桌面下解析为实心
   - 坏枚举值只跳过该行，其余键仍生效；只含枚举键的文件也能生效
2. **构建**：`./scripts/build.sh` 的 Release 与 Debug 均零告警。
3. **格式**：`clang-format --dry-run --Werror` 通过。
4. **启动**：无缺字告警，`UI: 主题` 加载成功，实际观感与 8px 偏移预览一致。
5. **裁剪边界**：消息框贴边时右下阴影完整可见。

## 八、明确不做

- 不动 VSYNC。
- 不做主题切换界面，不加命令行参数、环境变量或 `active.cfg` 标记文件。边框与阴影样式是**主题的一部分**，走已有的 `.cfg` 通路，不新开第二条选择通路。
- 不加载 12px 图集。
- 不引入 SDF、自定义 shader、第三方 GUI 库。
- 不删 `line` 键。
- 不支持三色边框的深-浅-深顺序。

## 九、提交拆分

按仓库既有风格（中文短句、无类型前缀、无 footer）拆两条：

1. `界面主题改为 VGA 配色，阴影与边框样式可配`：`ui.h`、`ui.c`、`theme.c`、`default.cfg`、`README.md`
2. `桌面底色改由主题决定`：`main.c`

## 十、待确认

无。第七节第 1 条的断言若暴露问题（尤其坑点 11 的标题栏高度），按实测值调整后重新走一遍验证。