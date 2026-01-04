class Raylib {

    #!extern(dll="raylib", args="int,int,char*")
    foreign static InitWindow(width, height, title)

    #!extern(dll="raylib")
    foreign static CloseWindow()
}
