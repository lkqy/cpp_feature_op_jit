#ifndef TURBOGRAPH_TYPES_HPP
#define TURBOGRAPH_TYPES_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <memory>
#include <functional>
#include <stdexcept>
#include <sstream>

namespace turbograph {

// ============================================
// 类型定义
// ============================================

/**
 * @brief 支持的数据类型枚举
 */
enum class DataType {
    INT32,
    INT64,
    DOUBLE,
    FLOAT,
    STRING,
    INT32_LIST,
    INT64_LIST,
    DOUBLE_LIST,
    STRING_LIST,
    UNKNOWN
};

/**
 * @brief 数据值变体类型
 */
using ValueVariant = std::variant<
    int32_t,
    int64_t,
    double,
    float,
    std::string,
    std::vector<int32_t>,
    std::vector<int64_t>,
    std::vector<double>,
    std::vector<std::string>
>;

// ============================================
// 类型工具函数
// ============================================

/**
 * @brief 获取类型的字符串表示
 */
inline std::string data_type_to_string(DataType type) {
    switch (type) {
        case DataType::INT32: return "int32";
        case DataType::INT64: return "int64";
        case DataType::DOUBLE: return "double";
        case DataType::FLOAT: return "float";
        case DataType::STRING: return "string";
        case DataType::INT32_LIST: return "int32_list";
        case DataType::INT64_LIST: return "int64_list";
        case DataType::DOUBLE_LIST: return "double_list";
        case DataType::STRING_LIST: return "string_list";
        default: return "unknown";
    }
}

/**
 * @brief 从字符串解析数据类型
 */
inline DataType string_to_data_type(const std::string& str) {
    if (str == "int32") return DataType::INT32;
    if (str == "int64") return DataType::INT64;
    if (str == "double") return DataType::DOUBLE;
    if (str == "float") return DataType::FLOAT;
    if (str == "string") return DataType::STRING;
    if (str == "int32_list") return DataType::INT32_LIST;
    if (str == "int64_list") return DataType::INT64_LIST;
    if (str == "double_list") return DataType::DOUBLE_LIST;
    if (str == "string_list") return DataType::STRING_LIST;
    return DataType::UNKNOWN;
}

/**
 * @brief 获取C++类型名称
 */
inline std::string get_cpp_type_name(DataType type) {
    switch (type) {
        case DataType::INT32: return "int32_t";
        case DataType::INT64: return "int64_t";
        case DataType::DOUBLE: return "double";
        case DataType::FLOAT: return "float";
        case DataType::STRING: return "std::string";
        case DataType::INT32_LIST: return "std::vector<int32_t>";
        case DataType::INT64_LIST: return "std::vector<int64_t>";
        case DataType::DOUBLE_LIST: return "std::vector<double>";
        case DataType::STRING_LIST: return "std::vector<std::string>";
        default: return "void";
    }
}

/**
 * @brief 判断是否为列表类型
 */
inline bool is_list_type(DataType type) {
    return type == DataType::INT32_LIST ||
           type == DataType::INT64_LIST ||
           type == DataType::DOUBLE_LIST ||
           type == DataType::STRING_LIST;
}

/**
 * @brief 获取列表元素类型
 */
inline DataType get_list_element_type(DataType list_type) {
    switch (list_type) {
        case DataType::INT32_LIST: return DataType::INT32;
        case DataType::INT64_LIST: return DataType::INT64;
        case DataType::DOUBLE_LIST: return DataType::DOUBLE;
        case DataType::STRING_LIST: return DataType::STRING;
        default: return DataType::UNKNOWN;
    }
}

// ============================================
// 执行上下文
// ============================================

/**
 * @brief 变量存储结构
 */
struct Variable {
    std::string name;
    DataType type;
    ValueVariant value;
    
    Variable() = default;
    Variable(const std::string& n, DataType t) : name(n), type(t) {}
    
    template<typename T>
    void set(T val) {
        value = val;
    }
    
    template<typename T>
    T get() const {
        return std::get<T>(value);
    }
};

/**
 * @brief 执行上下文
 */
class ExecutionContext {
public:
    std::unordered_map<std::string, Variable> variables;
    std::unordered_map<std::string, std::string> metadata;
    
    ExecutionContext() = default;
    
    void set_variable(const std::string& name, DataType type, const ValueVariant& value) {
        Variable var(name, type);
        var.value = value;
        variables[name] = std::move(var);
    }
    
    template<typename T>
    T get(const std::string& name) const {
        auto it = variables.find(name);
        if (it == variables.end()) {
            throw std::runtime_error("Variable not found: " + name);
        }
        return it->second.get<T>();
    }
    
    bool has_variable(const std::string& name) const {
        return variables.find(name) != variables.end();
    }
    
    void clear() {
        variables.clear();
    }
};

// ============================================
// 算子参数
// ============================================

/**
 * @brief 参数类型
 */
enum class ArgType {
    VARIABLE,    // 变量引用
    LITERAL,     // 字面量
    EXPRESSION   // 表达式
};

/**
 * @brief 参数定义
 */
struct Arg {
    std::string value;
    ArgType type;
    DataType data_type;
    
    Arg() : type(ArgType::VARIABLE), data_type(DataType::UNKNOWN) {}
    Arg(const std::string& v, ArgType t, DataType dt) 
        : value(v), type(t), data_type(dt) {}
    
    static Arg variable(const std::string& name, DataType type) {
        return Arg(name, ArgType::VARIABLE, type);
    }
    
    static Arg literal(const std::string& val, DataType type) {
        return Arg(val, ArgType::LITERAL, type);
    }
};

/**
 * @brief 算子调用定义
 */
struct OpCall {
    std::string op_name;
    std::string output_var;
    std::vector<Arg> args;
    std::unordered_map<std::string, std::string> options;
    
    OpCall() = default;
    OpCall(const std::string& name) : op_name(name) {}
    
    // 赋值运算符重载，支持链式赋值
    OpCall& operator=(const OpCall& other) {
        if (this != &other) {
            op_name = other.op_name;
            output_var = other.output_var;
            args = other.args;
            options = other.options;
        }
        return *this;
    }
    
    // 便利工厂方法
    static OpCall create(const std::string& name) {
        return OpCall(name);
    }
};

// 辅助结构体，用于链式配置
struct OpCallBuilder {
    OpCall op;
    
    OpCallBuilder(const std::string& name) : op(name) {}
    
    OpCallBuilder& output(const std::string& var) {
        op.output_var = var;
        return *this;
    }
    
    OpCallBuilder& arg(const Arg& argument) {
        op.args.push_back(argument);
        return *this;
    }
    
    OpCallBuilder& args(const std::vector<Arg>& arguments) {
        op.args = arguments;
        return *this;
    }
    
    OpCall build() {
        return op;
    }
};

// ============================================
// 管道配置
// ============================================

/**
 * @brief 管道配置
 */
struct PipelineConfig {
    std::string name;
    std::vector<OpCall> steps;
    
    // 输入输出定义
    struct IOField {
        std::string name;
        DataType type;
        bool required;
    };
    
    std::vector<IOField> inputs;
    std::vector<IOField> outputs;
    std::vector<IOField> variables;
    
    // 哈希指纹（用于缓存）
    std::string fingerprint;
    
    /**
     * @brief 生成配置指纹
     */
    std::string compute_fingerprint();
};

} // namespace turbograph

#endif // TURBOGRAPH_TYPES_HPP
