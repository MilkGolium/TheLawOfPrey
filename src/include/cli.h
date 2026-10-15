#ifndef CLI_H
#define CLI_H

#include <stdbool.h>

// 开发工具用的启动参数，不是配置系统：目标帧数与分辨率这些要外置的设置
// 走另一条通路（README「游玩闭环」的「配置外置」），两者不揉在一起。
// 参数表与报错口径见 README「命令行参数（开发工具）」。

// --frames 的缺省帧数
#define CLI_DEFAULT_FRAMES 2

typedef struct {
  const char* sceneName;   // NULL 表示未指定，用缺省启动场景
  const char* exportPath;  // NULL 表示本次不导帧
  int frames;              // 仅在 exportPath 非 NULL 时有意义
  bool windowed;           // 本次不进全屏
  const char* lang;        // NULL 表示未指定，用缺省语言（见 lang.h）
} CliOptions;

// 解析 argv。先把 out 填成缺省值再逐项覆盖。成功返回 true；失败返回 false，
// 具体原因已写入日志，调用方负责打印用法并以非零码退出。
bool CliParse(int argc, char** argv, CliOptions* out);

// 打印用法到标准输出。
void CliPrintUsage(void);

// 启动时打一行日志记录本次生效的参数，sceneName 为实际生效的启动场景名。
void CliLogEffectiveOptions(const CliOptions* options, const char* sceneName);

#endif  // CLI_H