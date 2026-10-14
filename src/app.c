#include "app.h"

// 退出请求的记录位。只存状态，不做任何收尾动作。
static bool sQuitRequested = false;

void AppRequestQuit(void) { sQuitRequested = true; }

bool AppShouldQuit(void) { return sQuitRequested; }