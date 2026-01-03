#include <wren.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <dlfcn.h>

#include <ffi.h>

void writeFn(WrenVM* vm, const char* text)
{
    printf("%s", text);
    fflush(stdout);
}

void errorFn(WrenVM* vm, WrenErrorType type, const char* module, int line,
    const char* message)
{
    fprintf(stderr, "%s.wren:%d: %s\n", module, line, message);
}

uint32_t readFile(char *buffer, uint32_t buf_size, const char* path)
{
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        return 0;
    }

    // TODO: use fseek to get the file size and check if exceed the buffer size
    fseek(file, 0, SEEK_END);
    uint32_t fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (fileSize < 0) {
        fprintf(stderr, "Could not determine file size for \"%s\".\n", path);
        fclose(file);
        return 0;
    }
    
    if (fileSize >= buf_size) {
        fprintf(stderr, "File \"%s\" is too large for buffer.\n", path);
        fclose(file);
        return 0;
    }

    uint32_t bytesRead = (uint32_t) fread(buffer, sizeof(char), buf_size - 1, file);
    
    if (bytesRead == 0 && ferror(file)) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        fclose(file);
        return 0;
    }
    
    buffer[bytesRead] = '\0';
    fclose(file);
    return bytesRead;
}

WrenLoadModuleResult loadModuleFn(WrenVM* vm, const char* name)
{
    WrenLoadModuleResult result = {0};
    
    // Create a buffer to hold the module source code
    char buffer[65536] = {0};
    char fileName[256] = {0};

    snprintf(fileName, 255, "%s.wren", name);
    
    // Try to read the module file
    uint32_t readLength = readFile(buffer, sizeof(buffer), fileName);
    if (readLength > 0) {
        // Allocate memory for the source code that will persist
        // We need to duplicate the buffer since readFile uses a local buffer
        char* source = malloc(readLength + 1);
        if (source != NULL) {
            strcpy(source, buffer);
            result.source = source;
            result.onComplete = NULL;  // No cleanup callback needed for simple malloc
            result.userData = NULL;
        } else {
            // Memory allocation failed
            result.source = NULL;
            result.onComplete = NULL;
            result.userData = NULL;
        }
    } else {
        // Module not found or couldn't be read
        result.source = NULL;
        result.onComplete = NULL;
        result.userData = NULL;
    }
    
    return result;
}

WrenForeignClassMethods bindForeignClassFn(WrenVM* vm, const char* module,
    const char* className)
{
    fprintf(stderr, "Binding foreign class %s\n", className);
    int slotCount = wrenGetSlotCount(vm);
    fprintf(stderr, "Slot count: %d\n", slotCount);

    
    
    return (WrenForeignClassMethods){0};
}

WrenForeignMethodFn bindForeignMethodFn(WrenVM* vm, const char* module,
    const char* className, bool isStatic, const char* signature)
{
    fprintf(stderr, "Binding foreign method %s.%s.%s\n", module, className, signature);
    int slotCount = wrenGetSlotCount(vm);
    fprintf(stderr, "Slot count: %d\n", slotCount);

    WrenInterpretResult result = wrenInterpret(vm, module, "System.print(Raylib.attributes.self)");
    
    return NULL;
}

int main()
{
    WrenConfiguration config;
    wrenInitConfiguration(&config);
    config.writeFn = &writeFn;
    config.errorFn = &errorFn;
    config.loadModuleFn = &loadModuleFn;
    config.bindForeignClassFn = &bindForeignClassFn;
    config.bindForeignMethodFn = &bindForeignMethodFn;
    
    WrenVM* vm = wrenNewVM(&config);

    WrenLoadModuleResult moduleResult = loadModuleFn(vm, "main");

    WrenInterpretResult result = wrenInterpret(vm, "main", moduleResult.source);
    
    if (result == WREN_RESULT_COMPILE_ERROR) {
        fprintf(stderr, "Compile error!\n");
        return 1;
    } else if (result == WREN_RESULT_RUNTIME_ERROR) {
        fprintf(stderr, "Runtime error!\n");
        return 1;
    }
    
    wrenFreeVM(vm);
    
    return 0;
}