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
    // Simple DLL handle cache - array of name->handle pairs
    struct {
        char* dllName;
        void* handle;
    } dllHandles[10];  // Support up to 10 different DLLs per class
    int dllHandleCount;
} FFIClassInfo;

// Structure to store FFI method information
typedef struct {
    char* methodName;
    char* signature;
    ObjClass* classObj;
    uint16_t symbol;
    // Store extracted FFI attributes
    char* dllName;
    char* argsSignature;
    char* retSignature;
    bool attributesExtracted;  // Flag to indicate if attributes have been extracted
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

// Function to store FFI class information
void storeFFIClass(WrenVM* vm, const char* className, const char* moduleName, ObjClass* classObj) {
    if (ffiClassCount >= MAX_FFI_CLASSES) {
        fprintf(stderr, "Too many FFI classes stored\n");
        return;
    }
    
    ffiClasses[ffiClassCount].className = strdup(className);
    ffiClasses[ffiClassCount].moduleName = strdup(moduleName);
    ffiClasses[ffiClassCount].classObj = classObj;
    ffiClasses[ffiClassCount].dllHandleCount = 0;  // Initialize DLL handle count
    
    fprintf(stderr, "Stored FFI class: module='%s', class='%s'\n", moduleName, className);
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
    
    // Initialize FFI attribute fields
    ffiMethods[ffiMethodCount].dllName = NULL;
    ffiMethods[ffiMethodCount].argsSignature = NULL;
    ffiMethods[ffiMethodCount].retSignature = NULL;
    ffiMethods[ffiMethodCount].attributesExtracted = false;
    
    ffiMethodCount++;
}

// Function to extract and store FFI attributes for a method
static void extractAndStoreFFIAttributes(WrenVM* vm, FFIMethodInfo* methodInfo, const char* methodName) {
    if (methodInfo == NULL || methodInfo->attributesExtracted) {
        return; // Already extracted or invalid
    }
    
    ObjClass* targetClass = methodInfo->classObj;
    if (targetClass == NULL || targetClass->attributes == 0) {
        return;
    }
    
    ObjInstance* attrInstance = AS_INSTANCE(targetClass->attributes);
    
    // fields[0] is the class's attributes, fields[1] is all the methods' attributes
    if (attrInstance->fields[1] != 0 && IS_MAP(attrInstance->fields[1])) {
        ObjMap* methodsMap = AS_MAP(attrInstance->fields[1]);
        
        // Find the method key in the attributes map
        Value methodKey;
        bool found = false;
        for (int i = 0; i < methodsMap->capacity; i++) {
            MapEntry* entry = &methodsMap->entries[i];
            if (!IS_UNDEFINED(entry->key)) {
                if (IS_STRING(entry->key)) {
                    ObjString* key = AS_STRING(entry->key);
                    const char* keyStr = key->value;
                    const char* lastSpace = strrchr(keyStr, ' ');
                    
                    if (lastSpace != NULL) {
                        const char* methodSignature = lastSpace + 1;
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
            Value methodAttrsValue = wrenMapGet(methodsMap, methodKey);
            if (!IS_UNDEFINED(methodAttrsValue) && IS_MAP(methodAttrsValue)) {
                ObjMap* methodAttrs = AS_MAP(methodAttrsValue);
                
                // Look for the 'extern' attribute
                for (int i = 0; i < methodAttrs->capacity; i++) {
                    MapEntry* entry = &methodAttrs->entries[i];
                    if (!IS_UNDEFINED(entry->key) && IS_STRING(entry->key)) {
                        ObjString* key = AS_STRING(entry->key);
                        if (strcmp(key->value, "extern") == 0 && IS_MAP(entry->value)) {
                            ObjMap* externMap = AS_MAP(entry->value);
                            
                            // Extract dll attribute
                            Value dllKey = CONST_STRING(vm, "dll");
                            Value dllValue = wrenMapGet(externMap, dllKey);
                            if (!IS_UNDEFINED(dllValue) && IS_LIST(dllValue)) {
                                ObjList* list = AS_LIST(dllValue);
                                if (list->elements.count > 0 && IS_STRING(list->elements.data[0])) {
                                    ObjString* value = AS_STRING(list->elements.data[0]);
                                    methodInfo->dllName = strdup(value->value);
                                }
                            }
                            
                            // Extract args attribute
                            Value argsKey = CONST_STRING(vm, "args");
                            Value argsValue = wrenMapGet(externMap, argsKey);
                            if (!IS_UNDEFINED(argsValue) && IS_LIST(argsValue)) {
                                ObjList* list = AS_LIST(argsValue);
                                if (list->elements.count > 0 && IS_STRING(list->elements.data[0])) {
                                    ObjString* value = AS_STRING(list->elements.data[0]);
                                    methodInfo->argsSignature = strdup(value->value);
                                }
                            }
                            
                            // Extract ret attribute
                            Value retKey = CONST_STRING(vm, "ret");
                            Value retValue = wrenMapGet(externMap, retKey);
                            if (!IS_UNDEFINED(retValue) && IS_LIST(retValue)) {
                                ObjList* list = AS_LIST(retValue);
                                if (list->elements.count > 0 && IS_STRING(list->elements.data[0])) {
                                    ObjString* value = AS_STRING(list->elements.data[0]);
                                    methodInfo->retSignature = strdup(value->value);
                                }
                            }
                            
                            methodInfo->attributesExtracted = true;
                            fprintf(stderr, "Extracted and cached FFI attributes for %s\n", methodName);
                            break;
                        }
                    }
                }
            }
        }
    }
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

// Helper function to find DLL handle in FFIClassInfo, load if not found
static void* getOrLoadDllHandle(WrenVM* vm, FFIClassInfo* ffiClass, const char* dllName) {
    if (ffiClass == NULL || dllName == NULL) return NULL;
    
    // Check if handle already cached in the array
    for (int i = 0; i < ffiClass->dllHandleCount; i++) {
        if (ffiClass->dllHandles[i].dllName != NULL && 
            strcmp(ffiClass->dllHandles[i].dllName, dllName) == 0) {
            return ffiClass->dllHandles[i].handle;
        }
    }
    
    // Load the DLL if not cached
    if (ffiClass->dllHandleCount >= 10) {
        fprintf(stderr, "Too many DLL handles cached for class %s\n", ffiClass->className);
        return NULL;
    }
    
    char libFileName[256];
    snprintf(libFileName, sizeof(libFileName), "./lib%s.so", dllName);
    
    void* handle = dlopen(libFileName, RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "Failed to load library %s: %s\n", libFileName, dlerror());
        return NULL;
    }
    
    // Cache the handle
    int index = ffiClass->dllHandleCount;
    ffiClass->dllHandles[index].dllName = strdup(dllName);
    ffiClass->dllHandles[index].handle = handle;
    ffiClass->dllHandleCount++;
    
    fprintf(stderr, "Cached DLL handle for %s in class %s (index %d)\n", dllName, ffiClass->className, index);
    return handle;
}

// Helper function to unload all DLL handles for a class
static void unloadAllDllHandles(FFIClassInfo* ffiClass) {
    if (ffiClass == NULL) return;
    
    // Iterate through all cached handles and unload them
    for (int i = 0; i < ffiClass->dllHandleCount; i++) {
        if (ffiClass->dllHandles[i].handle != NULL) {
            dlclose(ffiClass->dllHandles[i].handle);
            fprintf(stderr, "Unloaded DLL handle for %s in class %s\n", 
                    ffiClass->dllHandles[i].dllName, ffiClass->className);
            free(ffiClass->dllHandles[i].dllName);
            ffiClass->dllHandles[i].dllName = NULL;
            ffiClass->dllHandles[i].handle = NULL;
        }
    }
    
    ffiClass->dllHandleCount = 0;
}
static ObjModule* findModuleByClass(WrenVM* vm, ObjClass* targetClass) {
    if (vm == NULL || vm->modules == NULL || targetClass == NULL) {
        return NULL;
    }
    
    // Iterate through all modules in VM's module map
    for (int i = 0; i < vm->modules->capacity; i++) {
        MapEntry* entry = &vm->modules->entries[i];
        if (!IS_UNDEFINED(entry->key)) {
            if (wrenIsObjType(entry->value, OBJ_MODULE)) {
                ObjModule* module = AS_MODULE(entry->value);
                
                // Search through module variables to find our target class
                for (int j = 0; j < module->variables.count && j < module->variableNames.count; j++) {
                    Value varValue = module->variables.data[j];
                    
                    // Check if this variable is our target class
                    if (IS_CLASS(varValue) && AS_CLASS(varValue) == targetClass) {
                        return module;
                    }
                }
            }
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
                // Try to find module by target class pointer
                ObjModule* targetModule = findModuleByClass(vm, targetClass);
                if (targetModule != NULL && targetModule->name != NULL) {
                    moduleName = targetModule->name->value;
                }
            }
        }
    }

    // Check if this is a correctly stored FFI class
    FFIClassInfo* ffiClass = findFFIClass(moduleName, className);
    if (true) { // Skip validation for now
        if (ffiClass != NULL) {
            // fprintf(stderr, "Found FFI class: %s.%s\n", moduleName, className);
        }
    } else {
        // Report error back to Wren instead of exiting
        wrenSetSlotString(vm, 0, "FFI foreign class not found or not properly registered");
        wrenAbortFiber(vm, 0);
        return;
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

    // Find or create FFIMethodInfo for this method
    FFIMethodInfo* methodInfo = findFFIMethod(targetClass, methodSymbol);
    if (methodInfo == NULL) {
        // Create new method info if not found
        addFFIMethod(methodName, "foreign", targetClass, methodSymbol);
        methodInfo = findFFIMethod(targetClass, methodSymbol);
    }
    
    // Extract and cache attributes if not already done
    extractAndStoreFFIAttributes(vm, methodInfo, methodName);
    
    // Use cached attributes
    char* dllName = methodInfo->dllName;
    char* argsSignature = methodInfo->argsSignature;
    char* retSignature = methodInfo->retSignature;
    
    // Set default values if not specified
    if (retSignature == NULL) {
        retSignature = "void";
        fprintf(stderr, "FFI Attribute ret: void (default)\n");
    }
    
    if (argsSignature == NULL) {
        argsSignature = "";
        fprintf(stderr, "FFI Attribute args:  (default empty)\n");
    }
    
    // Print extracted attributes
    if (dllName != NULL) {
        fprintf(stderr, "FFI Attribute dll: %s\n", dllName);
    }

    // Perform FFI call if we have the required information
    if (dllName != NULL && methodName != NULL) {
        // Extract clean method name for FFI (remove parameter signature)
        char ffiFnName[256];
        const char* parenPos = strchr(methodName, '(');
        if (parenPos != NULL) {
            size_t nameLen = parenPos - methodName;
            strncpy(ffiFnName, methodName, nameLen);
            ffiFnName[nameLen] = '\0';
        } else {
            strcpy(ffiFnName, methodName);
        }
        
        fprintf(stderr, "Calling FFI: %s::%s(%s) -> %s\n", 
                dllName, ffiFnName, 
                argsSignature ? argsSignature : "void", 
                retSignature ? retSignature : "void");
        
        // Get FFIClassInfo for DLL handle caching
        FFIClassInfo* ffiClass = findFFIClassByObject(targetClass);
        if (ffiClass == NULL) {
            fprintf(stderr, "No FFI class info found for DLL caching, but continuing anyway\n");
            // Don't abort - just continue without caching
        }
        
        // Get or load DLL handle from cache (or directly if no class info)
        void* handle = NULL;
        if (ffiClass != NULL) {
            handle = getOrLoadDllHandle(vm, ffiClass, dllName);
        } else {
            // Fallback to direct loading
            char libFileName[256];
            snprintf(libFileName, sizeof(libFileName), "./lib%s.so", dllName);
            handle = dlopen(libFileName, RTLD_LAZY);
        }
        
        if (!handle) {
            fprintf(stderr, "Failed to get DLL handle for %s\n", dllName);
            wrenSetSlotString(vm, 0, "Failed to load dynamic library");
            wrenAbortFiber(vm, 0);
            goto cleanup;
        }
        
        // Get the function symbol
        void* func = dlsym(handle, ffiFnName);
        if (!func) {
            fprintf(stderr, "Failed to find function %s in %s: %s\n", ffiFnName, dllName, dlerror());
            wrenSetSlotString(vm, 0, "Function not found in library");
            wrenAbortFiber(vm, 0);
            goto cleanup;
        }
        
        // Parse arguments signature and set up libffi call
        ffi_cif cif;
        ffi_type** arg_types = NULL;
        void** arg_values = NULL;
        int* int_args = NULL;
        int64_t* i64_args = NULL;
        char** str_args = NULL;
        int arg_count = 0;
        ffi_type* ret_type = &ffi_type_void;
        
        // Parse return type
        if (retSignature != NULL && strcmp(retSignature, "int") == 0) {
            ret_type = &ffi_type_sint32;
        } else if (retSignature != NULL && strcmp(retSignature, "i64") == 0) {
            ret_type = &ffi_type_sint64;
        } else if (retSignature != NULL && strcmp(retSignature, "bool") == 0) {
            ret_type = &ffi_type_sint32;  // bool as int for simplicity
        }
        
        // Parse arguments signature
        if (argsSignature != NULL && strlen(argsSignature) > 0) {
            // Count arguments by counting commas + 1
            arg_count = 1;
            for (const char* p = argsSignature; *p; p++) {
                if (*p == ',') arg_count++;
            }
            
            arg_types = malloc(arg_count * sizeof(ffi_type*));
            arg_values = malloc(arg_count * sizeof(void*));
            int_args = malloc(arg_count * sizeof(int));
            i64_args = malloc(arg_count * sizeof(int64_t));
            str_args = malloc(arg_count * sizeof(char*));
            
            // Parse each argument type and set up values
            const char* start = argsSignature;
            int arg_index = 0;
            
            while (start && *start && arg_index < arg_count) {
                const char* end = strchr(start, ',');
                if (end == NULL) end = start + strlen(start);
                
                size_t len = end - start;
                char arg_type[32];
                if (len < sizeof(arg_type)) {
                    strncpy(arg_type, start, len);
                    arg_type[len] = '\0';
                    
                    // Remove any whitespace
                    char* trimmed = arg_type;
                    while (*trimmed == ' ') trimmed++;
                    
                    // Set argument type and get value from Wren stack
                    if (strcmp(trimmed, "int") == 0) {
                        arg_types[arg_index] = &ffi_type_sint32;
                        // Get int value from Wren stack (skip receiver)
                        if (arg_index + 1 < wrenGetSlotCount(vm)) {
                            int_args[arg_index] = (int)AS_NUM(vm->apiStack[arg_index + 1]);
                            arg_values[arg_index] = &int_args[arg_index];
                        }
                    } else if (strcmp(trimmed, "i64") == 0) {
                        arg_types[arg_index] = &ffi_type_sint64;
                        // Get int64_t value from Wren stack (skip receiver)
                        if (arg_index + 1 < wrenGetSlotCount(vm)) {
                            i64_args[arg_index] = (int64_t)AS_NUM(vm->apiStack[arg_index + 1]);
                            arg_values[arg_index] = &i64_args[arg_index];
                            fprintf(stderr, "i64 arg[%d] = %ld (0x%lx)\n", arg_index, i64_args[arg_index], i64_args[arg_index]);
                        }
                    } else if (strcmp(trimmed, "char*") == 0) {
                        arg_types[arg_index] = &ffi_type_pointer;
                        // Get string value from Wren stack
                        if (arg_index + 1 < wrenGetSlotCount(vm)) {
                            Value str_val = vm->apiStack[arg_index + 1];
                            if (IS_STRING(str_val)) {
                                str_args[arg_index] = AS_STRING(str_val)->value;
                                arg_values[arg_index] = &str_args[arg_index];
                            }
                        }
                    }
                    
                    arg_index++;
                }
                
                start = end ? end + 1 : NULL;
            }
        }
        
        // Initialize CIF
        ffi_status status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, arg_count, ret_type, arg_types);
        if (status != FFI_OK) {
            fprintf(stderr, "FFI prep_cif failed\n");
            if (arg_types) free(arg_types);
            if (arg_values) free(arg_values);
            if (int_args) free(int_args);
            if (i64_args) free(i64_args);
            if (str_args) free(str_args);
            // NOTE: Don't dlclose(handle) here to avoid unloading libraries that need to stay loaded
            // TODO: Implement proper library lifecycle management
            wrenSetSlotString(vm, 0, "FFI preparation failed");
            wrenAbortFiber(vm, 0);
            goto cleanup;
        }
        
        // Make the FFI call
        void* result = NULL;
        if (ret_type != &ffi_type_void) {
            if (ret_type == &ffi_type_sint64) {
                result = malloc(sizeof(int64_t));
            } else {
                result = malloc(sizeof(int));
            }
        }
        
        fprintf(stderr, "Making FFI call to %s with %d arguments\n", ffiFnName, arg_count);
        
        ffi_call(&cif, FFI_FN(func), result, arg_values);
        
        // Handle return value
        if (result != NULL) {
            if (ret_type == &ffi_type_sint64) {
                int64_t ret_val = *(int64_t*)result;
                fprintf(stderr, "FFI call returned: %ld\n", ret_val);
                
                // Set return value in Wren
                if (strcmp(retSignature, "i64") == 0) {
                    wrenSetSlotDouble(vm, 0, (double)ret_val);
                }
            } else {
                int ret_val = *(int*)result;
                fprintf(stderr, "FFI call returned: %d\n", ret_val);
                
                // Set return value in Wren
                if (strcmp(retSignature, "bool") == 0) {
                    wrenSetSlotBool(vm, 0, ret_val != 0);
                } else if (strcmp(retSignature, "int") == 0) {
                    wrenSetSlotDouble(vm, 0, ret_val);
                }
            }
            free(result);
        }
        
        if (arg_types) free(arg_types);
        if (arg_values) free(arg_values);
        if (int_args) free(int_args);
        if (i64_args) free(i64_args);
        if (str_args) free(str_args);
        
        // NOTE: DLL handle is cached in FFIClassInfo, don't unload here
        fprintf(stderr, "DLL handle cached in FFIClassInfo for %s\n", dllName);
    } else {
        fprintf(stderr, "Missing required FFI information:\n");
        fprintf(stderr, "  dllName: %s\n", dllName ? dllName : "NULL");
        fprintf(stderr, "  methodName: %s\n", methodName ? methodName : "NULL");
        fprintf(stderr, "  targetClass: %p\n", targetClass);
        wrenSetSlotString(vm, 0, "Missing FFI metadata");
        wrenAbortFiber(vm, 0);
    }

cleanup:
    // No cleanup needed since attributes are cached in FFIMethodInfo
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

// Finalizer function for FFI classes to unload DLL handles
static void finalizeFFIClass(void* data) {
    // Find the FFIClassInfo for this class
    FFIClassInfo* ffiClass = findFFIClassByObject((ObjClass*)data);
    if (ffiClass != NULL) {
        fprintf(stderr, "Finalizing FFI class %s - unloading DLL handles\n", ffiClass->className);
        unloadAllDllHandles(ffiClass);
    }
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
        result.finalize = &finalizeFFIClass;
        // fprintf(stderr, "Class %s extends FFI - providing allocate function\n", className);
        
        // Store the FFI class information for later use
        fprintf(stderr, "bindForeignClassFn: storing module='%s', class='%s'\n", module, className);
        storeFFIClass(vm, className, module, classObj);
        
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
