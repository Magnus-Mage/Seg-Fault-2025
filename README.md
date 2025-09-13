# FORTH-ESP32 Compiler

A minimal FORTH compiler that generates ESP-IDF compatible C++ code for ESP32 microcontrollers.

##  Current Status: Phase 4 - Semantic Analysis & C Code Generation

###  Features Implemented
- [x] **Project structure** - Complete directory layout and build system
- [x] **Lexical analyzer** - Tokenization of FORTH source code  
- [x] **Parser** - AST generation from token stream
- [x] **Dictionary system** - FORTH word definitions and lookups
- [x] **Semantic analyzer** - Stack effect analysis and error checking
- [x] **C code generator** - Generates ESP-IDF compatible C code
- [ ] **ESP32 optimization** - Hardware-specific optimizations
- [ ] **Advanced features** - Interrupts, peripherals, etc.

###  Architecture Overview

```
FORTH Source (.fth/ .forth)
    ↓
Lexical Analysis (Tokenizer)
    ↓
Syntax Analysis (Parser → AST)
    ↓
Semantic Analysis (Stack effects, type checking)
    ↓
C Code Generation (ESP-IDF compatible)
    ↓
ESP-IDF Project (Ready for compilation)
    ↓
ESP32 Binary (Flash to hardware)
```

##  Quick Start

### Prerequisites

- **CMake** >= 3.16
- **C++20 compatible compiler** (GCC 10+, Clang 12+, MSVC 2019+)
- **ESP-IDF** (optional, for ESP32 deployment)

### Building

```bash
# Clone and build
git clone <your-repo>
cd forth_esp32_compiler

# Quick build
./build_and_test.sh

# Manual build
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Basic Usage

```bash
# Compile FORTH program to C code
./build/forth_compiler examples/hello.fth --output generated

# Create complete ESP-IDF project
./build/forth_compiler examples/blink.fth --create-esp32 --output blink_project

# Detailed analysis
./build/forth_compiler examples/factorial.fth -v --show-code --stats
```

##  Command Line Options

### Basic Options

| Option | Short | Description | Example |
|--------|-------|-------------|---------|
| `--help` | `-h` | Show help message | `forth_compiler -h` |
| `--version` | | Show version information | `forth_compiler --version` |
| `--output` | `-o` | Output file/directory | `-o generated_code` |

### Analysis Options

| Option | Short | Description | Details |
|--------|-------|-------------|---------|
| `--verbose` | `-v` | Enable all detailed output | Shows tokens, AST, semantic analysis, and code generation |
| `--tokens` | `-t` | Show tokenization results | Displays token type, value, line, and column |
| `--ast` | `-a` | Show Abstract Syntax Tree | Hierarchical view of parsed FORTH program |
| `--semantic` | `-s` | Show semantic analysis | Stack effects, type checking, variable usage |
| `--codegen` | `-c` | Show code generation details | Statistics and generation process |
| `--show-code` | | Display generated C code | Shows both header and source code |
| `--dict` | `-d` | Show dictionary contents | Lists all defined FORTH words |
| `--stats` | | Show performance statistics | Timing breakdown and processing rates |

### Code Generation Options

| Option | Description | Default | Example |
|--------|-------------|---------|---------|
| `--target` | Target ESP32 variant | `esp32` | `--target esp32c3` |
| `--create-esp32` | Create ESP-IDF project | - | `--create-esp32` |
| `--optimize` | Enable optimizations | `ON` | `--optimize=OFF` |

### Supported Targets

- `esp32` - Original ESP32 (default)
- `esp32c3` - ESP32-C3 RISC-V variant
- `esp32s3` - ESP32-S3 with additional features

##  Usage Examples

### 1. Basic Compilation

```bash
# Create a simple FORTH program
echo ': HELLO ." Hello, World!" CR ;' > hello.fth

# Compile to C code
./forth_compiler hello.fth -o hello_generated

# This creates:
# - hello_generated.h (header with declarations)
# - hello_generated.c (source with implementation)
```

### 2. ESP32 Project Creation

```bash
# Create ESP32 blinker
cat > blink.fth << EOF
: BLINK
  13 OUTPUT          \ Set GPIO 13 as output
  1000 0 DO          \ Loop 1000 times
    13 HIGH          \ Turn LED on
    500 DELAY        \ Wait 500ms
    13 LOW           \ Turn LED off  
    500 DELAY        \ Wait 500ms
  LOOP ;
BLINK                \ Start blinking
EOF

# Generate ESP-IDF project
./forth_compiler blink.fth --create-esp32 --output blink_project

# Build and flash
cd blink_project
idf.py set-target esp32
idf.py build
idf.py flash monitor
```

### 3. Advanced Analysis

```bash
# Comprehensive analysis
./forth_compiler complex_program.fth \
  --verbose \
  --show-code \
  --stats \
  --output analysis_results

# This shows:
# - Tokenization breakdown
# - AST structure  
# - Semantic analysis results
# - Generated C code
# - Performance statistics
# - Dictionary contents
```

### 4. Development and Debugging

```bash
# Check syntax only
./forth_compiler program.fth --tokens --ast

# Semantic analysis only
./forth_compiler program.fth --semantic --dict

# Code generation testing
./forth_compiler program.fth --codegen --show-code --target esp32c3
```

##  Project Structure

```
forth_esp32_compiler/
├── CMakeLists.txt              # Main build configuration
├── build_and_test.sh           # Quick build and test script
├── README.md                   # This file
├── src/                        # Source code
│   ├── main.cpp               # Main application entry point
│   ├── lexer/                 # Lexical analysis
│   │   ├── lexer.h
│   │   └── lexer.cpp
│   ├── parser/                # Syntax analysis
│   │   ├── parser.h
│   │   ├── parser.cpp
│   │   ├── ast.h
│   │   └── ast.cpp
│   ├── dictionary/            # FORTH dictionary system
│   │   ├── dictionary.h
│   │   └── dictionary.cpp
│   ├── semantic/              # Semantic analysis
│   │   ├── analyzer.h
│   │   └── analyzer.cpp
│   ├── codegen/               # Code generation
│   │   ├── c_backend.h
│   │   └── c_backend.cpp
│   └── common/                # Utilities
│       └── utils.h
├── tests/                     # Test suite
│   ├── CMakeLists.txt
│   ├── test_main.cpp
│   ├── lexer/
│   ├── parser/
│   ├── semantic/
│   └── codegen/
├── examples/                  # Example FORTH programs
│   ├── hello.fth
│   ├── factorial.fth
│   ├── blink.fth
│   └── temp_monitor.fth
└── build/                     # Build artifacts (created during build)
```

##  Testing

### Running Tests

```bash
# Build and run all tests
./build_and_test.sh

# Manual testing
cd build
make test_forth_compiler
./test_forth_compiler

# Specific test categories
ctest --output-on-failure -R lexer    # Lexer tests only
ctest --output-on-failure -R parser   # Parser tests only
ctest --output-on-failure -R semantic # Semantic tests only
ctest --output-on-failure -R codegen  # Code generation tests only
```

### Test Coverage

- **Lexer Tests**: Token recognition, error handling, edge cases
- **Parser Tests**: AST generation, syntax validation, recovery
- **Semantic Tests**: Stack analysis, variable checking, word resolution
- **Codegen Tests**: C code generation, ESP-IDF integration, optimization

### Integration Testing

```bash
# Test complete pipeline
make test_codegen

# Test ESP32 project generation
make create_esp32_project
cd esp32_project && idf.py build

# Performance benchmarking  
make benchmark
```

## 🎛 Development Targets

The CMake build system provides several useful targets:

```bash
# Core targets
make forth_compiler          # Build main compiler
make test_forth_compiler     # Build and run tests
make clean_build            # Clean rebuild

# Code generation testing
make test_codegen           # Test C code generation
make generate_examples      # Create example FORTH files

# ESP32 integration
make create_esp32_project   # Generate ESP32 template
make test_esp32_build      # Test ESP32 build process

# Documentation and analysis
make docs                   # Generate documentation (requires Doxygen)
make cppcheck              # Static analysis (requires cppcheck)
make benchmark             # Performance benchmarking
```

##  FORTH Language Support

### Implemented Features

#### Stack Operations
- `DUP` `DROP` `SWAP` `ROT` `OVER` - Basic stack manipulation
- `DEPTH` `CLEAR` - Stack inspection and management

#### Arithmetic
- `+` `-` `*` `/` `MOD` - Basic arithmetic
- `ABS` `NEGATE` `MIN` `MAX` - Extended arithmetic

#### Comparison
- `=` `<>` `<` `>` `<=` `>=` - Comparisons
- `0=` `0<>` `0<` `0>` - Zero comparisons

#### Logic
- `AND` `OR` `XOR` `NOT` - Bitwise operations
- `TRUE` `FALSE` - Boolean constants

#### Control Flow
- `IF ... ELSE ... THEN` - Conditional execution
- `BEGIN ... UNTIL` - Loops with condition
- `DO ... LOOP` `DO ... +LOOP` - Counted loops

#### Words and Variables
- `: word ... ;` - Word definition
- `VARIABLE name` - Variable declaration
- `CONSTANT name` - Constant definition
- `@` `!` - Memory access

#### I/O and Strings
- `." text"` - Print string literal
- `.` - Print number
- `CR` - Carriage return
- `EMIT` `KEY` - Character I/O

#### ESP32 Specific
- `OUTPUT` `INPUT` `PULLUP` - GPIO configuration
- `HIGH` `LOW` `TOGGLE` - GPIO control
- `DELAY` - Millisecond delays
- `ADC-READ` - Analog input
- `PWM-SETUP` `PWM-WRITE` - PWM output

### Upcoming Features
- Floating point arithmetic
- String manipulation
- File I/O operations
- Interrupt handling
- WiFi and Bluetooth integration
- Sensor libraries

##  Generated Code Structure

The C backend generates clean, readable code:

```c
// Generated header (program.h)
#include "forth_runtime.h"

void forth_main(void);
void word_HELLO(void);

// Generated source (program.c)  
#include "program.h"

void word_HELLO(void) {
    forth_print("Hello, World!");
    forth_cr();
}

void forth_main(void) {
    forth_init();
    word_HELLO();
}
```

##  Known Limitations

### Current Limitations
- Limited floating-point support
- No dynamic memory allocation
- Basic error reporting in generated code
- Limited optimization passes

### Planned Improvements
- Advanced stack optimization
- Compile-time constant folding  
- Dead code elimination
- Better error messages with line numbers

### Development Setup
```bash
# Enable development mode
cmake -DENABLE_DEBUG=ON -DBUILD_TESTS=ON ..

# Enable all warnings and sanitizers
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

### Code Style
- C++20 standard
- 4-space indentation
- Descriptive variable names
- Comprehensive error handling

### Testing Guidelines
- Add tests for new features
- Maintain test coverage above 80%
- Test both success and failure cases
- Include integration tests

##  License

This project is open source. See LICENSE file for details.

##  Support

### Common Issues

**Build Errors:**
```bash
# Missing C++20 support
sudo apt update && sudo apt install gcc-10 g++-10

# CMake too old
# Install CMake 3.16+ from cmake.org
```

**ESP-IDF Integration:**
```bash
# Install ESP-IDF
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh && . ./export.sh
```

**Memory Issues:**
- Reduce stack size in generated code
- Use `--optimize` flag for smaller binaries
- Check FORTH program complexity

### Getting Help

1. Check the examples/ directory for usage patterns
2. Run with `--verbose` for detailed analysis
3. Use `--help` for command-line options
4. Check the test suite for expected behavior

### Reporting Issues

Include in bug reports:
- FORTH source code (if possible)
- Command line used
- Full error output
- System information (OS, compiler version)
- CMake and build logs

---

**Version:** 0.5.0  
**Last Updated:** Phase 4 - Semantic Analysis & C Code Generation  
**Next :** ESP32 Hardware Integration
