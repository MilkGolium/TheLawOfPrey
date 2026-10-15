#include <stdbool.h>
#include <stddef.h>

#include "app.h"
#include "cli.h"
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

// 目标帧数仍写死，去硬编码是「游玩闭环」里的另一条待办
static const int kTargetFPS = 144;

// --scene 没给时的启动场景
static const char* const kDefaultSceneName = "title";

// 渲染纹理回读到 CPU 内存后上下颠倒（见 graphsettings.h），导出前翻正。
// raylib 6.0 没有写单个像素的接口，因此以一行为单位把源图第 y 行贴到
// 目标图的第 height - 1 - y 行。
static Image FlipImageVertically(Image image) {
  Image flipped = GenImageColor(image.width, image.height, BLANK);
  for (int row = 0; row < image.height; row++) {
    const Rectangle source = {0.0f, (float)(image.height - 1 - row),
                              (float)image.width, 1.0f};
    const Rectangle destination = {0.0f, (float)row, (float)image.width, 1.0f};
    ImageDraw(&flipped, image, source, destination, WHITE);
  }
  return flipped;
}

// 导出设计画布。画布纹理不是渲染目标、也不是窗口后备缓冲，因此导出结果
// 与窗口尺寸和 DPI 无关。失败（目录不存在、扩展名不受支持）返回 false。
static bool ExportCanvasFrame(const char* path) {
  const Image canvas = LoadImageFromTexture(GetCanvasTexture());
  const Image flipped = FlipImageVertically(canvas);
  const bool exported = ExportImage(flipped, path);
  UnloadImage(flipped);
  UnloadImage(canvas);
  return exported;
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
    TraceLog(LOG_ERROR, "找不到场景: %s", sceneName);
    CliPrintUsage();
    return 1;
  }

  // === 初始化 ===

  // 高 DPI：让帧缓冲等于物理像素，避免系统把整个窗口平滑放大成模糊。
  // 必须在 InitWindow 之前设置。导帧时窗口隐藏：导的是设计画布而不是
  // 桌面截图，窗口只负责提供 GL 上下文。
  unsigned int windowFlags = FLAG_WINDOW_HIGHDPI;
  if (options.exportPath != NULL) windowFlags |= FLAG_WINDOW_HIDDEN;
  SetConfigFlags(windowFlags);

  InitWindow(GRAPH_DESIGN_WIDTH, GRAPH_DESIGN_HEIGHT, "The Law Of Prey");
  SetExitKey(KEY_NULL);  // 禁用 ESC 退出按键
  SetTargetFPS(kTargetFPS);
  // 默认全屏；--windowed 关掉；导帧期间窗口隐藏，不进全屏
  SetFullscreen(!options.windowed && options.exportPath == NULL);

  // 用于切换开发和发行的 assets 位置。优先检查身边，其次根据项目结构来寻找。
  InitAssetsDirectory();

  // 界面语言必须先于字体定下来：码点收集就是扫当前语言的文案目录，
  // 晚了会让图集收错语言的字（见 lang.h 与 README）。
  LangSet(options.lang);

  // 加载中文字体图集，收集 codepoint
  InitGameFont();

  // 界面文案表。字体收集阶段直接扫描当前语言目录下的同一批文件，
  // 与这里的内存表各读一次互不依赖，先后顺序无所谓。
  InitStrings();

  // 字体度量就绪后再套主题，主题只允许改动配色与装饰形态
  InitTheme();

  // 设计画布：场景画在 640*480 的纹理里，再整数倍放大到窗口
  InitCanvas();

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
        TraceLog(LOG_INFO, "已导出设计画布: %s", options.exportPath);
      } else {
        TraceLog(LOG_ERROR, "导出失败（目录不存在或扩展名不受支持）: %s",
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