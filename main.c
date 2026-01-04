#include <wren.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <dlfcn.h>

#include <ffi.h>

#include <wren_vm.h>

// Forward declarations
void dumpFrameStack(WrenVM* vm);

// Structure to store FFI class information
typedef struct {
    char* className;
    char* moduleName;
    ObjClass* classObj;
} FFIClassInfo;

// Global list to store FFI classes
#define MAX_FFI_CLASSES 100
static FFIClassInfo ffiClasses[MAX_FFI_CLASSES];
static int ffiClassCount = 0;

// Function to add a class to the FFI class list
void addFFIClass(const char* moduleName, const char* className, ObjClass* classObj) {
    if (ffiClassCount >= MAX_FFI_CLASSES) {
        fprintf(stderr, "Warning: Maximum FFI classes reached, cannot add %s.%s\n", moduleName, className);
        return;
    }
    
    // Allocate memory for class name and module name
    ffiClasses[ffiClassCount].className = strdup(className);
    ffiClasses[ffiClassCount].moduleName = strdup(moduleName);
    ffiClasses[ffiClassCount].classObj = classObj;
    
    fprintf(stderr, "Stored FFI class: %s.%s\n", moduleName, className);
    ffiClassCount++;
}

// Function to find an FFI class by name
FFIClassInfo* findFFIClass(const char* moduleName, const char* className) {
    for (int i = 0; i < ffiClassCount; i++) {
        if (strcmp(ffiClasses[i].moduleName, moduleName) == 0 && 
            strcmp(ffiClasses[i].className, className) == 0) {
            return &ffiClasses[i];
        }
    }
    return NULL;
}

// Function to find an FFI class by its object pointer
FFIClassInfo* findFFIClassByObject(ObjClass* classObj) {
    for (int i = 0; i < ffiClassCount; i++) {
        if (ffiClasses[i].classObj == classObj) {
            return &ffiClasses[i];
        }
    }
    return NULL;
}

// Function to print all stored FFI classes
void printFFIClasses() {
    fprintf(stderr, "=== Stored FFI Classes (%d) ===\n", ffiClassCount);
    for (int i = 0; i < ffiClassCount; i++) {
        fprintf(stderr, "%d: %s.%s (classObj: %p)\n", 
                i, ffiClasses[i].moduleName, ffiClasses[i].className, 
                (void*)ffiClasses[i].classObj);
    }
    fprintf(stderr, "=== End FFI Classes ===\n");
}

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
    result.allocate = NULL;
    result.finalize = NULL;
    fprintf(stderr, "Binding foreign class %s\n", className);

    // Access the class object from the stack and check its superclass
    bool extendsFFI = false;
    ObjClass* classObj = NULL;
    if (vm->fiber && vm->fiber->stackTop > vm->fiber->stack) {
        Value classValue = vm->fiber->stackTop[-1];
        if (IS_CLASS(classValue)) {
            classObj = AS_CLASS(classValue);
            if (classObj->superclass != NULL && classObj->superclass->name != NULL) {
                printf("bindForeignClassFn: current super class = %s\n", classObj->superclass->name->value);
                if (strcmp(classObj->superclass->name->value, "FFI") == 0) {
                    extendsFFI = true;
                }
            } else {
                printf("bindForeignClassFn: no super class\n");
            }
        }
    }

    // Only provide allocate function if class extends from FFI
    if (extendsFFI && classObj != NULL) {
        result.allocate = &allocateForeignClass;
        fprintf(stderr, "Class %s extends FFI - providing allocate function\n", className);
        
        // Store the FFI class information for later use
        addFFIClass(module, className, classObj);
        
        // Print all stored FFI classes for debugging
        printFFIClasses();
    } else {
        fprintf(stderr, "Class %s does not extend FFI - no allocate function\n", className);
    }

    dumpFrameStack(vm);

    return result;
}

void loadLibraryFn(WrenVM* vm)
{
    fprintf(stderr, "Loading library\n");
}


void logVarType(Value value) {
    if (IS_BOOL(value)) { fprintf(stderr, "Type = %s\n", "BOOL"); } else
    if (IS_CLASS(value)) { fprintf(stderr, "Type = %s\n", "CLASS"); } else
    if (IS_FIBER(value)) { fprintf(stderr, "Type = %s\n", "FIBER"); } else
    if (IS_FN(value)) { fprintf(stderr, "Type = %s\n", "FN"); } else
    if (IS_FOREIGN(value)) { fprintf(stderr, "Type = %s\n", "FOREIGN"); } else
    if (IS_MAP(value)) { fprintf(stderr, "Type = %s\n", "MAP"); } else
    if (IS_CLOSURE(value)) { fprintf(stderr, "Type = %s\n", "CLOSURE"); } else
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

void dumpFrameStack(WrenVM* vm) {
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

void executeForeignFn(WrenVM* vm)
{
    fprintf(stderr, "Executing foreign\n");

    const char* moduleName = "<unknown module>";
    const char* className = "<unknown class>";
    const char* methodName = "<unknown method>";

    if (vm != NULL) {
        // Determine the receiver and module/class information from the API stack
        if (vm->apiStack != NULL) {
            Value receiver = vm->apiStack[0];
            ObjClass* targetClass = NULL;

            if (IS_CLASS(receiver)) {
                targetClass = AS_CLASS(receiver);
            } else if (IS_INSTANCE(receiver)) {
                targetClass = AS_INSTANCE(receiver)->obj.classObj;
            }

            if (targetClass != NULL) {
                if (targetClass->name != NULL) {
                    className = targetClass->name->value;
                }

                FFIClassInfo* ffiInfo = findFFIClassByObject(targetClass);
                if (ffiInfo != NULL && ffiInfo->moduleName != NULL) {
                    moduleName = ffiInfo->moduleName;
                }
            }
        }

        // Attempt to recover the method signature from the current call frame
        if (vm->fiber != NULL && vm->fiber->numFrames > 0) {
            CallFrame* frame = &vm->fiber->frames[vm->fiber->numFrames - 1];
            if (frame->closure != NULL && frame->closure->fn != NULL && frame->ip != NULL) {
                ObjFn* fn = frame->closure->fn;
                uint8_t* ip = frame->ip;
                if (ip >= fn->code.data + 3) {
                    uint8_t opcode = *(ip - 3);

                    bool isCall = (opcode >= CODE_CALL_0 && opcode <= CODE_CALL_16);
                    if (isCall) {
                        uint16_t symbol = (uint16_t)(((ip - 2)[0] << 8) | (ip - 1)[0]);
                        if (symbol < vm->methodNames.count) {
                            ObjString* name = vm->methodNames.data[symbol];
                            if (name != NULL && name->value != NULL) {
                                methodName = name->value;
                            }
                        }
                    }
                }
            }
        }
    }

    fprintf(stderr, "Executing foreign method %s.%s.%s\n", moduleName, className, methodName);

    dumpFrameStack(vm);
}

WrenForeignMethodFn bindForeignMethodFn(WrenVM* vm, const char* module,
    const char* className, bool isStatic, const char* signature)
{
    if (strcmp(module, "meta") == 0 || strcmp(module, "random") == 0) {
        return NULL;
    }

    fprintf(stderr, "Binding foreign method %s.%s.%s\n", module, className, signature);
    
    // Check if this class is in our stored FFI classes list
    FFIClassInfo* ffiClass = findFFIClass(module, className);
    if (ffiClass == NULL) {
        fprintf(stderr, "Class %s.%s not found in FFI classes list - returning NULL\n", module, className);
        return NULL;
    }
    
    fprintf(stderr, "Found FFI class %s.%s in storage - providing foreign method\n", module, className);
    fprintf(stderr, "Binding foreign method %p\n", executeForeignFn);
    
    ObjClass* cls = AS_CLASS(vm->fiber->stackTop[-1]);
    fprintf(stderr, "Class: %s\n", cls->name->value);

    // TODO: How could we extract the method attributes here?

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
    WrenInterpretResult result;

    result = wrenInterpret(vm, NULL, "class FFI {}\n");

    result = wrenInterpret(vm, "start", "import \"main\"");
    
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
