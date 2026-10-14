#include "cli.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "scene_list.h"

// 参数一律写作 --key=value：不支持短选项、不支持 / 、不支持位置参数。
// 未知参数、缺 =、同一参数重复给出都报错退出。
static const char* const kUsage =
    "用法: TheLawOfPrey [--scene=<name>] [--export-frame=<path>]\n"
    "                      [--frames=<n>] [--windowed]\n"
    "\n"
    "  --scene=<name>       启动场景，取值 title / settings / test\n"
    "  --export-frame=<path> 渲染若干帧后导出设计画布（640*480）并退出，\n"
    "                       导出期间窗口隐藏。格式由扩展名决定\n"
    "  --frames=<n>         与 --export-frame 同用的帧数，缺省 2，必须 >= 1，\n"
    "                       单独给出视为用法错误\n"
    "  --windowed           本次不进全屏；与 --export-frame 同给时不生效\n";

// 比较 arg 的 key 段（'=' 之前）与给定 key 是否逐字相同
static bool KeyEquals(const char* arg, size_t keyLength, const char* key) {
  return strlen(key) == keyLength && strncmp(arg, key, keyLength) == 0;
}

static bool Reject(const char* format, const char* detail) {
  TraceLog(LOG_ERROR, format, detail);
  return false;
}

// 只接受十进制非负整数串，值须 >= 1。返回 false 表示写法非法。
static bool ParseFrames(const char* value, int* out) {
  if (value[0] == '\0') return false;
  for (const char* p = value; *p != '\0'; p++) {
    if (*p < '0' || *p > '9') return false;
  }
  errno = 0;
  const long parsed = strtol(value, NULL, 10);
  if (errno != 0 || parsed < 1 || parsed > 0x7FFFFFFF) return false;
  *out = (int)parsed;
  return true;
}

bool CliParse(int argc, char** argv, CliOptions* out) {
  out->sceneName = NULL;
  out->exportPath = NULL;
  out->frames = CLI_DEFAULT_FRAMES;
  out->windowed = false;

  bool seenScene = false;
  bool seenExportFrame = false;
  bool seenFrames = false;
  bool seenWindowed = false;

  for (int i = 1; i < argc; i++) {
    const char* arg = argv[i];

    // 位置参数与 / 开头的写法都在这里被拦下
    if (strncmp(arg, "--", 2) != 0)
      return Reject("参数不接受位置参数或短选项: %s", arg);

    // --windowed 是开关，不带取值，其余参数都要 --key=value
    if (strcmp(arg, "--windowed") == 0) {
      if (seenWindowed) return Reject("同一参数重复给出: %s", arg);
      seenWindowed = true;
      out->windowed = true;
      continue;
    }

    const char* separator = strchr(arg, '=');
    if (separator == NULL) return Reject("参数缺少 =: %s", arg);

    const size_t keyLength = (size_t)(separator - arg);
    const char* value = separator + 1;

    if (KeyEquals(arg, keyLength, "--scene")) {
      if (seenScene) return Reject("同一参数重复给出: %s", arg);
      seenScene = true;
      // 场景名与场景文件名一致，查表校验，未知取值不静默回落到标题场景
      if (SceneListFindByName(value) == NULL)
        return Reject("未知的场景名: %s", value);
      out->sceneName = value;
    } else if (KeyEquals(arg, keyLength, "--export-frame")) {
      if (seenExportFrame) return Reject("同一参数重复给出: %s", arg);
      seenExportFrame = true;
      if (value[0] == '\0') return Reject("导出路径为空: %s", arg);
      out->exportPath = value;
    } else if (KeyEquals(arg, keyLength, "--frames")) {
      if (seenFrames) return Reject("同一参数重复给出: %s", arg);
      seenFrames = true;
      int frames = 0;
      if (!ParseFrames(value, &frames))
        return Reject("帧数须为 >= 1 的整数: %s", arg);
      out->frames = frames;
    } else {
      return Reject("未知参数: %s", arg);
    }
  }

  // 帧数只服务于导帧，单独给出没有意义
  if (seenFrames && !seenExportFrame) {
    TraceLog(LOG_ERROR, "--frames 只能与 --export-frame 同用");
    return false;
  }

  return true;
}

void CliPrintUsage(void) { fputs(kUsage, stdout); }

void CliLogEffectiveOptions(const CliOptions* options, const char* sceneName) {
  TraceLog(LOG_INFO, "启动参数: scene=%s export-frame=%s frames=%d windowed=%s",
           sceneName, options->exportPath ? options->exportPath : "(无)",
           options->frames, options->windowed ? "是" : "否");
  if (options->windowed && options->exportPath != NULL) {
    TraceLog(LOG_INFO,
             "启动参数: --windowed 与 --export-frame 同给，导帧期间不生效");
  }
}