#include <wren.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#include <dlfcn.h>

#include <ffi.h>

#include <wren_vm.h>

// Structure to store foreign method metadata
typedef struct {
    char* dll;
    char* args;
    char* signature;
    char* className;
    char* methodName;
} ForeignMethodInfo;

// Structure to store module metadata
typedef struct {
    ForeignMethodInfo* methods;
    int methodCount;
    int methodCapacity;
} ModuleMetadata;

// Global module registry to store metadata
typedef struct {
    char* moduleName;
    ModuleMetadata* metadata;
} ModuleRegistryEntry;

ModuleRegistryEntry* moduleRegistry = NULL;
int moduleRegistryCount = 0;
int moduleRegistryCapacity = 0;

// Helper function to parse extern comments
void parseExternComment(const char* source, ModuleMetadata* metadata) {
    const char* ptr = source;
    char* currentClass = NULL;
    
    // First pass: find class definitions
    while ((ptr = strstr(ptr, "class ")) != NULL) {
        ptr += 6; // skip "class "
        while (*ptr && isspace(*ptr)) ptr++;
        
        const char* classEnd = ptr;
        while (*classEnd && !isspace(*classEnd) && *classEnd != '{') classEnd++;
        
        if (classEnd > ptr) {
            free(currentClass);
            int classLen = classEnd - ptr;
            currentClass = malloc(classLen + 1);
            strncpy(currentClass, ptr, classLen);
            currentClass[classLen] = '\0';
            fprintf(stderr, "Found class: %s\n", currentClass);
        }
        
        ptr = classEnd;
    }
    
    // Second pass: find extern comments
    ptr = source;
    while ((ptr = strstr(ptr, "#!extern")) != NULL) {
        // Find the opening parenthesis
        const char* paren = strchr(ptr, '(');
        if (!paren) break;
        
        // Find the closing parenthesis
        const char* endParen = strchr(paren, ')');
        if (!endParen) break;
        
        // Extract the content inside parentheses
        int contentLen = endParen - paren - 1;
        char* content = malloc(contentLen + 1);
        strncpy(content, paren + 1, contentLen);
        content[contentLen] = '\0';
        
        fprintf(stderr, "Debug: Extern content: '%s'\n", content);
        
        // Parse dll and args
        char* dll = NULL;
        char* args = NULL;
        
        char* dllStart = strstr(content, "dll=");
        if (dllStart) {
            dllStart += 5; // skip "dll="
            if (*dllStart == '"') dllStart++; // skip opening quote
            char* dllEnd = strchr(dllStart, '"');
            if (dllEnd) {
                int dllLen = dllEnd - dllStart;
                dll = malloc(dllLen + 1);
                strncpy(dll, dllStart, dllLen);
                dll[dllLen] = '\0';
                fprintf(stderr, "Debug: Extracted dll: '%s'\n", dll);
            } else {
                fprintf(stderr, "Debug: Could not find end quote for dll\n");
            }
        } else {
            fprintf(stderr, "Debug: Could not find dll= in content\n");
        }
        
        char* argsStart = strstr(content, "args=");
        if (argsStart) {
            argsStart += 6; // skip "args="
            if (*argsStart == '"') argsStart++; // skip opening quote
            char* argsEnd = strchr(argsStart, '"');
            if (argsEnd) {
                int argsLen = argsEnd - argsStart;
                args = malloc(argsLen + 1);
                strncpy(args, argsStart, argsLen);
                args[argsLen] = '\0';
                fprintf(stderr, "Debug: Extracted args: '%s'\n", args);
            } else {
                fprintf(stderr, "Debug: Could not find end quote for args\n");
            }
        } else {
            fprintf(stderr, "Debug: Could not find args= in content\n");
        }
        
        // Extract the method signature on the next line
        const char* lineStart = endParen + 1;
        while (*lineStart && isspace(*lineStart)) lineStart++;
        
        const char* lineEnd = strchr(lineStart, '\n');
        if (!lineEnd) lineEnd = strchr(lineStart, '\0');
        
        // Extract method signature
        int sigLen = lineEnd - lineStart;
        char* signature = malloc(sigLen + 1);
        strncpy(signature, lineStart, sigLen);
        signature[sigLen] = '\0';
        
        // Trim whitespace
        char* end = signature + sigLen - 1;
        while (end > signature && isspace(*end)) end--;
        *(end + 1) = '\0';
        
        fprintf(stderr, "Debug: Found signature line: '%s'\n", signature);
        
        // Extract method name (assuming format: "foreign static methodName(...)")
        char* methodStart = strstr(signature, "foreign static ");
        char* methodName = NULL;
        
        if (methodStart) {
            methodStart += 14; // length of "foreign static "
            while (*methodStart && isspace(*methodStart)) methodStart++; // skip spaces
            
            char* parenStart = strchr(methodStart, '(');
            if (parenStart) {
                int methodLen = parenStart - methodStart;
                methodName = malloc(methodLen + 1);
                strncpy(methodName, methodStart, methodLen);
                methodName[methodLen] = '\0';
                
                // Trim trailing spaces
                char* end = methodName + methodLen - 1;
                while (end > methodName && isspace(*end)) end--;
                *(end + 1) = '\0';
            }
        }
        
        // Store the method info
        if (metadata->methodCount >= metadata->methodCapacity) {
            metadata->methodCapacity = metadata->methodCapacity == 0 ? 8 : metadata->methodCapacity * 2;
            metadata->methods = realloc(metadata->methods, metadata->methodCapacity * sizeof(ForeignMethodInfo));
        }
        
        ForeignMethodInfo* method = &metadata->methods[metadata->methodCount++];
        method->dll = dll;
        method->args = args;
        method->signature = strdup(signature);
        method->className = currentClass ? strdup(currentClass) : NULL;
        method->methodName = methodName;
        
        fprintf(stderr, "Parsed extern: %s.%s (dll=%s, args=%s)\n", 
                currentClass ? currentClass : "unknown", methodName ? methodName : "unknown", dll ? dll : "null", args ? args : "null");
        
        free(content);
        free(signature);
        ptr = endParen + 1;
    }
    
    free(currentClass);
}

// Helper function to find method metadata
ForeignMethodInfo* findMethodInfo(ModuleMetadata* metadata, const char* className, const char* signature) {
    for (int i = 0; i < metadata->methodCount; i++) {
        ForeignMethodInfo* method = &metadata->methods[i];
        if (method->className && method->methodName && 
            strcmp(method->className, className) == 0) {
            
            // Check if the signature matches (either exact method name or with parameters)
            if (strcmp(method->methodName, signature) == 0) {
                return method;
            }
            
            // Also check if signature contains the method name (for signatures with parameters)
            if (strstr(signature, method->methodName) != NULL) {
                return method;
            }
        }
    }
    return NULL;
}

// Helper function to free module metadata
void freeModuleMetadata(ModuleMetadata* metadata) {
    for (int i = 0; i < metadata->methodCount; i++) {
        ForeignMethodInfo* method = &metadata->methods[i];
        free(method->dll);
        free(method->args);
        free(method->signature);
        free(method->className);
        free(method->methodName);
    }
    free(metadata->methods);
}

// Helper function to register module metadata
void registerModuleMetadata(const char* moduleName, ModuleMetadata* metadata) {
    // Check if module already exists
    for (int i = 0; i < moduleRegistryCount; i++) {
        if (strcmp(moduleRegistry[i].moduleName, moduleName) == 0) {
            // Free existing metadata and replace
            freeModuleMetadata(moduleRegistry[i].metadata);
            free(moduleRegistry[i].metadata);
            moduleRegistry[i].metadata = metadata;
            return;
        }
    }
    
    // Add new entry
    if (moduleRegistryCount >= moduleRegistryCapacity) {
        moduleRegistryCapacity = moduleRegistryCapacity == 0 ? 8 : moduleRegistryCapacity * 2;
        moduleRegistry = realloc(moduleRegistry, moduleRegistryCapacity * sizeof(ModuleRegistryEntry));
    }
    
    moduleRegistry[moduleRegistryCount].moduleName = strdup(moduleName);
    moduleRegistry[moduleRegistryCount].metadata = metadata;
    moduleRegistryCount++;
}

// Helper function to find module metadata
ModuleMetadata* findModuleMetadata(const char* moduleName) {
    for (int i = 0; i < moduleRegistryCount; i++) {
        if (strcmp(moduleRegistry[i].moduleName, moduleName) == 0) {
            return moduleRegistry[i].metadata;
        }
    }
    return NULL;
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

void loadModuleCompleteFn(WrenVM* vm, const char* module, struct WrenLoadModuleResult result)
{
    fprintf(stderr, "Module %s loaded\n", module);
    
    // Clean up module metadata if it exists
    if (result.userData) {
        ModuleMetadata* metadata = (ModuleMetadata*)result.userData;
        freeModuleMetadata(metadata);
        free(metadata);
    }
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
            
            // Parse extern comments and store metadata
            ModuleMetadata* metadata = malloc(sizeof(ModuleMetadata));
            memset(metadata, 0, sizeof(ModuleMetadata));
            parseExternComment(source, metadata);
            
            // Register metadata globally so bindForeignMethodFn can access it
            registerModuleMetadata(name, metadata);
            
            result.onComplete = &loadModuleCompleteFn;
            result.userData = metadata;
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

void executeForeignFn(WrenVM* vm)
{
    fprintf(stderr, "Executing foreign\n");
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
    
    WrenLoadModuleResult moduleResult = loadModuleFn(vm, "raylib");

    WrenInterpretResult result = wrenInterpret(vm, "raylib", moduleResult.source);
    
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
