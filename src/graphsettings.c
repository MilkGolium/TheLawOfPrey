#include "graphsettings.h"

#include "raylib.h"

void EnableFullscreen(void) {
  if (!IsWindowFullscreen()) {
    ToggleFullscreen();
  }
}

void DisableFullscreen(void) {
  if (IsWindowFullscreen()) {
    ToggleFullscreen();
  }
}

void SetFullscreen(bool enable) {
  if (IsWindowFullscreen() != enable) {
    ToggleFullscreen();
  }
}
