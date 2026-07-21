#include "raylib.h"

int main(void) {
    // Initialization
    const int screenWidth = 1024;
    const int screenHeight = 768;

    InitWindow(screenWidth, screenHeight, "The Law Of Prey");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        // Update
        ;

        // Draw
        BeginDrawing();
            ClearBackground(BLACK);
            DrawText("Hello, world!", 0, 0, 60, RED);
        EndDrawing();
    }

    // De-Initialization
    CloseWindow();
    return 0;
}
