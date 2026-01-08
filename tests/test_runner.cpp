/**
 * @file test_runner.cpp
 * @brief 单元测试程序
 * 
 * 测试TurboGraph-JIT的核心功能：
 * 1. 配置解析
 * 2. 代码生成
 * 3. 动态编译
 * 4. SO加载
 * 5. 管道执行
 */

#include "pipeline.hpp"
#include "config.hpp"
#include "code_generator.hpp"
#include "compiler.hpp"
#include "loader.hpp"
#include "ops.hpp"

#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>

using namespace turbograph;

// ============================================
// 测试工具
// ============================================

int tests_passed = 0;
int tests_failed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Running " << #name << "... "; \
    try { \
        test_##name(); \
        std::cout << "PASSED\n"; \
        tests_passed++; \
    } catch (const std::exception& e) { \
        std::cout << "FAILED: " << e.what() << "\n"; \
        tests_failed++; \
    } \
} while(0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) throw std::runtime_error("Assertion failed: " #cond); \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) throw std::runtime_error("Assertion failed: " #a " == " #b); \
} while(0)

#define ASSERT_DOUBLE_EQ(a, b, eps) do { \
    if (std::abs((a) - (b)) > (eps)) throw std::runtime_error("Assertion failed: " #a " ~= " #b); \
} while(0)

// ============================================
// 测试1: 类型系统
// ============================================

TEST(type_system) {
    // 测试类型转换
    ASSERT_EQ(data_type_to_string(DataType::DOUBLE), std::string("double"));
    ASSERT_EQ(data_type_to_string(DataType::INT32), std::string("int32"));
    ASSERT_EQ(data_type_to_string(DataType::STRING), std::string("string"));
    
    // 测试字符串到类型
    ASSERT_EQ(string_to_data_type("double"), DataType::DOUBLE);
    ASSERT_EQ(string_to_data_type("int32"), DataType::INT32);
    ASSERT_EQ(string_to_data_type("int64_list"), DataType::INT64_LIST);
    
    // 测试列表类型判断
    ASSERT_TRUE(is_list_type(DataType::DOUBLE_LIST));
    ASSERT_TRUE(is_list_type(DataType::INT32_LIST));
    ASSERT_TRUE(!is_list_type(DataType::DOUBLE));
    
    // 测试C++类型名称
    ASSERT_EQ(get_cpp_type_name(DataType::DOUBLE), std::string("double"));
    ASSERT_EQ(get_cpp_type_name(DataType::INT64), std::string("int64_t"));
    ASSERT_EQ(get_cpp_type_name(DataType::STRING), std::string("std::string"));
    
    std::cout << "All type system tests passed! ";
}

// ============================================
// 测试2: 配置解析
// ============================================

TEST(config_parsing) {
    // 创建JSON配置
    std::string json_config = R"({
        "name": "test_pipeline",
        "inputs": [
            {"name": "a", "type": "double", "required": true},
            {"name": "b", "type": "int32", "required": true}
        ],
        "variables": [
            {"name": "temp", "type": "double", "required": false}
        ],
        "steps": [
            {
                "op": "add",
                "args": ["$a", "$b"],
                "output": "temp"
            }
        ],
        "outputs": [
            {"name": "temp", "type": "double", "required": true}
        ]
    })";
    
    // 解析配置
    JsonConfigParser parser;
    auto config = parser.parse_string(json_config);
    
    // 验证解析结果
    ASSERT_EQ(config.name, std::string("test_pipeline"));
    ASSERT_EQ(config.inputs.size(), size_t(2));
    ASSERT_EQ(config.variables.size(), size_t(1));
    ASSERT_EQ(config.steps.size(), size_t(1));
    ASSERT_EQ(config.outputs.size(), size_t(1));
    
    // 验证输入
    ASSERT_EQ(config.inputs[0].name, std::string("a"));
    ASSERT_EQ(config.inputs[0].type, DataType::DOUBLE);
    ASSERT_EQ(config.inputs[1].name, std::string("b"));
    ASSERT_EQ(config.inputs[1].type, DataType::INT32);
    
    // 验证步骤
    ASSERT_EQ(config.steps[0].op_name, std::string("add"));
    ASSERT_EQ(config.steps[0].output_var, std::string("temp"));
    ASSERT_EQ(config.steps[0].args.size(), size_t(2));
    ASSERT_EQ(config.steps[0].args[0].value, std::string("a"));
    ASSERT_EQ(config.steps[0].args[1].value, std::string("b"));
    
    // 验证指纹
    ASSERT_TRUE(!config.fingerprint.empty());
    
    std::cout << "All config parsing tests passed! ";
}

// ============================================
// 测试3: 配置验证
// ============================================

TEST(config_validation) {
    JsonConfigParser parser;
    
    // 有效配置
    std::string valid_config = R"({
        "name": "valid_pipeline",
        "inputs": [{"name": "x", "type": "double", "required": true}],
        "variables": [{"name": "y", "type": "double", "required": false}],
        "steps": [{"op": "add", "args": ["$x", "1"], "output": "y"}],
        "outputs": [{"name": "y", "type": "double", "required": true}]
    })";
    
    auto config = parser.parse_string(valid_config);
    ASSERT_TRUE(parser.validate(config));
    
    // 无效配置 - 空名称
    std::string invalid_name = R"({
        "name": "",
        "inputs": [],
        "variables": [],
        "steps": [],
        "outputs": []
    })";
    auto invalid = parser.parse_string(invalid_name);
    ASSERT_TRUE(!parser.validate(invalid));
    
    std::cout << "All config validation tests passed! ";
}

// ============================================
// 测试4: 代码生成
// ============================================

TEST(code_generation) {
    PipelineConfig config;
    config.name = "test_gen";
    config.inputs = {{"a", DataType::DOUBLE, true}, {"b", DataType::DOUBLE, true}};
    config.variables = {{"c", DataType::DOUBLE, false}};
    config.steps = {
        OpCallBuilder("add")
            .output("c")
            .args({Arg::variable("a", DataType::DOUBLE), Arg::variable("b", DataType::DOUBLE)})
            .build()
    };
    config.outputs = {{"c", DataType::DOUBLE, false}};
    config.compute_fingerprint();
    
    CodeGenerator generator(config);
    std::string code = generator.generate();
    
    // 验证代码包含关键元素
    ASSERT_TRUE(code.find("test_gen") != std::string::npos);
    ASSERT_TRUE(code.find("#include") != std::string::npos);
    ASSERT_TRUE(code.find("ops.hpp") != std::string::npos);
    ASSERT_TRUE(code.find("pipeline_execute") != std::string::npos);
    ASSERT_TRUE(code.find("ctx.a") != std::string::npos);
    ASSERT_TRUE(code.find("ctx.b") != std::string::npos);
    ASSERT_TRUE(code.find("ctx.c") != std::string::npos);
    
    std::cout << "All code generation tests passed! ";
}

// ============================================
// 测试5: 解释执行
// ============================================

TEST(interpreter_execution) {
    auto config = create_demo_config();
    
    auto executor = PipelineManager::instance().create(config, PipelineMode::INTERPRETER);
    
    ExecutionContext ctx;
    ctx.set_variable("price_a", DataType::DOUBLE, 100.0);
    ctx.set_variable("price_b", DataType::DOUBLE, 50.0);
    ctx.set_variable("volume", DataType::INT32, 10);
    
    bool success = executor->execute(ctx);
    ASSERT_TRUE(success);
    
    double result = ctx.get<double>("final_score");
    // (100 + 50) * 10 / 100 = 15.0
    ASSERT_DOUBLE_EQ(result, 15.0, 0.001);
    
    std::cout << "All interpreter execution tests passed! ";
}

// ============================================
// 测试6: 上下文管理
// ============================================

TEST(context_management) {
    ExecutionContext ctx;
    
    // 设置变量
    ctx.set_variable("x", DataType::DOUBLE, 10.0);
    ctx.set_variable("y", DataType::INT32, 5);
    ctx.set_variable("s", DataType::STRING, std::string("hello"));
    
    // 获取变量
    ASSERT_DOUBLE_EQ(ctx.get<double>("x"), 10.0, 0.001);
    ASSERT_EQ(ctx.get<int32_t>("y"), 5);
    ASSERT_EQ(ctx.get<std::string>("s"), std::string("hello"));
    
    // 检查变量存在
    ASSERT_TRUE(ctx.has_variable("x"));
    ASSERT_TRUE(ctx.has_variable("y"));
    ASSERT_TRUE(ctx.has_variable("s"));
    ASSERT_TRUE(!ctx.has_variable("z"));
    
    // 清空
    ctx.clear();
    ASSERT_TRUE(!ctx.has_variable("x"));
    
    std::cout << "All context management tests passed! ";
}

// ============================================
// 测试7: 算子测试
// ============================================

TEST(operators) {
    // 测试基本算子
    ASSERT_DOUBLE_EQ(ops::add_op<double>(3.0, 4.0), 7.0, 0.001);
    ASSERT_DOUBLE_EQ(ops::sub_op<double>(10.0, 4.0), 6.0, 0.001);
    ASSERT_DOUBLE_EQ(ops::mul_op<double>(3.0, 4.0), 12.0, 0.001);
    ASSERT_DOUBLE_EQ(ops::div_op<double>(12.0, 4.0), 3.0, 0.001);
    
    // 测试条件算子
    ASSERT_DOUBLE_EQ(ops::if_else<double>(true, 1.0, 0.0), 1.0, 0.001);
    ASSERT_DOUBLE_EQ(ops::if_else<double>(false, 1.0, 0.0), 0.0, 0.001);
    
    // 测试数学算子
    ASSERT_DOUBLE_EQ(ops::abs_op<double>(-5.0), 5.0, 0.001);
    ASSERT_DOUBLE_EQ(ops::square_op<double>(3.0), 9.0, 0.001);
    ASSERT_DOUBLE_EQ(ops::sqrt_op<double>(9.0), 3.0, 0.001);
    
    // 测试极值算子
    ASSERT_DOUBLE_EQ(ops::max_op<double>(3.0, 5.0), 5.0, 0.001);
    ASSERT_DOUBLE_EQ(ops::min_op<double>(3.0, 5.0), 3.0, 0.001);
    
    // 测试符号函数
    ASSERT_EQ(ops::get_sign<double>(5.0), 1);
    ASSERT_EQ(ops::get_sign<double>(-5.0), -1);
    ASSERT_EQ(ops::get_sign<double>(0.0), 0);
    
    // 测试类型转换
    ASSERT_EQ(ops::direct_output_int32<double>(3.14), 3);
    ASSERT_EQ(ops::direct_output_int64<double>(3.14), 3);
    ASSERT_DOUBLE_EQ(ops::direct_output_double<int32_t>(42), 42.0, 0.001);
    
    std::cout << "All operator tests passed! ";
}

// ============================================
// 测试8: 配置生成
// ============================================

TEST(config_generation) {
    PipelineConfig config = create_demo_config();
    
    // 生成JSON
    std::string json = ConfigGenerator::generate_json(config);
    
    // 解析生成的JSON
    JsonConfigParser parser;
    auto parsed = parser.parse_string(json);
    
    // 验证
    ASSERT_EQ(parsed.name, config.name);
    ASSERT_EQ(parsed.inputs.size(), config.inputs.size());
    ASSERT_EQ(parsed.steps.size(), config.steps.size());
    
    std::cout << "All config generation tests passed! ";
}

// ============================================
// 主函数
// ============================================

int main() {
    std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║                   TurboGraph-JIT 单元测试                     ║
╚══════════════════════════════════════════════════════════════╝
)" << std::endl;
    
    std::cout << std::string(60, '-') << "\n\n";
    
    // 运行测试
    RUN_TEST(type_system);
    RUN_TEST(config_parsing);
    RUN_TEST(config_validation);
    RUN_TEST(code_generation);
    RUN_TEST(interpreter_execution);
    RUN_TEST(context_management);
    RUN_TEST(operators);
    RUN_TEST(config_generation);
    
    // 输出结果
    std::cout << "\n\n" << std::string(60, '=') << "\n";
    std::cout << "测试结果: " << tests_passed << " 通过, " << tests_failed << " 失败\n";
    std::cout << std::string(60, '=') << "\n";
    
    return tests_failed > 0 ? 1 : 0;
}
