#include "config.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include <unordered_set>

using json = nlohmann::json;

namespace turbograph {

// ============================================
// PipelineConfig 实现
// ============================================

std::string PipelineConfig::compute_fingerprint() {
    std::ostringstream oss;
    oss << name << "|";
    for (const auto& input : inputs) {
        oss << input.name << ":" << data_type_to_string(input.type) << ",";
    }
    oss << "|";
    for (const auto& step : steps) {
        oss << step.op_name << "(";
        for (size_t i = 0; i < step.args.size(); i++) {
            oss << step.args[i].value;
            if (i < step.args.size() - 1) oss << ",";
        }
        oss << ")->" << step.output_var << ";";
    }
    
    // 计算SHA256哈希
    std::string str = oss.str();
    std::hash<std::string> hasher;
    size_t hash = hasher(str);
    std::ostringstream hash_oss;
    hash_oss << std::hex << hash;
    fingerprint = hash_oss.str();
    return fingerprint;
}

// ============================================
// JsonConfigParser 实现
// ============================================

PipelineConfig JsonConfigParser::parse(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + config_path);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse_string(buffer.str());
}

PipelineConfig JsonConfigParser::parse_string(const std::string& json_str) {
    auto j = json::parse(json_str);
    PipelineConfig config;
    
    // 解析基本信息
    if (j.contains("name")) {
        config.name = j["name"].get<std::string>();
    }
    
    // 解析IO定义
    if (j.contains("inputs")) {
        config.inputs = parse_io_fields(j["inputs"], "inputs");
    }
    if (j.contains("outputs")) {
        config.outputs = parse_io_fields(j["outputs"], "outputs");
    }
    if (j.contains("variables")) {
        config.variables = parse_io_fields(j["variables"], "variables");
    }
    
    // 解析步骤
    if (j.contains("steps")) {
        config.steps = parse_steps(j["steps"]);
    }
    
    // 计算指纹
    config.compute_fingerprint();
    
    return config;
}

bool JsonConfigParser::validate(const PipelineConfig& config) {
    // 检查名称
    if (config.name.empty()) {
        std::cerr << "Pipeline name is empty" << std::endl;
        return false;
    }
    
    // 检查步骤
    std::unordered_set<std::string> defined_vars;
    for (const auto& input : config.inputs) {
        defined_vars.insert(input.name);
    }
    // 注意：variables 是预先声明的变量，steps 可以重新赋值，所以不添加到defined_vars
    
    for (size_t i = 0; i < config.steps.size(); i++) {
        const auto& step = config.steps[i];
        
        // 检查算子名称
        if (step.op_name.empty()) {
            std::cerr << "Step " << i << ": empty operator name" << std::endl;
            return false;
        }
        
        // 检查输出变量
        if (step.output_var.empty()) {
            std::cerr << "Step " << i << ": empty output variable" << std::endl;
            return false;
        }
        
        // 允许步骤重新赋值已声明的变量
        defined_vars.insert(step.output_var);
    }
    
    return true;
}

std::vector<PipelineConfig::IOField> JsonConfigParser::parse_io_fields(
    const json& arr, 
    const std::string& field_name) {
    
    std::vector<PipelineConfig::IOField> result;
    
    if (!arr.is_array()) {
        throw std::runtime_error(field_name + " must be an array");
    }
    
    for (const auto& item : arr) {
        PipelineConfig::IOField field;
        
        if (item.contains("name")) {
            field.name = item["name"].get<std::string>();
        }
        
        if (item.contains("type")) {
            std::string type_str = item["type"].get<std::string>();
            field.type = string_to_data_type(type_str);
            if (field.type == DataType::UNKNOWN) {
                std::cerr << "Unknown type: " << type_str << std::endl;
            }
        }
        
        if (item.contains("required")) {
            field.required = item["required"].get<bool>();
        } else {
            field.required = true;
        }
        
        result.push_back(field);
    }
    
    return result;
}

std::vector<OpCall> JsonConfigParser::parse_steps(const json& steps) {
    std::vector<OpCall> result;
    
    if (!steps.is_array()) {
        throw std::runtime_error("steps must be an array");
    }
    
    for (const auto& step : steps) {
        OpCall op_call;
        
        if (step.contains("op")) {
            op_call.op_name = step["op"].get<std::string>();
        }
        
        if (step.contains("output")) {
            op_call.output_var = step["output"].get<std::string>();
        }
        
        if (step.contains("args")) {
            op_call.args = parse_args(step["args"]);
        }
        
        if (step.contains("options")) {
            const auto& options = step["options"];
            if (options.is_object()) {
                for (const auto& [key, value] : options.items()) {
                    op_call.options[key] = value.get<std::string>();
                }
            }
        }
        
        result.push_back(op_call);
    }
    
    return result;
}

std::vector<Arg> JsonConfigParser::parse_args(const json& args) {
    std::vector<Arg> result;
    
    if (!args.is_array()) {
        throw std::runtime_error("args must be an array");
    }
    
    for (const auto& arg : args) {
        result.push_back(parse_arg(arg));
    }
    
    return result;
}

Arg JsonConfigParser::parse_arg(const json& arg) {
    if (arg.is_string()) {
        // 如果是字符串，尝试判断是变量还是字面量
        std::string str = arg.get<std::string>();
        
        // 以$开头的认为是变量引用
        if (!str.empty() && str[0] == '$') {
            return Arg::variable(str.substr(1), DataType::UNKNOWN);
        }
        
        // 尝试解析为数字
        try {
            size_t pos;
            double val = std::stod(str, &pos);
            if (pos == str.length()) {
                // 是数字，根据是否有小数点判断类型
                if (str.find('.') != std::string::npos || 
                    str.find('e') != std::string::npos ||
                    str.find('E') != std::string::npos) {
                    return Arg::literal(str, DataType::DOUBLE);
                } else {
                    // 检查是否超出int32范围
                    int64_t int_val = std::stoll(str);
                    if (int_val > INT32_MAX || int_val < INT32_MIN) {
                        return Arg::literal(str, DataType::INT64);
                    }
                    return Arg::literal(str, DataType::INT32);
                }
            }
        } catch (...) {
            // 不是数字，作为字符串字面量
        }
        
        // 默认作为字符串处理
        return Arg::literal(str, DataType::STRING);
    }
    
    if (arg.is_number_integer()) {
        int64_t val = arg.get<int64_t>();
        if (val > INT32_MAX || val < INT32_MIN) {
            return Arg::literal(std::to_string(val), DataType::INT64);
        }
        return Arg::literal(std::to_string(val), DataType::INT32);
    }
    
    if (arg.is_number()) {
        return Arg::literal(std::to_string(arg.get<double>()), DataType::DOUBLE);
    }
    
    if (arg.is_boolean()) {
        return Arg::literal(arg.get<bool>() ? "true" : "false", DataType::INT32);
    }
    
    return Arg::literal(arg.dump(), DataType::STRING);
}

DataType JsonConfigParser::infer_arg_type(const nlohmann::json& arg, const PipelineConfig& config) {
    // 根据上下文推断参数类型
    // 这里可以添加更复杂的类型推断逻辑
    return DataType::UNKNOWN;
}

// ============================================
// ConfigGenerator 实现
// ============================================

std::string ConfigGenerator::generate_json(const PipelineConfig& config) {
    json j;
    
    j["name"] = config.name;
    
    // 生成输入
    j["inputs"] = json::array();
    for (const auto& input : config.inputs) {
        json field;
        field["name"] = input.name;
        field["type"] = data_type_to_string(input.type);
        field["required"] = input.required;
        j["inputs"].push_back(field);
    }
    
    // 生成变量
    j["variables"] = json::array();
    for (const auto& var : config.variables) {
        json field;
        field["name"] = var.name;
        field["type"] = data_type_to_string(var.type);
        field["required"] = var.required;
        j["variables"].push_back(field);
    }
    
    // 生成步骤
    j["steps"] = json::array();
    for (const auto& step : config.steps) {
        json step_obj;
        step_obj["op"] = step.op_name;
        step_obj["output"] = step.output_var;
        
        step_obj["args"] = json::array();
        for (const auto& arg : step.args) {
            if (arg.type == ArgType::VARIABLE) {
                step_obj["args"].push_back("$" + arg.value);
            } else {
                step_obj["args"].push_back(arg.value);
            }
        }
        
        if (!step.options.empty()) {
            step_obj["options"] = json::object();
            for (const auto& [key, value] : step.options) {
                step_obj["options"][key] = value;
            }
        }
        
        j["steps"].push_back(step_obj);
    }
    
    return j.dump(2);
}

bool ConfigGenerator::save_to_file(const PipelineConfig& config, const std::string& path) {
    std::string json_str = generate_json(config);
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    file << json_str;
    return true;
}

// ============================================
// OpRegistry 实现
// ============================================

OpRegistry& OpRegistry::instance() {
    static OpRegistry registry;
    return registry;
}

void OpRegistry::register_op(const std::string& name, Creator creator) {
    registry_[name] = std::move(creator);
}

const OpRegistry::Creator* OpRegistry::get_creator(const std::string& name) const {
    auto it = registry_.find(name);
    if (it != registry_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool OpRegistry::has_op(const std::string& name) const {
    return registry_.find(name) != registry_.end();
}

std::vector<std::string> OpRegistry::list_ops() const {
    std::vector<std::string> result;
    for (const auto& [name, creator] : registry_) {
        result.push_back(name);
    }
    return result;
}

} // namespace turbograph
