#ifndef ENTITY_H
#define ENTITY_H

#include <stdbool.h>

#include "raylib.h"

typedef struct {
  Vector2 position;
  bool has_render;
  Texture2D texture;
} Entity;

#endif
