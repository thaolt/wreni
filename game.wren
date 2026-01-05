import "raylib" for Raylib as RL

class Game {
    construct new() {
        // Initialize window first
        RL.InitWindow(800, 600, "Wreni - Paddle Ball Game")
        RL.SetTargetFPS(60)
        
        _screenWidth = 800
        _screenHeight = 600
        
        // Game state
        _score = 0
        _gameOver = false
        _gameStarted = false
        
        // Colors
        _backgroundColor = 0xFF1E1E2E
        _textColor = 0xFFCDD6F4
        
        // Initialize game objects
        _paddle = Paddle.new(_screenWidth, _screenHeight)
        _ball = Ball.new(_screenWidth, _screenHeight)
    }
    
    start() {
        while (!RL.WindowShouldClose()) {
            update()
            draw()
        }
        
        RL.CloseWindow()
    }
    
    update() {
        var deltaTime = RL.GetFrameTime()
        
        // Handle game restart
        if (_gameOver && RL.IsKeyPressed(32)) { // SPACE key
            reset()
        }
        
        // Update paddle and check for game start
        _paddle.update(deltaTime, _screenWidth)
        if (!_gameStarted && !_gameOver && (_paddle.isMovingLeft() || _paddle.isMovingRight())) {
            _gameStarted = true
        }
        
        // Update ball if game has started
        if (_gameStarted && !_gameOver) {
            _ball.update(deltaTime, _screenWidth, _screenHeight, _paddle)
            
            // Check for game over
            if (_ball.isGameOver(_screenHeight)) {
                _gameOver = true
            }
            
            // Update score
            if (_ball.scoredThisFrame()) {
                _score = _score + 10
                // Increase difficulty every 50 points
                if (_score % 50 == 0) {
                    _ball.increaseSpeed()
                }
            }
        }
    }
    
    draw() {
        RL.BeginDrawing()
        RL.ClearBackground(_backgroundColor)
        
        // Draw game objects
        _paddle.draw()
        _ball.draw()
        
        // Draw UI
        drawUI()
        
        RL.EndDrawing()
    }
    
    drawUI() {
        // Draw start screen
        if (!_gameStarted && !_gameOver) {
            drawStartScreen()
        }
        
        // Draw score
        if (_gameStarted) {
            RL.DrawText("SCORE: %(_score)", 10, 10, 20, _textColor)
        }
        
        // Draw game over screen
        if (_gameOver) {
            drawGameOverScreen()
        }
    }
    
    drawStartScreen() {
        var titleWidth = RL.MeasureText("PADDLE BALL GAME", 30)
        var controlsWidth = RL.MeasureText("Use LEFT and RIGHT arrow keys to move paddle", 20)
        var startWidth = RL.MeasureText("Press any arrow key to start", 20)
        var objectiveWidth = RL.MeasureText("Don't let the ball fall!", 18)
        
        RL.DrawText("PADDLE BALL GAME", (_screenWidth / 2) - (titleWidth / 2), _screenHeight / 2 - 80, 30, _textColor)
        RL.DrawText("Use LEFT and RIGHT arrow keys to move paddle", (_screenWidth / 2) - (controlsWidth / 2), _screenHeight / 2 - 30, 20, _textColor)
        RL.DrawText("Press any arrow key to start", (_screenWidth / 2) - (startWidth / 2), _screenHeight / 2, 20, 0xFF89B4FA)
        RL.DrawText("Don't let the ball fall!", (_screenWidth / 2) - (objectiveWidth / 2), _screenHeight / 2 + 30, 18, 0xFF9399B2)
    }
    
    drawGameOverScreen() {
        var gameOverWidth = RL.MeasureText("GAME OVER", 30)
        var restartWidth = RL.MeasureText("Press SPACE to restart", 20)
        
        RL.DrawText("GAME OVER", (_screenWidth / 2) - (gameOverWidth / 2), _screenHeight / 2 - 30, 30, 0xFFFF6B6B)
        RL.DrawText("Press SPACE to restart", (_screenWidth / 2) - (restartWidth / 2), _screenHeight / 2 + 10, 20, _textColor)
    }
    
    reset() {
        _score = 0
        _gameOver = false
        _gameStarted = false
        _paddle.reset()
        _ball.reset()
    }
}

class Paddle {
    construct new(screenWidth, screenHeight) {
        _width = 100
        _height = 15
        _speed = 500
        _color = 0xFF89B4FA
        
        _screenWidth = screenWidth
        _screenHeight = screenHeight
        
        reset()
        
        _movingLeft = false
        _movingRight = false
    }
    
    reset() {
        _x = _screenWidth / 2 - _width / 2
        _y = _screenHeight - 50
    }
    
    update(deltaTime, screenWidth) {
        _movingLeft = false
        _movingRight = false
        
        // Update position with arrow keys
        if (RL.IsKeyDown(263) && _x > 0) { // LEFT arrow
            _x = _x - (_speed * deltaTime)
            _movingLeft = true
        }
        if (RL.IsKeyDown(262) && _x < screenWidth - _width) { // RIGHT arrow
            _x = _x + (_speed * deltaTime)
            _movingRight = true
        }
    }
    
    draw() {
        RL.DrawRectangle(_x, _y, _width, _height, _color)
    }
    
    isMovingLeft() { _movingLeft }
    isMovingRight() { _movingRight }
    
    // Getters for collision detection
    x { _x }
    y { _y }
    width { _width }
    height { _height }
    color { _color }
}

class Ball {
    construct new(screenWidth, screenHeight) {
        _radius = 7.5
        _color = 0xFFF9E2AF
        _active = true
        _scoredThisFrame = false
        
        _screenWidth = screenWidth
        _screenHeight = screenHeight
        
        reset()
    }
    
    reset() {
        _x = _screenWidth / 2
        _y = _screenHeight / 2 + 200
        _speedX = 200
        _speedY = -300
        _active = true
        _scoredThisFrame = false
    }
    
    update(deltaTime, screenWidth, screenHeight, paddle) {
        _scoredThisFrame = false
        
        // Update position
        _x = _x + (_speedX * deltaTime)
        _y = _y + (_speedY * deltaTime)
        
        // Wall collisions
        checkWallCollisions(screenWidth, screenHeight)
        
        // Paddle collision
        checkPaddleCollision(paddle)
    }
    
    checkWallCollisions(screenWidth, screenHeight) {
        // Left and right walls
        if (_x - _radius <= 0 || _x + _radius >= screenWidth) {
            _speedX = -_speedX
            _x = _x - _radius <= 0 ? _radius : screenWidth - _radius
        }
        
        // Top wall
        if (_y - _radius <= 0) {
            _speedY = -_speedY
            _y = _radius
        }
    }
    
    checkPaddleCollision(paddle) {
        // Circle vs rectangle collision
        if (_y + _radius >= paddle.y && 
            _y - _radius <= paddle.y + paddle.height &&
            _x + _radius >= paddle.x && 
            _x - _radius <= paddle.x + paddle.width) {
            
            // Only bounce if moving downward
            if (_speedY > 0) {
                _speedY = -_speedY
                _y = paddle.y - _radius
                
                // Simple physics based on hit position
                var hitPos = (_x - paddle.x) / paddle.width
                
                if (hitPos < 0.33) {
                    _speedX = -200  // Bounce left
                } else if (hitPos > 0.67) {
                    _speedX = 200   // Bounce right
                } else {
                    _speedX = 0    // Bounce straight up
                }
                
                _scoredThisFrame = true
            }
        }
    }
    
    draw() {
        if (_active) {
            RL.DrawCircle(_x, _y, _radius, _color)
        }
    }
    
    increaseSpeed() {
        _speedY = _speedY * 1.1
        if (_speedX != 0) {
            _speedX = _speedX * 1.1
        }
    }
    
    isGameOver(screenHeight) {
        return _y > screenHeight
    }
    
    scoredThisFrame() { _scoredThisFrame }
}

// Start the game
var game = Game.new()
game.start()
