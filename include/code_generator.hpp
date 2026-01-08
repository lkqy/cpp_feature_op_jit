#ifndef TURBOGRAPH_CODE_GENERATOR_HPP
#define TURBOGRAPH_CODE_GENERATOR_HPP

#include "config.hpp"
#include "types.hpp"
#include <string>
#include <sstream>
#include <unordered_map>
#include <ctime>
#include <iomanip>

namespace turbograph {

// ============================================
// 代码生成选项
// ============================================

/**
 * @brief 代码生成选项
 */
struct CodeGenOptions {
    bool enable_inline = true;
    bool enable_vectorize = true;
    bool use_fast_math = true;
    std::string compiler_flags = "-O3 -march=native -std=c++17";
    std::string output_dir = "./generated";
    bool use_cache = true;
    bool verbose = false;
};

// ============================================
// 代码生成器
// ============================================

/**
 * @brief 代码生成器
 * 将管道配置转换为可编译的C++代码
 */
class CodeGenerator {
public:
    CodeGenerator(const PipelineConfig& config, const CodeGenOptions& options = {});
    
    /**
     * @brief 生成代码
     * @return 生成的C++代码
     */
    std::string generate();
    
    /**
     * @brief 生成并保存到文件
     * @param path 输出文件路径
     * @return 是否成功
     */
    bool save_to_file(const std::string& path);
    
    /**
     * @brief 获取生成的代码
     */
    const std::string& code() const { return code_; }
    
private:
    PipelineConfig config_;
    CodeGenOptions options_;
    std::string code_;
    std::unordered_map<std::string, DataType> variables_;
    
    /**
     * @brief 收集所有变量
     */
    void collect_variables();
    
    /**
     * @brief 推断输出类型
     */
    DataType infer_output_type(const OpCall& step);
    
    /**
     * @brief 生成头部
     */
    void generate_header(std::ostream& oss);
    
    /**
     * @brief 生成命名空间开始
     */
    void generate_namespace_begin(std::ostream& oss);
    
    /**
     * @brief 生成命名空间结束
     */
    void generate_namespace_end(std::ostream& oss);
    
    /**
     * @brief 生成上下文结构
     */
    void generate_context_struct(std::ostream& oss);
    
    /**
     * @brief 生成辅助函数
     */
    void generate_helper_functions(std::ostream& oss);
    
    /**
     * @brief 生成主执行函数
     */
    void generate_execute_function(std::ostream& oss);
    
    /**
     * @brief 生成算子调用
     */
    void generate_op_call(std::ostream& oss, const OpCall& step);
    
    /**
     * @brief 生成参数代码
     */
    std::string generate_arg_code(const Arg& arg);
    
    /**
     * @brief 生成算子调用代码
     */
    std::string generate_op_call_code(const OpCall& step, const std::string& args_str);
    
    /**
     * @brief 映射算子名称
     */
    std::string map_operator_name(const std::string& op_name);
    
    /**
     * @brief 获取类型前缀
     */
    std::string get_type_prefix(DataType type);
    
    /**
     * @brief 生成导出函数
     */
    void generate_export_function(std::ostream& oss);
    
    /**
     * @brief 获取当前时间字符串
     */
    std::string get_current_time();
};

// ============================================
// 简化版本代码生成器（用于快速演示）
// ============================================

/**
 * @brief 简化版代码生成器
 * 生成更简洁但同样高效的代码
 */
class SimpleCodeGenerator {
public:
    explicit SimpleCodeGenerator(const PipelineConfig& config);
    
    /**
     * @brief 生成代码
     */
    std::string generate();
    
private:
    PipelineConfig config_;
    std::unordered_map<std::string, DataType> variables_;
    
    /**
     * @brief 收集变量
     */
    void collect_variables();
    
    /**
     * @brief 生成简化版算子调用
     */
    void generate_simple_op_call(std::ostream& oss, const OpCall& step);
    
    /**
     * @brief 映射算子
     */
    std::string map_operator(const std::string& op_name);
};

} // namespace turbograph

#endif // TURBOGRAPH_CODE_GENERATOR_HPP
