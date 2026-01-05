import "raylib" for Raylib as RL

// Initialize the game window
RL.InitWindow(800, 600, "Wreni - Paddle Ball Game")
RL.SetTargetFPS(60)

var screenWidth = RL.GetScreenWidth()
var screenHeight = RL.GetScreenHeight()

// Game variables
var paddleWidth = 100
var paddleHeight = 15
var paddleX = screenWidth / 2 - paddleWidth / 2
var paddleY = screenHeight - 50
var paddleSpeed = 500

var ballSize = 7.5  // Radius for the circle (was 15 for rectangle size)
var ballX = screenWidth / 2
var ballY = screenHeight / 2 + 200
var ballSpeedX = 200
var ballSpeedY = -300
var ballActive = true

var score = 0
var gameOver = false
var gameStarted = false

// Colors (using hex format: 0xAARRGGBB)
var backgroundColor = 0xFF1E1E2E
var paddleColor = 0xFF89B4FA
var ballColor = 0xFFF9E2AF
var textColor = 0xFFCDD6F4

// Main game loop
while (!RL.WindowShouldClose()) {
    var deltaTime = RL.GetFrameTime()
    
    // Reset game on SPACE when game over
    if (gameOver && RL.IsKeyPressed(32)) { // 32 is KEY_SPACE
        ballX = screenWidth / 2
        ballY = screenHeight / 2 + 200
        ballSpeedX = 200
        ballSpeedY = -300
        ballActive = true
        gameOver = false
        score = 0
        gameStarted = false
    }
    
    // Update paddle position with arrow keys using IsKeyDown (smooth movement)
    if (RL.IsKeyDown(263) && paddleX > 0) { // LEFT arrow key
        paddleX = paddleX - (paddleSpeed * deltaTime)
        if (!gameStarted && !gameOver) {
            gameStarted = true
        }
    }
    if (RL.IsKeyDown(262) && paddleX < screenWidth - paddleWidth) { // RIGHT arrow key
        paddleX = paddleX + (paddleSpeed * deltaTime)
        if (!gameStarted && !gameOver) {
            gameStarted = true
        }
    }
    
    // Update ball position if active and game has started
    if (ballActive && !gameOver && gameStarted) {
        ballX = ballX + (ballSpeedX * deltaTime)
        ballY = ballY + (ballSpeedY * deltaTime)
        
        // Ball collision with left and right walls
        if (ballX - ballSize <= 0 || ballX + ballSize >= screenWidth) {
            ballSpeedX = -ballSpeedX
            ballX = ballX - ballSize <= 0 ? ballSize : screenWidth - ballSize
        }
        
        // Ball collision with top wall
        if (ballY - ballSize <= 0) {
            ballSpeedY = -ballSpeedY
            ballY = ballSize
        }
        
        // Ball collision with paddle (circle vs rectangle)
        if (ballY + ballSize >= paddleY && 
            ballY - ballSize <= paddleY + paddleHeight &&
            ballX + ballSize >= paddleX && 
            ballX - ballSize <= paddleX + paddleWidth) {
            
            // Only bounce if ball is moving downward (to prevent multiple bounces)
            if (ballSpeedY > 0) {
                ballSpeedY = -ballSpeedY
                ballY = paddleY - ballSize
                
                // Simple physics: determine bounce direction based on where ball hits paddle
                var hitPos = (ballX - paddleX) / paddleWidth
                
                // Left third of paddle -> bounce left
                // Middle third -> bounce straight up  
                // Right third -> bounce right
                if (hitPos < 0.33) {
                    ballSpeedX = -200  // Bounce left
                } else if (hitPos > 0.67) {
                    ballSpeedX = 200   // Bounce right
                } else {
                    ballSpeedX = 0    // Bounce straight up
                }
                
                score = score + 10
                // Increase speed slightly every 50 points
                if (score % 50 == 0) {
                    ballSpeedY = ballSpeedY * 1.1
                    if (ballSpeedX != 0) {
                        ballSpeedX = ballSpeedX * 1.1
                    }
                }
            }
        }
        
        // Ball falls off the bottom - game over
        if (ballY > screenHeight) {
            ballActive = false
            gameOver = true
        }
    }
    
    // Drawing
    RL.BeginDrawing()
    RL.ClearBackground(backgroundColor)
    
    // Draw paddle
    RL.DrawRectangle(paddleX, paddleY, paddleWidth, paddleHeight, paddleColor)
    
    // Draw ball if active
    if (ballActive) {
        RL.DrawCircle(ballX, ballY, ballSize, ballColor)
    }
    
    // Draw start screen instructions if game hasn't started
    if (!gameStarted && !gameOver) {
        var titleWidth = RL.MeasureText("PADDLE BALL GAME", 30)
        var controlsWidth = RL.MeasureText("Use LEFT and RIGHT arrow keys to move paddle", 20)
        var startWidth = RL.MeasureText("Press any arrow key to start", 20)
        var objectiveWidth = RL.MeasureText("Don't let the ball fall!", 18)
        
        RL.DrawText("PADDLE BALL GAME", (screenWidth / 2) - (titleWidth / 2), screenHeight / 2 - 80, 30, textColor)
        RL.DrawText("Use LEFT and RIGHT arrow keys to move paddle", (screenWidth / 2) - (controlsWidth / 2), screenHeight / 2 - 30, 20, textColor)
        RL.DrawText("Press any arrow key to start", (screenWidth / 2) - (startWidth / 2), screenHeight / 2, 20, paddleColor)
        RL.DrawText("Don't let the ball fall!", (screenWidth / 2) - (objectiveWidth / 2), screenHeight / 2 + 30, 18, 0xFF9399B2)
    }
    
    // Draw score as text (only show when game has started)
    if (gameStarted) {
        RL.DrawText("SCORE: %(score)", 10, 10, 20, textColor)
    }
    
    // Draw game over indicator
    if (gameOver) {
        // Draw game over text
        var gameOverWidth = RL.MeasureText("GAME OVER", 30)
        var restartWidth = RL.MeasureText("Press SPACE to restart", 20)
        
        RL.DrawText("GAME OVER", (screenWidth / 2) - (gameOverWidth / 2), screenHeight / 2 - 30, 30, 0xFFFF6B6B)
        RL.DrawText("Press SPACE to restart", (screenWidth / 2) - (restartWidth / 2), screenHeight / 2 + 10, 20, textColor)
    }
    
    RL.EndDrawing()
}

RL.CloseWindow()