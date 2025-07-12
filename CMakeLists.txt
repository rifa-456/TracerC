cmake_minimum_required(VERSION 3.14)
project(TracerC LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(spdlog REQUIRED)
find_package(cxxopts REQUIRED)
add_executable(TracerC
        src/main.cpp
        src/Tracer.cpp
        src/SyscallMap.cpp
        include/Syscall.h
)
include_directories(${CMAKE_SOURCE_DIR}/include)
target_link_libraries(TracerC PRIVATE
        spdlog::spdlog
        cxxopts::cxxopts
)
