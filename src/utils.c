#include "utils.h"

#include <raylib.h>

void InitAssetsDirectory(void) {
  // 首先切换到程序自身所在的目录
  ChangeDirectory(GetApplicationDirectory());

  // 检查程序身边的 assets 是否存在
  if (DirectoryExists("assets")) {
    TraceLog(LOG_INFO, "正在使用程序身边的 assets 目录：%s/assets",
             GetWorkingDirectory());
  }
  // 如果身边没有，且编译时指定了项目根目录，尝试使用 CMake 指定的路径
#ifdef PROJECT_ROOT_DIR
  else if (DirectoryExists(TextFormat("%s/assets", PROJECT_ROOT_DIR))) {
    ChangeDirectory(PROJECT_ROOT_DIR);
    TraceLog(LOG_WARNING, "程序身边的 assets 不存在，已回退至开发目录：%s",
             GetWorkingDirectory());
  }
#endif
  else {
    TraceLog(LOG_ERROR, "未找到 assets 目录！当前工作目录：%s",
             GetWorkingDirectory());
  }
}
