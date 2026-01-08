cmake_minimum_required(VERSION 3.15)
project(TurboGraphJIT VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -pipe -static-libstdc++")

# 使用本地nlohmann/json
include_directories(third_party)

# Main library
add_library(turbograph STATIC
    src/config_parser.cpp
    src/code_generator.cpp
    src/compiler.cpp
    src/loader.cpp
    src/pipeline.cpp
)

target_include_directories(turbograph PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Examples
add_executable(benchmark examples/benchmark.cpp)
target_link_libraries(benchmark PRIVATE turbograph)

add_executable(test_runner tests/test_runner.cpp)
target_link_libraries(test_runner PRIVATE turbograph)

# Installation
install(DIRECTORY include/ DESTINATION include)
install(TARGETS turbograph benchmark test_runner
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Tests (optional)
enable_testing()
add_test(NAME pipeline_test COMMAND test_runner)
