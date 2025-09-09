cmake_minimum_required(VERSION 3.16)

project(forth_esp32_compiler
    VERSION 0.2.0
    DESCRIPTION "FORTH Compiler targeting ESP32 with LLVM backend"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build configuration
option(BUILD_TESTS "Build test suite" ON)
option(WITH_LLVM "Enable LLVM backend (requires LLVM installation)" OFF)
option(ENABLE_DEBUG "Enable debug output" OFF)

# Source files
set(SOURCES
    src/main.cpp
    src/lexer/lexer.cpp
    src/parser/ast.cpp
    src/parser/parser.cpp
    src/dictionary/dictionary.cpp
    src/semantic/analyzer.cpp
    src/codegen/llvm_backend.cpp
)

# Include directories
include_directories(src)

# Main executable
add_executable(forth_compiler ${SOURCES})

target_compile_features(forth_compiler PRIVATE cxx_std_20)

target_compile_options(forth_compiler PRIVATE
    -Wall -Wextra -Wpedantic -Wno-unused-parameter
    $<$<CONFIG:Debug>:-g -O0 -DDEBUG -fsanitize=address>
    $<$<CONFIG:Release>:-O3 -DNDEBUG -DNDEBUG_STACK_CHECKS>
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_options(forth_compiler PRIVATE -fsanitize=address)
endif()

if(ENABLE_DEBUG)
    target_compile_definitions(forth_compiler PRIVATE ENABLE_DEBUG_OUTPUT)
endif()

# Math library
target_link_libraries(forth_compiler PRIVATE m)

# === LLVM Integration ===
if(WITH_LLVM)
    find_package(LLVM REQUIRED CONFIG)

    message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
    message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
    message(STATUS "LLVM Include Dirs: ${LLVM_INCLUDE_DIRS}")
    message(STATUS "LLVM Library Dirs: ${LLVM_LIBRARY_DIRS}")

    # Separate LLVM definitions to handle potential issues
    separate_arguments(LLVM_DEFINITIONS_LIST UNIX_COMMAND "${LLVM_DEFINITIONS}")
    
    include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

    target_include_directories(forth_compiler PRIVATE ${LLVM_INCLUDE_DIRS})
    target_link_directories(forth_compiler PRIVATE ${LLVM_LIBRARY_DIRS})
    target_compile_definitions(forth_compiler PRIVATE ${LLVM_DEFINITIONS_LIST})
    target_compile_definitions(forth_compiler PRIVATE WITH_REAL_LLVM)

    # LLVM 15 compatible component mapping
    if(COMMAND llvm_map_components_to_libnames)
        # Updated components for LLVM 15
        llvm_map_components_to_libnames(llvm_libs
            # Core components
            support 
            core 
            irreader 
            bitwriter 
            analysis
            
            # Target and code generation
            target 
            transformutils 
            codegen 
            
            # Machine code components (important for LLVM 15)
            mc 
            mcparser
            mcdisassembler
            
            # Optimization passes
            passes 
            
            # Platform-specific codegen
	    # nativecodegen
            
            # Additional useful components
            ipo
            instcombine
            scalaropts
            vectorize
            objcarcopts
	    bitreader

	    # x86 related passes
	    x86codegen
	    x86asmprinter
  	    x86asmparser


        )
        
        # Link LLVM libraries
        target_link_libraries(forth_compiler PRIVATE ${llvm_libs})
        
        message(STATUS "LLVM libraries: ${llvm_libs}")
    else()
        message(FATAL_ERROR "llvm_map_components_to_libnames() is not available. Please check LLVM installation.")
    endif()
    
    # Additional LLVM 15 specific settings
    if(LLVM_PACKAGE_VERSION VERSION_GREATER_EQUAL "15.0")
        message(STATUS "Configuring for LLVM 15+")
        
        # LLVM 15+ may require additional preprocessor definitions
        target_compile_definitions(forth_compiler PRIVATE 
            LLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING=1
        )
        
        # Ensure C++17 or higher (LLVM 15 requirement)
        if(CMAKE_CXX_STANDARD LESS 17)
            set_property(TARGET forth_compiler PROPERTY CXX_STANDARD 17)
            message(STATUS "Updated C++ standard to C++17 for LLVM 15 compatibility")
        endif()
    endif()
    
    # Handle potential threading requirements
    find_package(Threads REQUIRED)
    target_link_libraries(forth_compiler PRIVATE Threads::Threads)
    
    # Handle potential zlib dependency
    find_package(ZLIB)
    if(ZLIB_FOUND)
        target_link_libraries(forth_compiler PRIVATE ${ZLIB_LIBRARIES})
    endif()
    
    # Handle potential terminfo/ncurses dependency on Linux
    if(UNIX AND NOT APPLE)
        find_library(TERMINFO_LIB tinfo)
        if(TERMINFO_LIB)
            target_link_libraries(forth_compiler PRIVATE ${TERMINFO_LIB})
        else()
            find_library(NCURSES_LIB ncurses)
            if(NCURSES_LIB)
                target_link_libraries(forth_compiler PRIVATE ${NCURSES_LIB})
            endif()
        endif()
    endif()
    
    # Add dl library on Unix systems (for dynamic loading)
    if(UNIX)
        target_link_libraries(forth_compiler PRIVATE ${CMAKE_DL_LIBS})
    endif()
    
endif()

# === Testing ===
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
    message(STATUS "Test suite enabled")
endif()

# === Installation ===
install(TARGETS forth_compiler
    RUNTIME DESTINATION bin
    COMPONENT runtime)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    install(FILES README.md
        DESTINATION share/doc/forth_compiler
        COMPONENT documentation)
endif()

install(DIRECTORY examples/
    DESTINATION share/forth_compiler/examples
    COMPONENT examples
    PATTERN "*.fth"
    PATTERN "README*")

# === Summary ===
message(STATUS "=== FORTH ESP32 Compiler Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Tests: ${BUILD_TESTS}")
message(STATUS "LLVM Backend: ${WITH_LLVM}")
if(WITH_LLVM)
    message(STATUS "LLVM Version: ${LLVM_PACKAGE_VERSION}")
    message(STATUS "LLVM Definitions: ${LLVM_DEFINITIONS}")
endif()
message(STATUS "Debug Output: ${ENABLE_DEBUG}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "===========================================")

# === Development Targets ===
add_custom_target(run_tests
    COMMAND ctest --output-on-failure
    DEPENDS forth_compiler
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running test suite")

add_custom_target(clean_build
    COMMAND ${CMAKE_COMMAND} --build . --target clean
    COMMAND ${CMAKE_COMMAND} --build .
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Clean rebuild")

add_custom_target(generate_examples
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/examples
    COMMAND ${CMAKE_COMMAND} -E echo ": HELLO .\\\" Hello, World!\\\" CR ;" > ${CMAKE_BINARY_DIR}/examples/hello.fth
    COMMAND ${CMAKE_COMMAND} -E echo ": FACTORIAL DUP 1 <= IF DROP 1 ELSE DUP 1- FACTORIAL * THEN ;" > ${CMAKE_BINARY_DIR}/examples/factorial.fth
    COMMENT "Generating example FORTH files")

add_dependencies(forth_compiler generate_examples)

# === LLVM Debug Information ===
if(WITH_LLVM AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Add debug target to show LLVM configuration
    add_custom_target(llvm_debug_info
        COMMAND ${CMAKE_COMMAND} -E echo "LLVM Configuration Debug Info:"
        COMMAND ${CMAKE_COMMAND} -E echo "LLVM_VERSION: ${LLVM_PACKAGE_VERSION}"
        COMMAND ${CMAKE_COMMAND} -E echo "LLVM_INCLUDE_DIRS: ${LLVM_INCLUDE_DIRS}"
        COMMAND ${CMAKE_COMMAND} -E echo "LLVM_LIBRARY_DIRS: ${LLVM_LIBRARY_DIRS}"
        COMMAND ${CMAKE_COMMAND} -E echo "LLVM_DEFINITIONS: ${LLVM_DEFINITIONS}"
        COMMAND ${CMAKE_COMMAND} -E echo "LLVM_LIBS: ${llvm_libs}"
        COMMENT "Display LLVM configuration information")
endif()
