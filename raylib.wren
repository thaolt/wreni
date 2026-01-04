foreign class Raylib is FFI {
    #!extern(dll="raylib", args="i32,i32,char*")
    foreign static InitWindow(width, height, title)

    #!extern(dll="raylib")
    foreign static BeginDrawing()

    #!extern(dll="raylib")
    foreign static EndDrawing()

    #!extern(dll="raylib", ret="bool")
    foreign static WindowShouldClose()

    #!extern(dll="raylib")
    foreign static CloseWindow()

    #!extern(dll="raylib", args="i64")
    foreign static ClearBackground(color)

    #!extern(dll="raylib", args="i32")
    foreign static SetTargetFPS(fps)

    #!extern(dll="raylib", ret="f32")
    foreign static GetFrameTime()

    #!extern(dll="raylib", args="i32,i32,i32,i32,i64")
    foreign static DrawRectangle(x, y, width, height, color)
}
