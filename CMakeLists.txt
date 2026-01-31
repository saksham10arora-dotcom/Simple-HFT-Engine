cmake_minimum_required(VERSION 3.10)
project(HFT_Engine)

# Use C++17 for performance
set(CMAKE_CXX_STANDARD 17)

# "Beast Mode" Flags: 
# -O3 for max speed, -march=native to use your specific CPU's power
if(MSVC)
    # Windows (MSVC) Speed Flags
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Oi /Ot /GL")
else()
    # Linux/Mac (GCC/Clang) Speed Flags
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto")
endif()
# Tell CMake where to find your .hpp files
include_directories(include)

# Tell CMake to build the engine using your main.cpp
add_executable(hft_engine src/main.cpp)