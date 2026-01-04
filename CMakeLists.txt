cmake_minimum_required(VERSION 3.10)
project(wreni C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Allow user to specify WREN_DIR, default to original path if not provided
set(WREN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/wren" CACHE PATH "Path to Wren source directory")

# Check if WREN_DIR exists
if(NOT EXISTS "${WREN_DIR}")
    message(FATAL_ERROR "WREN directory \"${WREN_DIR}\" does not exist. Please specify a valid WREN_DIR using: cmake -DWREN_DIR=/path/to/wren")
endif()

# Set build directories
set(BUILD_DIR "${CMAKE_BINARY_DIR}/build")

# Wren source directories
set(WREN_INC "${WREN_DIR}/src/include")
set(WREN_OPT_DIR "${WREN_DIR}/src/optional")
set(WREN_SRC_DIR "${WREN_DIR}/src/vm")

# Collect Wren source files
file(GLOB WREN_VM_SOURCES "${WREN_SRC_DIR}/*.c")
file(GLOB WREN_OPT_SOURCES "${WREN_OPT_DIR}/*.c")
set(WREN_SOURCES ${WREN_VM_SOURCES} ${WREN_OPT_SOURCES})

# Create Wren static library
add_library(wren STATIC ${WREN_SOURCES})

# Set include directories for Wren library
target_include_directories(wren PRIVATE
    ${WREN_INC}
    ${WREN_SRC_DIR}
    ${WREN_OPT_DIR}
)

# Add compile definitions for Wren
target_compile_definitions(wren PRIVATE DWREN_OPT_META)

# Create main executable
add_executable(wreni main.c)

# Link libraries
target_link_libraries(wreni 
    wren
    ${CMAKE_DL_LIBS}
    ffi
    m
)

# Set include directories for main executable
target_include_directories(wreni PRIVATE
    ${WREN_INC}
    ${WREN_SRC_DIR}
    ${WREN_OPT_DIR}
)

# Set output directory
set_target_properties(wreni PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR}
)

# Custom target to clean build directory
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${BUILD_DIR}
    COMMENT "Removing build directory"
)

# Install target (optional)
install(TARGETS wreni
    RUNTIME DESTINATION bin
)
