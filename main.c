#include <wren.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <dlfcn.h>

#include <ffi.h>

#include <wren_vm.h>

// Include debug functions
#include "wren/src/vm/wren_debug.h"

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
} FFIMethodInfo;

// Global list to store FFI classes
#define MAX_FFI_CLASSES 100
static FFIClassInfo ffiClasses[MAX_FFI_CLASSES];
static int ffiClassCount = 0;

// Global list to store FFI methods
#define MAX_FFI_METHODS 100
static FFIMethodInfo ffiMethods[MAX_FFI_METHODS];
static int ffiMethodCount = 0;

// Forward declarations for functions that need these structs
FFIClassInfo* findFFIClassByObject(ObjClass* classObj);
static const char* getMethodNameFromSymbol(WrenVM* vm, ObjClass* classObj, int symbol);

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
void executeForeignFn(WrenVM* vm)
{
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
    }

    // getting current frame
    CallFrame* frame = &vm->fiber->frames[vm->fiber->numFrames-1];
    ObjFn* fn = frame->closure->fn;

    uint16_t methodSymbol = (uint16_t) (*(frame->ip - 2) << 8 | *(frame->ip - 1));
    
    // Get the actual method name from the VM using the symbol
    if (targetClass != NULL) {
        methodName = vm->methodNames.data[methodSymbol]->value;
    }

    fprintf(stderr, "Executing foreign method %s.%s.%s\n", moduleName, className, methodName);

    // Extract method attributes after getting method name
    if (targetClass != NULL && targetClass->attributes != 0) {
        ObjInstance* attrInstance = AS_INSTANCE(targetClass->attributes);
        
        // fields[0] is the class's attributes, fields[1] is all the methods' attributes
        if (attrInstance->fields[1] != 0 && IS_MAP(attrInstance->fields[1])) {
            ObjMap* methodsMap = AS_MAP(attrInstance->fields[1]);
            
            // The method names in the attributes map include the full signature
            // We need to find the matching key by extracting the method signature from the end
            Value methodKey;
            bool found = false;
            for (int i = 0; i < methodsMap->capacity; i++) {
                MapEntry* entry = &methodsMap->entries[i];
                if (!IS_UNDEFINED(entry->key)) {
                    if (IS_STRING(entry->key)) {
                        ObjString* key = AS_STRING(entry->key);
                        
                        // Extract method signature from the end of the stored key
                        // Find the last space to get the method name with parameters
                        const char* keyStr = key->value;
                        const char* lastSpace = strrchr(keyStr, ' ');
                        
                        if (lastSpace != NULL) {
                            // Move past the space to get the method signature
                            const char* methodSignature = lastSpace + 1;
                            
                            // Compare with our method name
                            if (strcmp(methodSignature, methodName) == 0) {
                                methodKey = entry->key;
                                found = true;
                                break;
                            }
                        }
                    }
                }
            }
            
            if (found) {
                // Look up the method attributes in the methods map
                Value methodAttrsValue = wrenMapGet(methodsMap, methodKey);
                if (!IS_UNDEFINED(methodAttrsValue) && IS_MAP(methodAttrsValue)) {
                    ObjMap* methodAttrs = AS_MAP(methodAttrsValue);
                    
                    // Create string key for 'extern' attribute lookup using CONST_STRING macro
                    Value externKey = CONST_STRING(vm, "extern");
                    
                    // Look for 'extern' attribute directly
                    Value externValue = wrenMapGet(methodAttrs, externKey);
                    if (!IS_UNDEFINED(externValue) && IS_MAP(externValue)) {
                        ObjMap* externMap = AS_MAP(externValue);
                        
                        // Create string keys for dll and args lookup
                        Value dllKey = CONST_STRING(vm, "dll");
                        Value argsKey = CONST_STRING(vm, "args");
                        
                        // Extract dll attribute
                        Value dllValue = wrenMapGet(externMap, dllKey);
                        if (!IS_UNDEFINED(dllValue) && IS_LIST(dllValue)) {
                            ObjList* list = AS_LIST(dllValue);
                            if (list->elements.count > 0 && IS_STRING(list->elements.data[0])) {
                                ObjString* value = AS_STRING(list->elements.data[0]);
                                fprintf(stderr, "FFI Attribute dll: %s\n", value->value);
                            }
                        }
                        
                        // Extract args attribute
                        Value argsValue = wrenMapGet(externMap, argsKey);
                        if (!IS_UNDEFINED(argsValue) && IS_LIST(argsValue)) {
                            ObjList* list = AS_LIST(argsValue);
                            if (list->elements.count > 0 && IS_STRING(list->elements.data[0])) {
                                ObjString* value = AS_STRING(list->elements.data[0]);
                                fprintf(stderr, "FFI Attribute args: %s\n", value->value);
                            }
                        }
                    }
                }
            }
        }
    }
}

// Helper to get method name from a class method table by symbol index
static const char* getMethodNameFromSymbol(WrenVM* vm, ObjClass* classObj, int symbol) {
    if (classObj == NULL || symbol < 0 || symbol >= classObj->methods.count) {
        return "<unknown method>";
    }
    fprintf(stderr, "Symbol count: %d\n", vm->methodNames.count);
    
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
    // fprintf(stderr, "Binding foreign class %s\n", className);

    // Access the class object from the stack and check its superclass
    bool extendsFFI = false;
    ObjClass* classObj = NULL;
    if (vm->fiber && vm->fiber->stackTop > vm->fiber->stack) {
        Value classValue = vm->fiber->stackTop[-1];
        if (IS_CLASS(classValue)) {
            classObj = AS_CLASS(classValue);
            if (classObj->superclass != NULL && classObj->superclass->name != NULL) {
                // printf("bindForeignClassFn: current super class = %s\n", classObj->superclass->name->value);
                if (strcmp(classObj->superclass->name->value, "FFI") == 0) {
                    extendsFFI = true;
                }
            } else {
                // printf("bindForeignClassFn: no super class\n");
            }
        }
    }

    // Only provide allocate function if class extends from FFI
    if (extendsFFI && classObj != NULL) {
        result.allocate = &allocateForeignClass;
        // fprintf(stderr, "Class %s extends FFI - providing allocate function\n", className);
        
        // Store the FFI class information for later use
        addFFIClass(module, className, classObj);
        
        // Print all stored FFI classes for debugging
        printFFIClasses();
    } else {
        // fprintf(stderr, "Class %s does not extend FFI - no allocate function\n", className);
    }

    // wrenDumpStack(vm->fiber);

    return result;
}

void loadLibraryFn(WrenVM* vm)
{
    fprintf(stderr, "Loading library\n");
}


WrenForeignMethodFn bindForeignMethodFn(WrenVM* vm, const char* module,
    const char* className, bool isStatic, const char* signature)
{
    if (strcmp(module, "meta") == 0 || strcmp(module, "random") == 0) {
        return NULL;
    }

    // fprintf(stderr, "Binding foreign method %s.%s.%s\n", module, className, signature);
    
    // Check if this class is in our stored FFI classes list
    FFIClassInfo* ffiClass = findFFIClass(module, className);
    if (ffiClass == NULL) {
        // fprintf(stderr, "Class %s.%s not found in FFI classes list - returning NULL\n", module, className);
        return NULL;
    }
    
    // fprintf(stderr, "Found FFI class %s.%s in storage - providing foreign method\n", module, className);
    // fprintf(stderr, "Binding foreign method %p\n", executeForeignFn);
    
    ObjClass* cls = AS_CLASS(vm->fiber->stackTop[-1]);
    // fprintf(stderr, "Class: %s\n", cls->name->value);

    // Extract method name from signature (remove parentheses and parameters)
    char methodName[256];
    strncpy(methodName, signature, sizeof(methodName) - 1);
    methodName[sizeof(methodName) - 1] = '\0';
    
    // Find the opening parenthesis and terminate the string there
    char* paren = strchr(methodName, '(');
    if (paren != NULL) {
        *paren = '\0';
    }
    
    // Try to find the symbol in the VM's methodNames table
    uint16_t symbol = 0;
    bool foundSymbol = false;
    
    // fprintf(stderr, "Binding foreign method %s.%s.%s\n", module, className, signature);
    
    // Look through the VM's methodNames to find the matching name
    for (int i = 0; i < vm->methodNames.count; i++) {
        ObjString* name = vm->methodNames.data[i];
        if (name != NULL && name->value != NULL) {
            // Strip parentheses from VM method name for comparison
            char vmMethodName[256];
            strncpy(vmMethodName, name->value, sizeof(vmMethodName) - 1);
            vmMethodName[sizeof(vmMethodName) - 1] = '\0';
            
            char* paren = strchr(vmMethodName, '(');
            if (paren != NULL) {
                *paren = '\0';
            }
            
            if (strcmp(vmMethodName, methodName) == 0) {
                symbol = (uint16_t)i;
                foundSymbol = true;
                break;
            }
        }
    }
    
    if (foundSymbol) {
        addFFIMethod(methodName, signature, cls, symbol);
    } else {
        // fprintf(stderr, "Warning: Could not find symbol for method %s, storing without symbol\n", methodName);
        addFFIMethod(methodName, signature, cls, 0); // Store with symbol 0 as placeholder
    }

    // Return the standard foreign function (we'll use VM symbol table to find the method)
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
