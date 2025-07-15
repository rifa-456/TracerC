cmake_minimum_required(VERSION 3.14)
project(TracerC LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(spdlog REQUIRED)
find_package(cxxopts REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
add_executable(TracerC
        src/main.cpp
        src/Tracer.cpp
        src/SyscallMap.cpp
)
if (UNIX)
    target_link_options(TracerC PRIVATE "-rdynamic")
endif ()
target_include_directories(TracerC PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(TracerC PRIVATE
        spdlog::spdlog
        cxxopts::cxxopts
)
