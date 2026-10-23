#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "app.h"
#include "cli.h"
#include "config.h"
#include "font.h"
#include "graphsettings.h"
#include "lang.h"
#include "raylib.h"
#include "scene_list.h"
#include "scene_manager.h"
#include "strings.h"
#include "theme.h"
#include "ui.h"
#include "utils.h"

// --scene 没给时的启动场景
static const char* const kDefaultSceneName = "title";

// 语言标识既可能来自命令行参数，也可能来自配置文件。配置文件里的
// 标识放在 ConfigGetString 的内部缓冲里，复制一份再交给 LangSet，
// 免得日后再次加载配置时把当前语言指过去。
static char sLangId[32] = "";

// 导出设计画布。画布纹理不是渲染目标、也不是窗口后备缓冲，因此导出结果
// 与窗口尺寸和 DPI 无关。失败（目录不存在、扩展名不受支持）返回 false。
static bool ExportCanvasFrame(const char* path) {
  // 渲染纹理回读到 CPU 内存后是上下颠倒的（见 graphsettings.h），导出前翻正。
  // 用 ImageFlipVertical 就地翻：手写逐行搬运要依赖 ImageDraw，而那个函数在
  // raylib 6.5 里已经被移除（6.0 还在），换版本就编译不过。
  Image canvas = LoadImageFromTexture(GetCanvasTexture());
  ImageFlipVertical(&canvas);
  const bool exported = ExportImage(canvas, path);
  UnloadImage(canvas);
  return exported;
}

// 语言标识的来源有三级：命令行参数 > 配置文件 > 缺省。
// 配置文件里的标识先过 LangIsKnown，不认识就告警并退回缺省。
static const char* ResolveLanguage(const CliOptions* options) {
  const char* source = NULL;
  if (options->lang != NULL) {
    source = options->lang;
  } else {
    const char* configured = ConfigGetString("lang");
    if (configured != NULL && configured[0] != '\0') {
      if (LangIsKnown(configured)) {
        source = configured;
      } else {
        TraceLog(LOG_WARNING, "Config: unknown language %s, keeping %s",
                 configured, LangCurrent());
      }
    }
  }

  if (source == NULL) return NULL;

  size_t length = strlen(source);
  if (length >= sizeof(sLangId)) length = sizeof(sLangId) - 1;
  memcpy(sLangId, source, length);
  sLangId[length] = '\0';
  return sLangId;
}

// 配置里的主题若与当前生效的主题不同，按名字加载。主题名必须能在扫描
// 结果里找到，否则说明配置写错了或主题文件不在包里，保持当前主题并告警。
static void ApplyConfiguredTheme(void) {
  const char* wanted = ConfigGetString("theme");
  if (wanted == NULL || wanted[0] == '\0') return;
  if (strcmp(wanted, GetCurrentThemeName()) == 0) return;

  bool found = false;
  const int count = GetThemeCount();
  for (int i = 0; i < count; i++) {
    if (strcmp(wanted, GetThemeName(i)) == 0) {
      found = true;
      break;
    }
  }

  if (!found) {
    TraceLog(LOG_WARNING, "Config: unknown theme %s, keeping %s", wanted,
             GetCurrentThemeName());
    return;
  }

  if (!LoadTheme(wanted)) {
    TraceLog(LOG_WARNING, "Config: the theme %s failed to load, keeping %s",
             wanted, GetCurrentThemeName());
  }
}

int main(int argc, char** argv) {
  // === 命令行参数 ===
  // 此刻字体、文案、窗口都还没初始化，解析失败可以直接走人

  CliOptions options;
  if (!CliParse(argc, argv, &options)) {
    CliPrintUsage();
    return 1;
  }

  const char* sceneName =
      options.sceneName != NULL ? options.sceneName : kDefaultSceneName;
  Scene* startScene = SceneListFindByName(sceneName);
  if (startScene == NULL) {
    TraceLog(LOG_ERROR, "scene not found: %s", sceneName);
    CliPrintUsage();
    return 1;
  }

  // 配置文件在开窗口之前读：全屏与目标帧数都要用它的值。显式指定的
  // 路径打不开是硬错误（那是打错字的典型场景），此时非零退出，
  // 不静默退回默认值。
  ConfigSetPathOverride(options.configPath);
  if (!ConfigLoad()) return 1;

  // === 初始化 ===

  // 高 DPI：让帧缓冲等于物理像素，避免系统把整个窗口平滑放大成模糊。
  // 必须在 InitWindow 之前设置。导帧时窗口隐藏：导的是设计画布而不是
  // 桌面截图，窗口只负责提供 GL 上下文。
  unsigned int windowFlags = FLAG_WINDOW_HIGHDPI;
  if (options.exportPath != NULL) windowFlags |= FLAG_WINDOW_HIDDEN;
  SetConfigFlags(windowFlags);

  InitWindow(GRAPH_DESIGN_WIDTH, GRAPH_DESIGN_HEIGHT, "The Law Of Prey");
  SetExitKey(KEY_NULL);  // 禁用 ESC 退出按键
  SetTargetFPS(ConfigGetInt("target_fps"));
  // 默认全屏；--windowed 关掉；导帧期间窗口隐藏，不进全屏；
  // 配置里的 fullscreen 给出缺省倾向
  SetFullscreen(!options.windowed && options.exportPath == NULL &&
                ConfigGetBool("fullscreen"));

  // 用于切换开发和发行的 assets 位置。优先检查身边，其次根据项目结构来寻找。
  InitAssetsDirectory();

  // 界面语言必须先于字体定下来：码点收集就是扫当前语言的文案目录，
  // 晚了会让图集收错语言的字（见 lang.h 与 README）。
  LangSet(ResolveLanguage(&options));

  // 加载中文字体图集，收集 codepoint
  InitGameFont();

  // 界面文案表。字体收集阶段直接扫描当前语言目录下的同一批文件，
  // 与这里的内存表各读一次互不依赖，先后顺序无所谓。
  InitStrings();

  // 字体度量就绪后再套主题，主题只允许改动配色与装饰形态
  InitTheme();

  // 配置里的主题在 InitTheme 之后应用：主题列表要等扫描完才知道有哪些
  ApplyConfiguredTheme();

  // 设计画布：场景画在 640*480 的纹理里，再整数倍放大到窗口
  InitCanvas();

  // 配置的生效值与权限状态打一行，放在场景切换之前
  ConfigLogEffective(GetCurrentThemeName());

  // 初始场景切换放在字体、文案、主题、画布都就绪之后：场景的 Init 会用到
  // 这些资源，早于它们切换会拿到空表。启动与运行时走同一条通路，
  // 提请求再执行一次 Process，不直接调底层切换。
  InitSceneManager();
  SceneManager_RequestSceneSwitch(startScene);
  SceneManager_ProcessPendingSwitch();

  CliLogEffectiveOptions(&options, sceneName);

  // === 主循环 ===
  // 三个出口在这里汇合：程序内退出（按钮或 Ctrl+Q）、系统关窗、导帧完成。
  // 它们的卸载顺序完全相同。
  int exitCode = 0;
  int framesDrawn = 0;
  bool exportPending = options.exportPath != NULL;

  while (!WindowShouldClose() && !AppShouldQuit()) {
    // 切换请求统一在本帧所有场景回调之外执行，因此不会出现回调中途换场景
    SceneManager_ProcessPendingSwitch();
    SceneManager_UpdateCurrentScene();

    // 场景一律画在设计画布内，坐标即设计分辨率坐标
    BeginCanvas();
    ClearBackground(gUiStyle.desktop);
    SceneManager_DrawCurrentScene();
    EndCanvas();

    // 画布整数倍放大到窗口，未覆盖的部分留作黑边
    BeginDrawing();
    ClearBackground(BLACK);
    DrawCanvas();
    EndDrawing();

    framesDrawn++;
    if (exportPending && framesDrawn >= options.frames) {
      exportPending = false;
      if (ExportCanvasFrame(options.exportPath)) {
        TraceLog(LOG_INFO, "exported the design canvas: %s",
                 options.exportPath);
      } else {
        TraceLog(
            LOG_ERROR,
            "export failed (directory missing or unsupported extension): %s",
            options.exportPath);
        exitCode = 1;
      }
      break;
    }
  }

  // === 主循环结束，清理并退出 ===

  // 空场景检测已经包含在函数内，无需再次检测
  SceneManager_UnloadCurrentScene();
  // 释放字体图集（必须在 CloseWindow 之前，此时 GL 上下文尚在）
  UnloadGameFont();
  // 释放界面文案表
  UnloadStrings();
  // 释放抖动阴影贴图，同样必须在 CloseWindow 之前
  UiUnloadStyle();
  // 释放设计画布，同样必须在 CloseWindow 之前
  UnloadCanvas();
  CloseWindow();
  return exitCode;
}