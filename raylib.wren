foreign class Raylib is FFI {
    #!extern(dll="raylib", args="int,int,char*")
    foreign static InitWindow(width, height, title)

    #!extern(dll="raylib")
    foreign static BeginDrawing()

    #!extern(dll="raylib")
    foreign static EndDrawing()

    #!extern(dll="raylib")
    foreign static WindowShouldClose()

    #!extern(dll="raylib")
    foreign static CloseWindow()
}
