import "meta" for Meta

#!dll = "NULL"
class LibraryLoader {
    construct new() {
        System.print(LibraryLoader.attributes.self)
    }
    
    foreign static loadLibrary(name)
    
    print(className) {
        System.print("Loading %(className)")
    }
}

var loader = LibraryLoader.new()
loader.print("Root")
