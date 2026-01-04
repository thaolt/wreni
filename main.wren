import "raylib" for Raylib as RL

RL.InitWindow(800, 600, "Wreni")
while (!RL.WindowShouldClose()) {
    RL.BeginDrawing()
    RL.ClearBackground(0xFF186618)
    RL.EndDrawing()
}
RL.CloseWindow()

