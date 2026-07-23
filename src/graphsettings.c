#include "graphsettings.h"

// Function to enable fullscreen mode
void EnableFullscreen(void) {
  if (!IsWindowFullscreen()) {
    ToggleFullscreen();
  }
}

// Function to disable fullscreen mode
void DisableFullscreen(void) {
  if (IsWindowFullscreen()) {
    ToggleFullscreen();
  }
}

// Function to set fullscreen mode based on a boolean value
void SetFullscreen(bool enable) {
  if (IsWindowFullscreen() != enable) {
    ToggleFullscreen();
  }
}
