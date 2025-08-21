# C++ Orderbook
Comprehensive HFT orderbook built in C++ 

## Project Structure
```
Orderbook-1/
├── CMakeLists.txt          # CMake build configuration
├── main.cpp                # Main program entry point
├── include/                # Header files
│   ├── Order.hpp          # Order struct and OrderSide enum
│   ├── OrderManager.hpp   # Order management interface
│   ├── MatchingEngine.hpp # Order matching engine interface
│   └── AtomicRingBuffer.hpp # Lock-free ring buffer interface
└── src/                   # Source files
    ├── OrderManager.cpp   # OrderManager implementation
    ├── MatchingEngine.cpp # MatchingEngine implementation
    └── AtomicRingBuffer.cpp # AtomicRingBuffer implementation
```

## Quick Start - Build and Run

### Prerequisites
- CMake 3.14 or higher
- C++17 compatible compiler (Visual Studio 2019+ on Windows, GCC 7+ on Linux)

## Just copy and paste this

```bash
cd /mnt/c/Users/vkotr/Orderbook-1/build
make -j4
./main
```

### Windows (Visual Studio)
```bash
# Navigate to project directory
cd Orderbook-1

# Create and enter build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build the project
cmake --build . --config Release

# Run the executable
.\Release\main.exe
```

### Linux/macOS (GCC/Clang)
```bash
# Navigate to project directory
cd Orderbook-1

# Create and enter build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build the project
make

# Run the executable
./main
```

## Development Workflow

### After Making Changes
```bash
# From the build directory, rebuild:
cmake --build . --config Release

# Run again:
.\Release\main.exe  # Windows
./main               # Linux/macOS
```

### Clean Rebuild
```bash
# Delete build directory and start fresh
cd ..
rm -rf build        # Linux/macOS
# OR
rmdir /s build      # Windows

# Then follow Quick Start steps again
```

## Current Status
- ✅ Project builds successfully
- ✅ Basic structure in place
- ❌ Implementation files need to be completed
- ❌ Core orderbook logic to be implemented

## Next Steps for Implementation
1. Implement `OrderManager` class for order storage and management
2. Implement `MatchingEngine` class for order matching logic
3. Implement `AtomicRingBuffer` class for high-performance data structures
4. Add comprehensive testing
5. Optimize for high-frequency trading performance

## Notes
- The project currently compiles with empty implementation files
- All TODO comments indicate where actual logic should be implemented
- Use C++17 features for modern, efficient code
- Focus on performance and thread safety for HFT applications 
