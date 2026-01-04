#include <wren.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#include <dlfcn.h>

#include <ffi.h>

#include <wren_vm.h>

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

void loadModuleCompleteFn(WrenVM* vm, const char* name, struct WrenLoadModuleResult result)
{
    fprintf(stderr, "Finish loading module '%s'\n", name);
}

WrenLoadModuleResult loadModuleFn(WrenVM* vm, const char* name)
{
    WrenLoadModuleResult result = {0};

    // ignore builtin modules "meta" and "random"
    if (strcmp(name, "meta") == 0 || strcmp(name, "random") == 0) {
        return result;
    }

    fprintf(stderr, "Loading module '%s'\n", name);
    
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
            result.onComplete = &loadModuleCompleteFn;
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

void allocateForeignClass(WrenVM* vm)
{
    fprintf(stderr, "Allocating foreign class\n");
}

WrenForeignClassMethods bindForeignClassFn(WrenVM* vm, const char* module,
    const char* className)
{
    if (strcmp(module, "meta") == 0 || strcmp(module, "random") == 0) {
        return (WrenForeignClassMethods){0};
    }
    
    WrenForeignClassMethods result = {0};
    result.allocate = &allocateForeignClass;
    result.finalize = NULL;
    fprintf(stderr, "Binding foreign class %s\n", className);

    
    return result;
}

void loadLibraryFn(WrenVM* vm)
{
    fprintf(stderr, "Loading library\n");
}


void logVarType(Value value) {
    if (IS_BOOL(value)) { fprintf(stderr, "Type = %s\n", "BOOL"); } else
    if (IS_CLASS(value)) { fprintf(stderr, "Type = %s\n", "CLASS"); } else
    if (IS_CLOSURE(value)) { fprintf(stderr, "Type = %s\n", "CLOSURE"); } else
    if (IS_FIBER(value)) { fprintf(stderr, "Type = %s\n", "FIBER"); } else
    if (IS_FN(value)) { fprintf(stderr, "Type = %s\n", "FN"); } else
    if (IS_FOREIGN(value)) { fprintf(stderr, "Type = %s\n", "FOREIGN"); } else
    if (IS_MAP(value)) { fprintf(stderr, "Type = %s\n", "MAP"); } else
    if (IS_INSTANCE(value)) { fprintf(stderr, "Type = %s\n", "INSTANCE"); } else
    if (IS_LIST(value)) { fprintf(stderr, "Type = %s\n", "LIST"); } else
    if (IS_RANGE(value)) { fprintf(stderr, "Type = %s\n", "RANGE"); } else
    if (IS_STRING(value)) { fprintf(stderr, "Type = %s\n", "STRING"); } else
    if (IS_OBJ(value)) { fprintf(stderr, "Type = %s\n", "OBJ"); } else
    if (IS_NUM(value)) { fprintf(stderr, "Type = %s\n", "NUM"); } else
    if (IS_NULL(value)) { fprintf(stderr, "Type = %s\n", "NULL"); } else
    if (IS_UNDEFINED(value)) { fprintf(stderr, "Type = %s\n", "UNDEFINED"); } else
    fprintf(stderr, "Type = %s\n", "UNKNOWN");
}

void executeForeignFn(WrenVM* vm)
{
    fprintf(stderr, "Executing foreign\n");

    // TODO: dump the fiber stack
    if (vm->fiber != NULL) {
        fprintf(stderr, "=== Fiber Stack Dump ===\n");
        
        // Dump call frames
        fprintf(stderr, "Call frames (%d/%d):\n", vm->fiber->numFrames, vm->fiber->frameCapacity);
        for (int i = 0; i < vm->fiber->numFrames; i++) {
            CallFrame* frame = &vm->fiber->frames[i];
            fprintf(stderr, "  Frame %d: closure=%p, stackStart=%p, ip=%p\n", 
                    i, (void*)frame->closure, (void*)frame->stackStart, (void*)frame->ip);
            if (frame->closure && frame->closure->fn && frame->closure->fn->debug && frame->closure->fn->debug->name) {
                fprintf(stderr, "    Function: %s\n", frame->closure->fn->debug->name);
            }
        }
        
        // Dump stack values
        int stackSize = (int)(vm->fiber->stackTop - vm->fiber->stack);
        fprintf(stderr, "Stack values (%d/%d used):\n", stackSize, vm->fiber->stackCapacity);
        for (int i = 0; i < stackSize; i++) {
            Value value = vm->fiber->stack[i];
            fprintf(stderr, "  [%d]: ", i);
            logVarType(value);
            
            // Print actual value for common types
            if (IS_NUM(value)) {
                fprintf(stderr, "    Value: %f", AS_NUM(value));
            } else if (IS_BOOL(value)) {
                fprintf(stderr, "    Value: %s", AS_BOOL(value) ? "true" : "false");
            } else if (IS_NULL(value)) {
                fprintf(stderr, "    Value: null");
            } else if (IS_STRING(value)) {
                fprintf(stderr, "    Value: \"%s\"", AS_CSTRING(value));
            } else if (IS_CLASS(value)) {
                fprintf(stderr, "    Value: %s", AS_CLASS(value)->name->value);
            } else if (IS_CLOSURE(value)) {
                fprintf(stderr, "    Value: %s", AS_CLOSURE(value)->fn->debug->name);
            }
            fprintf(stderr, "\n");
        }
        
        fprintf(stderr, "=== End Fiber Stack Dump ===\n");
    } else {
        fprintf(stderr, "No active fiber to dump\n");
    }

}

WrenForeignMethodFn bindForeignMethodFn(WrenVM* vm, const char* module,
    const char* className, bool isStatic, const char* signature)
{
    if (strcmp(module, "meta") == 0 || strcmp(module, "random") == 0) {
        return NULL;
    }

    if (strcmp(module, "root") == 0 && strcmp(signature, "loadLibrary(_)") == 0) {
        return &loadLibraryFn;
    }

    fprintf(stderr, "Binding foreign method %s.%s.%s\n", module, className, signature);
    fprintf(stderr, "Binding foreign method %p\n", executeForeignFn);


    return executeForeignFn;
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
    
    // WrenLoadModuleResult moduleResult = loadModuleFn(vm, "main");

    WrenInterpretResult result = wrenInterpret(vm, "start", "import \"main\"");
    
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
