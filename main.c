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
void executeForeignFnWithMethodIndex(WrenVM* vm, int methodIndex);

// Structure to store FFI class information
typedef struct {
    char* className;
    char* moduleName;
    ObjClass* classObj;
} FFIClassInfo;

// Structure to store FFI method information
typedef struct {
    char* methodName;
    char* signature;
    ObjClass* classObj;
    uint16_t symbol;
    WrenForeignMethodFn uniqueFn; // Unique function pointer for this method
} FFIMethodInfo;

// Global list to store FFI classes
#define MAX_FFI_CLASSES 100
static FFIClassInfo ffiClasses[MAX_FFI_CLASSES];
static int ffiClassCount = 0;

// Global list to store FFI methods
#define MAX_FFI_METHODS 100
static FFIMethodInfo ffiMethods[MAX_FFI_METHODS];
static int ffiMethodCount = 0;

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

// Lightweight jump functions for each FFI method
void executeForeignFn_0(WrenVM* vm);
void executeForeignFn_1(WrenVM* vm);
void executeForeignFn_2(WrenVM* vm);
void executeForeignFn_3(WrenVM* vm);
void executeForeignFn_4(WrenVM* vm);
void executeForeignFn_5(WrenVM* vm);
void executeForeignFn_6(WrenVM* vm);
void executeForeignFn_7(WrenVM* vm);
void executeForeignFn_8(WrenVM* vm);
void executeForeignFn_9(WrenVM* vm);

// Array of function pointers for lightweight jumps
static WrenForeignMethodFn methodJumpFunctions[10];

// Function to add a method to the FFI method list
void addFFIMethod(const char* methodName, const char* signature, ObjClass* classObj, uint16_t symbol) {
    if (ffiMethodCount >= MAX_FFI_METHODS) {
        fprintf(stderr, "Warning: Maximum FFI methods reached, cannot add %s\n", methodName);
        return;
    }
    
    // Allocate memory for method name and signature
    ffiMethods[ffiMethodCount].methodName = strdup(methodName);
    ffiMethods[ffiMethodCount].signature = strdup(signature);
    ffiMethods[ffiMethodCount].classObj = classObj;
    ffiMethods[ffiMethodCount].symbol = symbol;
    
    // Assign a unique function pointer
    if (ffiMethodCount < sizeof(methodJumpFunctions) / sizeof(methodJumpFunctions[0])) {
        ffiMethods[ffiMethodCount].uniqueFn = methodJumpFunctions[ffiMethodCount];
    } else {
        ffiMethods[ffiMethodCount].uniqueFn = executeForeignFn_0; // Fallback to first function
    }
    
    fprintf(stderr, "Stored FFI method: %s (symbol: %d, index: %d)\n", methodName, symbol, ffiMethodCount);
    ffiMethodCount++;
}

// Function to find an FFI method by class object and symbol
FFIMethodInfo* findFFIMethod(ObjClass* classObj, uint16_t symbol) {
    for (int i = 0; i < ffiMethodCount; i++) {
        if (ffiMethods[i].classObj == classObj && ffiMethods[i].symbol == symbol) {
            return &ffiMethods[i];
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

// Function to execute foreign method with specific index
void executeForeignFnWithMethodIndex(WrenVM* vm, int methodIndex) {
    fprintf(stderr, "Executing foreign with method index %d\n", methodIndex);
    
    const char* methodName = "<unknown method>";
    const char* moduleName = "<unknown module>";
    const char* className = "<unknown class>";
    ObjClass* targetClass = NULL;

    if (methodIndex >= 0 && methodIndex < ffiMethodCount) {
        FFIMethodInfo* methodInfo = &ffiMethods[methodIndex];
        methodName = methodInfo->methodName;
        targetClass = methodInfo->classObj;
        
        if (targetClass != NULL && targetClass->name != NULL) {
            className = targetClass->name->value;
        }
        
        FFIClassInfo* ffiInfo = findFFIClassByObject(targetClass);
        if (ffiInfo != NULL && ffiInfo->moduleName != NULL) {
            moduleName = ffiInfo->moduleName;
        }
    }

    fprintf(stderr, "Executing foreign method %s.%s.%s\n", moduleName, className, methodName);
    dumpFrameStack(vm);
}

// Lightweight jump function implementations
void executeForeignFn_0(WrenVM* vm) { executeForeignFnWithMethodIndex(vm, 0); }
void executeForeignFn_1(WrenVM* vm) { executeForeignFnWithMethodIndex(vm, 1); }
void executeForeignFn_2(WrenVM* vm) { executeForeignFnWithMethodIndex(vm, 2); }
void executeForeignFn_3(WrenVM* vm) { executeForeignFnWithMethodIndex(vm, 3); }
void executeForeignFn_4(WrenVM* vm) { executeForeignFnWithMethodIndex(vm, 4); }
void executeForeignFn_5(WrenVM* vm) { executeForeignFnWithMethodIndex(vm, 5); }
void executeForeignFn_6(WrenVM* vm) { executeForeignFnWithMethodIndex(vm, 6); }
void executeForeignFn_7(WrenVM* vm) { executeForeignFnWithMethodIndex(vm, 7); }
void executeForeignFn_8(WrenVM* vm) { executeForeignFnWithMethodIndex(vm, 8); }
void executeForeignFn_9(WrenVM* vm) { executeForeignFnWithMethodIndex(vm, 9); }

// Initialize the jump function array
void initializeMethodJumpFunctions() {
    methodJumpFunctions[0] = executeForeignFn_0;
    methodJumpFunctions[1] = executeForeignFn_1;
    methodJumpFunctions[2] = executeForeignFn_2;
    methodJumpFunctions[3] = executeForeignFn_3;
    methodJumpFunctions[4] = executeForeignFn_4;
    methodJumpFunctions[5] = executeForeignFn_5;
    methodJumpFunctions[6] = executeForeignFn_6;
    methodJumpFunctions[7] = executeForeignFn_7;
    methodJumpFunctions[8] = executeForeignFn_8;
    methodJumpFunctions[9] = executeForeignFn_9;
}

// Helper to get method name from a class method table by symbol index
static const char* getMethodNameFromSymbol(WrenVM* vm, ObjClass* classObj, int symbol) {
    if (classObj == NULL || symbol < 0 || symbol >= classObj->methods.count) {
        return "<unknown method>";
    }
    
    Method* method = &classObj->methods.data[symbol];
    if (method->type != METHOD_FOREIGN) {
        return "<unknown method>";
    }
    
    // Look up the symbol name in VM's methodNames table
    if (symbol < vm->methodNames.count) {
        ObjString* name = vm->methodNames.data[symbol];
        if (name != NULL && name->value != NULL) {
            return name->value;
        }
    }
    
    return "<unknown method>";
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
    ObjClass* targetClass = NULL;

    if (vm != NULL) {
        // Determine the receiver and module/class information from API stack
        if (vm->apiStack != NULL) {
            Value receiver = vm->apiStack[0];

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

        // Try to find the method by examining the current call frame and using stored method info
        if (vm->fiber != NULL && vm->fiber->numFrames > 0 && targetClass != NULL) {
            // Look through all frames to find a CALL instruction
            for (int frameIndex = vm->fiber->numFrames - 1; frameIndex >= 0; frameIndex--) {
                CallFrame* frame = &vm->fiber->frames[frameIndex];
                if (frame != NULL && frame->closure != NULL && frame->closure->fn != NULL && frame->ip != NULL) {
                    ObjFn* fn = frame->closure->fn;
                    uint8_t* ip = frame->ip;
                    uint8_t* codeStart = fn->code.data;
                    
                    // Look backwards to find a CALL instruction
                    for (int offset = 3; offset <= 20 && ip >= codeStart + offset; offset++) {
                        uint8_t opcode = *(ip - offset);
                        if (opcode >= CODE_CALL_0 && opcode <= CODE_CALL_16) {
                            uint16_t symbol = (uint16_t)(((ip - offset + 1)[0] << 8) | (ip - offset + 2)[0]);
                            
                            // Try to find the method in our stored FFI methods
                            FFIMethodInfo* methodInfo = findFFIMethod(targetClass, symbol);
                            if (methodInfo != NULL) {
                                methodName = methodInfo->methodName;
                                goto found_method;
                            }
                            
                            // Fallback to the original method
                            methodName = getMethodNameFromSymbol(vm, targetClass, symbol);
                            goto found_method;
                        }
                    }
                }
            }
        }
        found_method:;
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

    // Extract method name from signature (remove parentheses and parameters)
    char methodName[256];
    strncpy(methodName, signature, sizeof(methodName) - 1);
    methodName[sizeof(methodName) - 1] = '\0';
    
    // Find the opening parenthesis and terminate the string there
    char* paren = strchr(methodName, '(');
    if (paren != NULL) {
        *paren = '\0';
    }
    
    fprintf(stderr, "Debug: extracted method name '%s' from signature '%s'\n", methodName, signature);
    
    // Try to find the symbol in the VM's methodNames table
    uint16_t symbol = 0;
    bool foundSymbol = false;
    
    fprintf(stderr, "Debug: VM has %d method names\n", vm->methodNames.count);
    
    // Look through the VM's methodNames to find the matching name
    for (int i = 0; i < vm->methodNames.count; i++) {
        ObjString* name = vm->methodNames.data[i];
        if (name != NULL && name->value != NULL) {
            fprintf(stderr, "Debug: methodNames[%d] = '%s'\n", i, name->value);
            if (strcmp(name->value, methodName) == 0) {
                symbol = (uint16_t)i;
                foundSymbol = true;
                fprintf(stderr, "Found symbol %d for method name %s\n", symbol, methodName);
                break;
            }
        }
    }
    
    if (foundSymbol) {
        addFFIMethod(methodName, signature, cls, symbol);
    } else {
        fprintf(stderr, "Warning: Could not find symbol for method %s, storing without symbol\n", methodName);
        addFFIMethod(methodName, signature, cls, 0); // Store with symbol 0 as placeholder
    }

    // Return the unique function pointer for this method
    return ffiMethods[ffiMethodCount - 1].uniqueFn;
}

int main()
{
    // Initialize method jump functions
    initializeMethodJumpFunctions();
    
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
