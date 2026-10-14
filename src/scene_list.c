#include "scene_list.h"

#include <stddef.h>
#include <string.h>

typedef struct {
  const char* name;
  Scene* scene;
} SceneEntry;

// 名称与场景文件名一致，因此命令行取值与源码一一对应。
// 尚未落地的游玩界面不在表内：传入 game 会被当成未知名称报错，
// 而不是拿到一个尚未定义的场景。
static SceneEntry sScenes[] = {
    {"title", &TitleScene},
    {"settings", &SettingsScene},
    {"test", &TestScene},
};

Scene* SceneListFindByName(const char* name) {
  if (name == NULL) return NULL;
  const int count = (int)(sizeof(sScenes) / sizeof(sScenes[0]));
  for (int i = 0; i < count; i++) {
    if (strcmp(sScenes[i].name, name) == 0) return sScenes[i].scene;
  }
  return NULL;
}