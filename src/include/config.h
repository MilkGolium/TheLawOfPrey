#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

// === 持久化配置 ===
//
// 键名、类型、默认值与合法范围只在 src/config.c 的键表里登记一份，
// 设置界面上线后由同一张表生成控件，界面与文件格式因此不会分叉。
//
// 生效优先级：启动参数 > 配置文件 > 代码默认值。
// 例如 --lang 覆盖配置里的 lang，--windowed 覆盖配置里的 fullscreen。
//
// 本阶段程序只读配置，绝不写回：回写与注释保留等设置界面上线时再定。

// 解析开始前指定配置文件的覆盖路径（来自 --config）。
// 传 NULL 表示用平台默认位置。不调用则等同于传 NULL。
void ConfigSetPathOverride(const char* path);

// 读取并解析配置文件。
// 文件不存在不算失败：全部用默认值并打一行日志。
// 返回 false 只发生在「显式指定的路径打不开」这类硬错误上，
// 调用方据此报错并以非零码退出。
bool ConfigLoad(void);

// 实际使用的配置文件路径，供日志使用。加载前返回空串。
const char* ConfigPath(void);

// 配置文件当前是否可写：文件存在时可写，或文件不存在但所在目录可写。
// 供将来的写入方判断，本阶段不写任何文件。
bool ConfigCanWrite(void);

// 取配置值。键名大小写敏感，与键表逐字符比较。
// 键名不在表里是程序错误：打一条 LOG_ERROR 并返回该类型的默认值，
// 不静默返回猜测值。
bool ConfigGetBool(const char* key);
int ConfigGetInt(const char* key);

// 取字符串配置值。返回指向内部缓冲区的指针：调用者不得释放，
// 有效期到下一次 ConfigLoad 为止。
const char* ConfigGetString(const char* key);

// 打一行日志：路径、四个键的生效值与权限状态。
// 权限的四种情形各有各的文案，见 src/config.c 的说明。
// appliedTheme 是主题层实际生效的主题名（main 传 GetCurrentThemeName()）：
// 配置里请求的名字可能被拒（认不出的主题会退回缺省），日志要打生效的那个。
void ConfigLogEffective(const char* appliedTheme);

#endif  // CONFIG_H
