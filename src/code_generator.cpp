#include "code_generator.hpp"
#include "ops.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <set>
#include <cctype>

namespace turbograph {

// ============================================
// 辅助函数：生成合法的C++标识符
// ============================================

std::string make_valid_identifier(const std::string& str) {
    if (str.empty()) return "p_invalid";
    std::string result = str;
    // 如果以数字开头，添加前缀"p_"
    if (std::isdigit(result[0])) {
        result = "p_" + result;
    }
    // 替换不合法的字符
    for (char& c : result) {
        if (!std::isalnum(c) && c != '_') {
            c = '_';
        }
    }
    return result;
}

// ============================================
// 算子注册表 - 自动从ops.hpp发现算子
// ============================================

/**
 * @brief 算子元数据
 */
struct OperatorMetadata {
    std::string config_name;      // 配置中的名称
    std::string function_name;    // ops.hpp中的函数名
    DataType return_type;         // 返回类型
    int param_count;              // 参数个数
    bool needs_template;          // 是否需要模板参数
    std::string default_template; // 默认模板参数
};

/**
 * @brief 算子注册表
 * 自动发现并注册所有可用的算子
 */
class OperatorRegistry {
public:
    static OperatorRegistry& instance() {
        static OperatorRegistry registry;
        return registry;
    }
    
    // 获取算子元数据
    const OperatorMetadata* get_operator(const std::string& config_name) const {
        auto it = operators_.find(config_name);
        if (it != operators_.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    // 获取所有算子名称
    std::vector<std::string> get_all_operator_names() const {
        std::vector<std::string> names;
        for (const auto& op : operators_) {
            names.push_back(op.first);
        }
        return names;
    }
    
    // 检查算子是否存在
    bool has_operator(const std::string& config_name) const {
        return operators_.find(config_name) != operators_.end();
    }
    
private:
    OperatorRegistry() {
        register_all_operators();
    }
    
    void register_all_operators() {
        // 基础数学算子（无模板）
        register_operator("get_sign", "get_sign", DataType::INT32, 1, false);
        register_operator("price_diff", "price_diff", DataType::DOUBLE, 2, false);
        register_operator("avg_avg_log", "avg_avg_log", DataType::INT64, 5, false);
        
        // 类型转换算子（需要模板）
        register_operator("direct_output_int32", "direct_output_int32", DataType::INT32, 1, true, "int32_t");
        register_operator("direct_output_int64", "direct_output_int64", DataType::INT64, 1, true, "int64_t");
        register_operator("direct_output_double", "direct_output_double", DataType::DOUBLE, 1, true, "double");
        register_operator("direct_output_string", "direct_output_string", DataType::STRING, 1, true, "double");
        
        // 容器操作算子
        register_operator("len", "len", DataType::INT64, 1, false);
        register_operator("list_to_string", "list_to_string", DataType::STRING, 2, false);
        register_operator("catein_list_cross", "catein_list_cross", DataType::INT32, 2, false);
        register_operator("catein_list_cross_count", "catein_list_cross_count", DataType::INT32, 2, false);
        
        // 扩展算子（需要模板）
        register_operator("add", "add_op", DataType::DOUBLE, 2, true, "double");
        register_operator("sub", "sub_op", DataType::DOUBLE, 2, true, "double");
        register_operator("mul", "mul_op", DataType::DOUBLE, 2, true, "double");
        register_operator("div", "div_op", DataType::DOUBLE, 2, true, "double");
        register_operator("if_else", "if_else", DataType::DOUBLE, 3, false);
        register_operator("max", "max_op", DataType::DOUBLE, 2, true, "double");
        register_operator("min", "min_op", DataType::DOUBLE, 2, true, "double");
        register_operator("abs", "abs_op", DataType::DOUBLE, 1, true, "double");
        register_operator("square", "square_op", DataType::DOUBLE, 1, true, "double");
        register_operator("sqrt", "sqrt_op", DataType::DOUBLE, 1, true, "double");
        register_operator("floor", "floor_op", DataType::INT32, 1, true, "double");
        register_operator("ceil", "ceil_op", DataType::INT32, 1, true, "double");
        register_operator("percent", "percent_op", DataType::DOUBLE, 2, false);
        register_operator("moving_average", "moving_average", DataType::DOUBLE, 2, false);
        register_operator("vector_sum", "vector_sum", DataType::DOUBLE, 1, false);
        register_operator("vector_avg", "vector_avg", DataType::DOUBLE, 1, false);
    }
    
    void register_operator(const std::string& config_name, 
                          const std::string& function_name,
                          DataType return_type,
                          int param_count,
                          bool needs_template,
                          const std::string& default_template = "") {
        OperatorMetadata meta;
        meta.config_name = config_name;
        meta.function_name = function_name;
        meta.return_type = return_type;
        meta.param_count = param_count;
        meta.needs_template = needs_template;
        meta.default_template = default_template;
        operators_[config_name] = meta;
    }
    
    std::unordered_map<std::string, OperatorMetadata> operators_;
};

// ============================================
// 代码生成器实现
// ============================================

CodeGenerator::CodeGenerator(const PipelineConfig& config, const CodeGenOptions& options)
    : config_(config), options_(options) {
    
    // 收集所有使用的变量
    collect_variables();
}

void CodeGenerator::collect_variables() {
    // 收集输入变量
    for (const auto& input : config_.inputs) {
        variables_[input.name] = input.type;
    }
    
    // 收集变量定义
    for (const auto& var : config_.variables) {
        variables_[var.name] = var.type;
    }
    
    // 收集步骤输出变量
    for (const auto& step : config_.steps) {
        // 尝试从算子注册表获取输出类型
        DataType out_type = infer_output_type(step);
        variables_[step.output_var] = out_type;
    }
}

DataType CodeGenerator::infer_output_type(const OpCall& step) {
    // 从注册表获取算子信息
    const auto* meta = OperatorRegistry::instance().get_operator(step.op_name);
    if (meta) {
        return meta->return_type;
    }
    
    // 备用：推断类型
    static const std::unordered_map<std::string, DataType> op_output_types = {
        {"get_sign", DataType::INT32},
        {"price_diff", DataType::DOUBLE},
        {"avg_avg_log", DataType::INT64},
        {"direct_output_int32", DataType::INT32},
        {"direct_output_int64", DataType::INT64},
        {"direct_output_double", DataType::DOUBLE},
        {"direct_output_string", DataType::STRING},
        {"len", DataType::INT64},
        {"list_to_string", DataType::STRING},
        {"catein_list_cross", DataType::INT32},
        {"catein_list_cross_count", DataType::INT32},
        {"add", DataType::DOUBLE},
        {"sub", DataType::DOUBLE},
        {"mul", DataType::DOUBLE},
        {"div", DataType::DOUBLE},
        {"if_else", DataType::DOUBLE},
        {"max", DataType::DOUBLE},
        {"min", DataType::DOUBLE},
        {"abs", DataType::DOUBLE},
        {"square", DataType::DOUBLE},
        {"sqrt", DataType::DOUBLE},
        {"floor", DataType::INT32},
        {"ceil", DataType::INT32},
        {"percent", DataType::DOUBLE},
        {"moving_average", DataType::DOUBLE},
        {"vector_sum", DataType::DOUBLE},
        {"vector_avg", DataType::DOUBLE},
    };
    
    auto it = op_output_types.find(step.op_name);
    if (it != op_output_types.end()) {
        return it->second;
    }
    
    return DataType::DOUBLE;
}

std::string CodeGenerator::generate() {
    std::ostringstream oss;
    
    // 生成头部
    generate_header(oss);
    
    // 生成命名空间
    generate_namespace_begin(oss);
    
    // 生成上下文结构
    generate_context_struct(oss);
    
    // 生成辅助函数
    generate_helper_functions(oss);
    
    // 生成主执行函数
    generate_execute_function(oss);
    
    // 生成导出函数
    generate_export_function(oss);
    
    // 结束命名空间
    generate_namespace_end(oss);
    
    code_ = oss.str();
    return code_;
}

void CodeGenerator::generate_header(std::ostream& oss) {
    std::string ns_name = make_valid_identifier(config_.fingerprint);
    
    oss << R"(// ============================================================
// Auto-generated pipeline code
// Pipeline: )" << config_.name << R"(
// Generated at: )" << get_current_time() << R"(
// Fingerprint: )" << config_.fingerprint << R"(
// Namespace: )" << ns_name << R"(
// Generated by: TurboGraph-JIT Code Generator
// ============================================================

#ifndef TURBOGRAPH_GENERATED_)" << config_.fingerprint << R"(
#define TURBOGRAPH_GENERATED_)" << config_.fingerprint << R"(

#include <cmath>
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <cstdint>

// 引入算子库（使用绝对路径）
#include "/workspace/turbograph_jit/include/ops.hpp"

)";
}

void CodeGenerator::generate_namespace_begin(std::ostream& oss) {
    std::string ns_name = make_valid_identifier(config_.fingerprint);
    oss << R"(namespace turbograph {
namespace generated {
namespace )" << ns_name << R"( {

)";
}

void CodeGenerator::generate_namespace_end(std::ostream& oss) {
    std::string ns_name = make_valid_identifier(config_.fingerprint);
    oss << R"(
}  // namespace )" << ns_name << R"(
}  // namespace generated
}  // namespace turbograph

#endif  // TURBOGRAPH_GENERATED_)" << config_.fingerprint << R"(
)";
}

void CodeGenerator::generate_context_struct(std::ostream& oss) {
    oss << R"(
// ============================================================
// 执行上下文结构
// ============================================================
struct PipelineContext {
)";
    
    // 生成输入变量
    if (!config_.inputs.empty()) {
        oss << "    // 输入变量\n";
        for (const auto& input : config_.inputs) {
            oss << "    " << get_cpp_type_name(input.type) << " " << input.name << ";\n";
        }
    }
    
    // 生成中间变量
    if (!config_.variables.empty() || !config_.steps.empty()) {
        oss << "    // 中间变量\n";
        for (const auto& var : config_.variables) {
            oss << "    " << get_cpp_type_name(var.type) << " " << var.name << ";\n";
        }
        // 步骤输出变量（只添加不在inputs、variables和steps中的）
        std::set<std::string> defined_vars;
        for (const auto& input : config_.inputs) {
            defined_vars.insert(input.name);
        }
        for (const auto& var : config_.variables) {
            defined_vars.insert(var.name);
        }
        for (const auto& step : config_.steps) {
            defined_vars.insert(step.output_var);
        }
        std::set<std::string> output_vars;
        for (const auto& step : config_.steps) {
            output_vars.insert(step.output_var);
        }
        for (const auto& var_name : output_vars) {
            if (defined_vars.find(var_name) == defined_vars.end()) {
                auto it = variables_.find(var_name);
                if (it != variables_.end()) {
                    oss << "    " << get_cpp_type_name(it->second) << " " << var_name << ";\n";
                }
            }
        }
    }
    
    // 生成输出变量（只添加不在inputs、variables和steps中的）
    if (!config_.outputs.empty()) {
        oss << "    // 输出变量\n";
        // 收集所有已定义的变量名
        std::set<std::string> all_defined;
        for (const auto& input : config_.inputs) {
            all_defined.insert(input.name);
        }
        for (const auto& var : config_.variables) {
            all_defined.insert(var.name);
        }
        for (const auto& step : config_.steps) {
            all_defined.insert(step.output_var);
        }
        // 只输出不在已定义集合中的变量
        for (const auto& output : config_.outputs) {
            if (all_defined.find(output.name) == all_defined.end()) {
                oss << "    " << get_cpp_type_name(output.type) << " " << output.name << ";\n";
            }
        }
    }
    
    oss << R"(
    // 构造函数
    PipelineContext() = default;
};

)";
}

void CodeGenerator::generate_helper_functions(std::ostream& oss) {
    oss << R"(
// ============================================================
// 辅助函数
// ============================================================

// 类型转换辅助函数
inline int32_t to_int32(double value) {
    return static_cast<int32_t>(value);
}

inline int64_t to_int64(double value) {
    return static_cast<int64_t>(value);
}

inline double to_double(int32_t value) {
    return static_cast<double>(value);
}

inline double to_double(int64_t value) {
    return static_cast<double>(value);
}

)";
}

void CodeGenerator::generate_execute_function(std::ostream& oss) {
    oss << R"(
// ============================================================
// 主执行函数
// ============================================================

bool execute_internal(PipelineContext& ctx) {
)";
    
    // 生成算子调用代码
    for (const auto& step : config_.steps) {
        generate_op_call(oss, step);
    }
    
    // 生成输出赋值
    if (!config_.outputs.empty()) {
        oss << "\n    // 赋值输出变量（输出变量已在算子执行中赋值）\n";
    }
    
    oss << R"(
    return true;
}

)";
}

void CodeGenerator::generate_op_call(std::ostream& oss, const OpCall& step) {
    oss << "    // " << step.op_name << " -> " << step.output_var << "\n";
    
    // 生成参数代码
    std::ostringstream args_oss;
    bool first = true;
    for (const auto& arg : step.args) {
        if (!first) {
            args_oss << ", ";
        }
        first = false;
        
        std::string arg_str = generate_arg_code(arg);
        args_oss << arg_str;
    }
    
    // 生成算子调用代码
    std::string op_call = generate_op_call_code(step, args_oss.str());
    
    oss << "    " << op_call << "\n\n";
}

std::string CodeGenerator::generate_arg_code(const Arg& arg) {
    if (arg.type == ArgType::VARIABLE) {
        return "ctx." + arg.value;
    } else {
        // 字面量
        return arg.value;
    }
}

std::string CodeGenerator::generate_op_call_code(const OpCall& step, const std::string& args_str) {
    // 从注册表获取算子信息
    const auto* meta = OperatorRegistry::instance().get_operator(step.op_name);
    
    if (meta) {
        // 使用注册表中的信息生成调用
        std::string call_code = "ctx." + step.output_var + " = ::turbograph::ops::" + meta->function_name;
        
        if (meta->needs_template) {
            // 需要模板参数
            DataType output_type = infer_output_type(step);
            std::string type_prefix = get_type_prefix(output_type);
            call_code += "<" + type_prefix + ">";
        }
        
        call_code += "(" + args_str + ");";
        return call_code;
    }
    
    // 未知算子，使用通用处理
    return "ctx." + step.output_var + " = ::turbograph::ops::" + step.op_name + "(" + args_str + ");";
}

std::string CodeGenerator::map_operator_name(const std::string& op_name) {
    // 从注册表获取函数名
    const auto* meta = OperatorRegistry::instance().get_operator(op_name);
    if (meta) {
        return meta->function_name;
    }
    return op_name;
}

std::string CodeGenerator::get_type_prefix(DataType type) {
    switch (type) {
        case DataType::INT32: return "int32_t";
        case DataType::INT64: return "int64_t";
        case DataType::DOUBLE: return "double";
        case DataType::FLOAT: return "float";
        default: return "double";
    }
}

void CodeGenerator::generate_export_function(std::ostream& oss) {
    std::string ns_name = make_valid_identifier(config_.fingerprint);
    
    oss << R"(
// ============================================================
// 导出接口 (C链接)
// ============================================================

extern "C" {

bool pipeline_execute_)" << ns_name << R"((void* input_data, void* output_data) {
    PipelineContext ctx;
    
    // 解析输入数据
)";
    
    // 生成输入解析代码
    size_t offset = 0;
    for (const auto& input : config_.inputs) {
        std::string type_name = get_cpp_type_name(input.type);
        oss << "    // 输入: " << input.name << " (" << type_name << ")\n";
        oss << "    if (input_data) {\n";
        if (type_name == "double") {
            oss << "        double* arr = static_cast<double*>(input_data);\n";
            oss << "        ctx." << input.name << " = arr[" << offset << "];\n";
        } else if (type_name == "int32_t") {
            oss << "        int32_t* arr = static_cast<int32_t*>(input_data);\n";
            oss << "        ctx." << input.name << " = arr[" << offset << "];\n";
        } else if (type_name == "int64_t") {
            oss << "        int64_t* arr = static_cast<int64_t*>(input_data);\n";
            oss << "        ctx." << input.name << " = arr[" << offset << "];\n";
        }
        offset++;
        oss << "    }\n\n";
    }
    
    // 执行内部逻辑
    oss << R"(
    // 执行管道
    bool result = execute_internal(ctx);
    
    // 写入输出数据
    if (output_data && result) {
)";
    
    // 生成输出写入代码
    offset = 0;
    for (const auto& output : config_.outputs) {
        std::string type_name = get_cpp_type_name(output.type);
        oss << "        // 输出: " << output.name << "\n";
        if (type_name == "double") {
            oss << "        {\n";
            oss << "            double* arr = static_cast<double*>(output_data);\n";
            oss << "            arr[" << offset << "] = static_cast<double>(ctx." << output.name << ");\n";
            oss << "        }\n";
        } else if (type_name == "int32_t") {
            oss << "        {\n";
            oss << "            int32_t* arr = static_cast<int32_t*>(output_data);\n";
            oss << "            arr[" << offset << "] = static_cast<int32_t>(ctx." << output.name << ");\n";
            oss << "        }\n";
        } else if (type_name == "int64_t") {
            oss << "        {\n";
            oss << "            int64_t* arr = static_cast<int64_t*>(output_data);\n";
            oss << "            arr[" << offset << "] = static_cast<int64_t>(ctx." << output.name << ");\n";
            oss << "        }\n";
        }
        offset++;
    }
    
    oss << R"(
    }
    
    return result;
}

// 获取执行器信息
const char* pipeline_name() {
    return ")" << config_.name << R"(";
}

const char* pipeline_fingerprint() {
    return ")" << config_.fingerprint << R"(";
}

}  // extern "C"
)";
}

bool CodeGenerator::save_to_file(const std::string& path) {
    std::string code = generate();
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << path << std::endl;
        return false;
    }
    file << code;
    file.close();
    return true;
}

std::string CodeGenerator::get_current_time() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// ============================================
// 简化版本代码生成器
// ============================================

SimpleCodeGenerator::SimpleCodeGenerator(const PipelineConfig& config)
    : config_(config) {
    config_.compute_fingerprint();
    
    // 收集变量
    for (const auto& input : config_.inputs) {
        variables_[input.name] = input.type;
    }
    for (const auto& var : config_.variables) {
        variables_[var.name] = var.type;
    }
    for (const auto& step : config_.steps) {
        variables_[step.output_var] = DataType::DOUBLE;
    }
}

std::string SimpleCodeGenerator::generate() {
    std::ostringstream oss;
    std::string ns_name = make_valid_identifier(config_.fingerprint);
    
    oss << "// ============================================================\n";
    oss << "// Auto-generated pipeline code\n";
    oss << "// Pipeline: " << config_.name << "\n";
    oss << "// Fingerprint: " << config_.fingerprint << "\n";
    oss << "// Namespace: " << ns_name << "\n";
    oss << "// ============================================================\n\n";
    
    oss << "#include <cmath>\n";
    oss << "#include <string>\n";
    oss << "#include <vector>\n";
    oss << "#include <sstream>\n";
    oss << "#include <stdexcept>\n";
    oss << "#include <cstdint>\n";
    oss << "#include <iostream>\n\n";
    
    // 使用绝对路径引入算子库
    oss << "#include \"/workspace/turbograph_jit/include/ops.hpp\"\n\n";
    
    oss << "using namespace turbograph::ops;\n\n";
    
    oss << "extern \"C\" {\n\n";
    
    oss << "// 简化版本的执行函数\n";
    oss << "bool pipeline_execute_" << ns_name << R"((double* inputs, double* outputs, int32_t* int_inputs, int32_t* int_outputs) {
)";
    
    // 收集所有需要声明的变量，避免重复
    std::set<std::string> declared_vars;
    
    // 声明变量（只声明一次）
    for (const auto& var : config_.variables) {
        oss << "    " << get_cpp_type_name(var.type) << " " << var.name << ";\n";
        declared_vars.insert(var.name);
    }
    
    for (const auto& step : config_.steps) {
        // 只声明不在已声明变量中的输出变量
        if (declared_vars.find(step.output_var) == declared_vars.end()) {
            auto it = variables_.find(step.output_var);
            if (it != variables_.end()) {
                oss << "    " << get_cpp_type_name(it->second) << " " << step.output_var << ";\n";
                declared_vars.insert(step.output_var);
            }
        }
    }
    
    oss << "\n    // 算子执行\n";
    
    // 生成算子调用
    for (const auto& step : config_.steps) {
        generate_simple_op_call(oss, step);
    }
    
    // 输出结果
    if (!config_.outputs.empty()) {
        oss << "\n    // 输出结果\n";
        size_t idx = 0;
        for (const auto& output : config_.outputs) {
            // 直接使用变量名，不再使用ctx前缀
            oss << "    if (outputs) outputs[" << idx << "] = static_cast<double>(" 
                << output.name << ");\n";
            idx++;
        }
    }
    
    oss << "\n    return true;\n";
    oss << "}\n\n";
    oss << "}  // extern \"C\"\n";
    
    return oss.str();
}

void SimpleCodeGenerator::generate_simple_op_call(std::ostream& oss, const OpCall& step) {
    oss << "    // " << step.op_name << " -> " << step.output_var << "\n";
    
    std::ostringstream args_oss;
    bool first = true;
    for (const auto& arg : step.args) {
        if (!first) args_oss << ", ";
        first = false;
        
        if (arg.type == ArgType::VARIABLE) {
            args_oss << arg.value;
        } else {
            args_oss << arg.value;
        }
    }
    
    // 从注册表获取算子信息
    const auto* meta = OperatorRegistry::instance().get_operator(step.op_name);
    std::string func_name;
    
    if (meta) {
        func_name = meta->function_name;
        if (meta->needs_template) {
            func_name += "<double>";
        }
    } else {
        func_name = step.op_name;
    }
    
    oss << "    " << step.output_var << " = " << func_name 
        << "(" << args_oss.str() << ");\n\n";
}

std::string SimpleCodeGenerator::map_operator(const std::string& op_name) {
    const auto* meta = OperatorRegistry::instance().get_operator(op_name);
    if (meta) {
        std::string result = meta->function_name;
        if (meta->needs_template) {
            result += "<double>";
        }
        return result;
    }
    return op_name;
}

} // namespace turbograph
