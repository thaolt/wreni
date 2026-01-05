import "raylib" for Raylib as RL

RL.InitWindow(800, 600, "Wreni")
RL.SetTargetFPS(60)

var screenWidth = RL.GetScreenWidth()
var screenHeight = RL.GetScreenHeight()
var rectX = 100
var rectY = 100
var rectWidth = 100
var rectHeight = 100
var velocityX = 300  // pixels per second
var velocityY = 300  // pixels per second


while (!RL.WindowShouldClose()) {
    var deltaTime = RL.GetFrameTime()
    
    // Update position based on velocity and delta time
    rectX = rectX + (velocityX * deltaTime)
    rectY = rectY + (velocityY * deltaTime)
    
    // Check for collision with window edges and bounce
    if (rectX <= 0) {
        rectX = 0
        velocityX = -velocityX  // Reverse X velocity
    }
    if (rectX + rectWidth >= screenWidth) {
        rectX = screenWidth - rectWidth
        velocityX = -velocityX  // Reverse X velocity
    }
    if (rectY <= 0) {
        rectY = 0
        velocityY = -velocityY  // Reverse Y velocity
    }
    if (rectY + rectHeight >= screenHeight) {
        rectY = screenHeight - rectHeight
        velocityY = -velocityY  // Reverse Y velocity
    }
    
    RL.BeginDrawing()
    RL.ClearBackground(0xFF000000)
    RL.DrawRectangle(rectX, rectY, rectWidth, rectHeight, 0xFF660066)
    RL.EndDrawing()
}
RL.CloseWindow()

