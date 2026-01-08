#!/bin/bash
# 编译脚本 - 直接使用g++编译项目

set -e

PROJECT_DIR="/workspace/turbograph_jit"
BUILD_DIR="$PROJECT_DIR/build"

echo "创建构建目录..."
mkdir -p "$BUILD_DIR"

echo "编译项目..."

# 编译测试程序
g++ -std=c++17 -O3 -march=native -I"$PROJECT_DIR/include" -I"$PROJECT_DIR/third_party" \
    "$PROJECT_DIR/tests/test_runner.cpp" \
    "$PROJECT_DIR/src/config_parser.cpp" \
    "$PROJECT_DIR/src/code_generator.cpp" \
    "$PROJECT_DIR/src/compiler.cpp" \
    "$PROJECT_DIR/src/loader.cpp" \
    "$PROJECT_DIR/src/pipeline.cpp" \
    -o "$BUILD_DIR/test_runner" \
    -ldl

echo "编译测试程序成功!"
echo ""
echo "运行测试..."
"$BUILD_DIR/test_runner"

echo ""
echo "编译性能测试程序..."
g++ -std=c++17 -O3 -march=native -I"$PROJECT_DIR/include" -I"$PROJECT_DIR/third_party" \
    "$PROJECT_DIR/examples/benchmark.cpp" \
    "$PROJECT_DIR/src/config_parser.cpp" \
    "$PROJECT_DIR/src/code_generator.cpp" \
    "$PROJECT_DIR/src/compiler.cpp" \
    "$PROJECT_DIR/src/loader.cpp" \
    "$PROJECT_DIR/src/pipeline.cpp" \
    -o "$BUILD_DIR/benchmark" \
    -ldl

echo "编译性能测试程序成功!"
echo ""
echo "运行性能测试..."
"$BUILD_DIR/benchmark"

echo ""
echo "完成!"
