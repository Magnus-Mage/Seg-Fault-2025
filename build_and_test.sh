#!/bin/bash

echo "🚀 FORTH-ESP32 Compiler - Phase 3 Build & Test"
echo "=============================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print status
print_status() {
    echo -e "${GREEN}✓${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

# Step 1: Clean previous build
echo "Step 1: Cleaning previous build..."
rm -rf build/
print_status "Build directory cleaned"

# Step 2: Create build directory and configure
echo ""
echo "Step 2: Configuring build..."
mkdir build
cd build

if cmake .. ; then
    print_status "CMake configuration successful"
else
    print_error "CMake configuration failed!"
    exit 1
fi

# Step 3: Build the project
echo ""
echo "Step 3: Building project..."
if make -j$(nproc) ; then
    print_status "Build successful"
else
    print_error "Build failed!"
    exit 1
fi

# Step 4: Run tests
echo ""
echo "Step 4: Running tests..."
if ./test_forth_compiler ; then
    print_status "All tests passed!"
else
    print_warning "Some tests failed - check output above"
fi

# Step 5: Test with example file
echo ""
echo "Step 5: Testing with example FORTH program..."
cd ..

if [ -f "examples/basic_test.forth" ]; then
    echo "Testing basic_test.forth..."
    if ./build/forth_compiler examples/basic_test.forth -v ; then
        print_status "Example program processed successfully"
    else
        print_warning "Example program had issues - check output above"
    fi
else
    print_warning "Example file not found - skipping example test"
fi

echo ""
echo "=============================================="
echo "🎉 Phase 3 build and test complete!"
echo ""
echo "Next steps:"
echo "• Review any test failures or warnings above"
echo "• If all tests pass, you're ready for Phase 4"
echo "• Run './build/forth_compiler --help' for usage info"
