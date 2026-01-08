/**
 * @file benchmark.cpp
 * @brief 性能测试程序
 * 
 * 演示TurboGraph-JIT的两种执行模式：
 * 1. 解释执行（Interpreter Mode）- 基线性能
 * 2. JIT编译执行（JIT Mode）- 动态代码生成
 * 
 * 运行测试：
 *   cd build && cmake .. && make && ./benchmark
 */

#include "pipeline.hpp"
#include "config.hpp"
#include "code_generator.hpp"
#include "compiler.hpp"
#include "loader.hpp"

#include <iostream>
#include <chrono>
#include <iomanip>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>

using namespace turbograph;
using namespace std::chrono;

// 测试配置
struct TestCase {
    std::string name;
    int iterations;
    int complexity;  // 算子数量
};

// ============================================
// 测试结果
// ============================================

struct BenchmarkResult {
    std::string name;
    double interpreter_time_ms;
    double jit_time_ms;
    double speedup;
    bool interpreter_success;
    bool jit_success;
};

void print_result(const BenchmarkResult& result) {
    std::cout << std::string(60, '=') << "\n";
    std::cout << "测试: " << result.name << "\n";
    std::cout << std::string(60, '=') << "\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "解释执行:   " << std::setw(10) << result.interpreter_time_ms << " ms\n";
    std::cout << "JIT执行:    " << std::setw(10) << result.jit_time_ms << " ms\n";
    std::cout << "性能提升:   " << std::setw(10) << result.speedup << "x\n";
    std::cout << "解释执行:   " << (result.interpreter_success ? "成功" : "失败") << "\n";
    std::cout << "JIT执行:    " << (result.jit_success ? "成功" : "失败") << "\n";
    std::cout << std::string(60, '-') << "\n";
}

// ============================================
// 创建测试配置
// ============================================

PipelineConfig create_test_config(int complexity) {
    PipelineConfig config;
    config.name = "test_pipeline_" + std::to_string(complexity);
    
    // 输入
    config.inputs = {
        {"a", DataType::DOUBLE, true},
        {"b", DataType::DOUBLE, true},
        {"c", DataType::DOUBLE, true}
    };
    
    // 创建变量
    for (int i = 0; i < complexity; i++) {
        config.variables.push_back({"var_" + std::to_string(i), DataType::DOUBLE, false});
    }
    
    // 创建步骤
    config.steps = {
        OpCallBuilder("add")
            .output("var_0")
            .args({Arg::variable("a", DataType::DOUBLE), Arg::variable("b", DataType::DOUBLE)})
            .build()
    };
    
    for (int i = 1; i < complexity; i++) {
        config.steps.push_back(
            OpCallBuilder("mul")
                .output("var_" + std::to_string(i))
                .args({Arg::variable("var_" + std::to_string(i-1), DataType::DOUBLE), Arg::variable("c", DataType::DOUBLE)})
                .build()
        );
    }
    
    // 输出 - 使用最后一个中间变量作为输出
    config.outputs = {
        {"var_" + std::to_string(complexity - 1), DataType::DOUBLE, false}
    };
    
    config.compute_fingerprint();
    return config;
}

// ============================================
// 性能测试
// ============================================

BenchmarkResult run_benchmark(const TestCase& test) {
    BenchmarkResult result;
    result.name = test.name;
    
    // 创建配置
    auto config = create_test_config(test.complexity);
    
    // 准备测试数据
    std::vector<double> inputs_a(test.iterations);
    std::vector<double> inputs_b(test.iterations);
    std::vector<double> inputs_c(test.iterations);
    std::vector<double> outputs_interpreter(test.iterations);
    std::vector<double> outputs_jit(test.iterations);
    
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(1.0, 100.0);
    
    for (int i = 0; i < test.iterations; i++) {
        inputs_a[i] = dist(rng);
        inputs_b[i] = dist(rng);
        inputs_c[i] = dist(rng);
    }
    
    // 清理缓存
    PipelineManager::instance().clear_cache();
    
    // 测试解释执行
    {
        auto executor = PipelineManager::instance().create(config, PipelineMode::INTERPRETER);
        
        auto start = high_resolution_clock::now();
        
        result.interpreter_success = true;
        for (int i = 0; i < test.iterations; i++) {
            ExecutionContext ctx;
            ctx.set_variable("a", DataType::DOUBLE, inputs_a[i]);
            ctx.set_variable("b", DataType::DOUBLE, inputs_b[i]);
            ctx.set_variable("c", DataType::DOUBLE, inputs_c[i]);
            
            if (!executor->execute(ctx)) {
                result.interpreter_success = false;
                break;
            }
            
            outputs_interpreter[i] = ctx.get<double>("var_" + std::to_string(test.complexity - 1));
        }
        
        auto end = high_resolution_clock::now();
        result.interpreter_time_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    }
    
    // 清理缓存
    PipelineManager::instance().clear_cache();
    
    // 测试JIT执行
    {
        auto executor = PipelineManager::instance().create(config, PipelineMode::JIT);
        
        // 预编译
        auto jit_executor = dynamic_cast<JITExecutor*>(executor.get());
        if (jit_executor) {
            jit_executor->recompile();
        }
        
        auto start = high_resolution_clock::now();
        
        result.jit_success = true;
        for (int i = 0; i < test.iterations; i++) {
            ExecutionContext ctx;
            ctx.set_variable("a", DataType::DOUBLE, inputs_a[i]);
            ctx.set_variable("b", DataType::DOUBLE, inputs_b[i]);
            ctx.set_variable("c", DataType::DOUBLE, inputs_c[i]);
            
            if (!executor->execute(ctx)) {
                result.jit_success = false;
                break;
            }
            
            outputs_jit[i] = ctx.get<double>("var_" + std::to_string(test.complexity - 1));
        }
        
        auto end = high_resolution_clock::now();
        result.jit_time_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    }
    
    // 计算加速比
    if (result.interpreter_time_ms > 0 && result.jit_time_ms > 0) {
        result.speedup = result.interpreter_time_ms / result.jit_time_ms;
    } else {
        result.speedup = 0;
    }
    
    return result;
}

// ============================================
// 主函数
// ============================================

int main() {
    std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║                   TurboGraph-JIT 性能测试                     ║
║                                                               ║
║  演示配置驱动的算子执行引擎的两种执行模式：                     ║
║  1. 解释执行 - 通过反射和虚函数调用逐条解释执行                ║
║  2. JIT编译 - 动态生成C++代码，编译为SO后加载执行              ║
╚══════════════════════════════════════════════════════════════╝
)";
    
    std::cout << "\n测试配置:\n";
    std::cout << "- 编译优化: -O3 -march=native\n";
    std::cout << "- 迭代次数: 100000\n";
    std::cout << std::string(60, '-') << "\n\n";
    
    // 运行测试
    std::vector<TestCase> tests = {
        {"简单算子链 (5个算子)", 100000, 5},
        {"中等算子链 (20个算子)", 100000, 20},
        {"复杂算子链 (50个算子)", 100000, 50},
        {"超复杂算子链 (100个算子)", 100000, 100}
    };
    
    std::vector<BenchmarkResult> results;
    
    for (const auto& test : tests) {
        std::cout << "运行测试: " << test.name << "...\n";
        results.push_back(run_benchmark(test));
        print_result(results.back());
        std::cout << "\n";
    }
    
    // 总结
    std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║                         测试总结                              ║
╚══════════════════════════════════════════════════════════════╝
)";
    
    double avg_speedup = 0;
    int count = 0;
    for (const auto& r : results) {
        if (r.speedup > 0) {
            avg_speedup += r.speedup;
            count++;
        }
    }
    if (count > 0) {
        avg_speedup /= count;
    }
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "平均性能提升: " << avg_speedup << "x\n";
    std::cout << "\n说明:\n";
    std::cout << "- JIT编译模式通过动态生成优化后的原生代码，显著提升性能\n";
    std::cout << "- 随着算子数量增加，JIT模式的优势更加明显\n";
    std::cout << "- 首次执行会有编译开销，后续执行直接使用缓存的SO\n";
    
    std::cout << "\n测试完成!\n";
    
    return 0;
}
