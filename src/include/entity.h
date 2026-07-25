#include <stdbool.h>  // bool 类型需要这个头文件

#include "raylib.h"

// Entity template
typedef struct {
  Vector2 position;
  bool has_render;
  Texture2D texture;
  // more properties...
} Entity;
