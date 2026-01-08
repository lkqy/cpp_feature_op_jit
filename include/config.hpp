#ifndef TURBOGRAPH_CONFIG_HPP
#define TURBOGRAPH_CONFIG_HPP

#include "types.hpp"
#include <json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace turbograph {

// ============================================
// 配置解析器接口
// ============================================

/**
 * @brief 配置解析器基类
 */
class IConfigParser {
public:
    virtual ~IConfigParser() = default;
    
    /**
     * @brief 解析配置文件
     * @param config_path 配置文件路径
     * @return 解析后的管道配置
     */
    virtual PipelineConfig parse(const std::string& config_path) = 0;
    
    /**
     * @brief 从JSON字符串解析
     * @param json_str JSON字符串
     * @return 解析后的管道配置
     */
    virtual PipelineConfig parse_string(const std::string& json_str) = 0;
    
    /**
     * @brief 验证配置有效性
     * @param config 配置
     * @return 验证是否通过
     */
    virtual bool validate(const PipelineConfig& config) = 0;
};

/**
 * @brief JSON配置解析器实现
 */
class JsonConfigParser : public IConfigParser {
public:
    JsonConfigParser() = default;
    
    PipelineConfig parse(const std::string& config_path) override;
    PipelineConfig parse_string(const std::string& json_str) override;
    bool validate(const PipelineConfig& config) override;
    
private:
    /**
     * @brief 解析IO字段数组
     */
    std::vector<PipelineConfig::IOField> parse_io_fields(
        const nlohmann::json& arr, 
        const std::string& field_name);
    
    /**
     * @brief 解析算子步骤
     */
    std::vector<OpCall> parse_steps(const nlohmann::json& steps);
    
    /**
     * @brief 解析参数
     */
    std::vector<Arg> parse_args(const nlohmann::json& args);
    
    /**
     * @brief 解析单个参数
     */
    Arg parse_arg(const nlohmann::json& arg);
    
    /**
     * @brief 推断参数类型
     */
    DataType infer_arg_type(const nlohmann::json& arg, const PipelineConfig& config);
};

// ============================================
// 配置文件生成器
// ============================================

/**
 * @brief 配置生成器
 */
class ConfigGenerator {
public:
    /**
     * @brief 生成JSON配置字符串
     * @param config 管道配置
     * @return JSON字符串
     */
    static std::string generate_json(const PipelineConfig& config);
    
    /**
     * @brief 保存配置到文件
     * @param config 管道配置
     * @param path 输出路径
     * @return 是否成功
     */
    static bool save_to_file(const PipelineConfig& config, const std::string& path);
};

// ============================================
// 注册表
// ============================================

/**
 * @brief 算子注册表
 */
class OpRegistry {
public:
    using Creator = std::function<OpCall(const std::vector<Arg>&, const std::unordered_map<std::string, std::string>&)>;
    
    static OpRegistry& instance();
    
    /**
     * @brief 注册算子
     * @param name 算子名称
     * @param creator 创建函数
     */
    void register_op(const std::string& name, Creator creator);
    
    /**
     * @brief 获取算子创建函数
     */
    const Creator* get_creator(const std::string& name) const;
    
    /**
     * @brief 检查算子是否已注册
     */
    bool has_op(const std::string& name) const;
    
    /**
     * @brief 获取所有已注册的算子名称
     */
    std::vector<std::string> list_ops() const;
    
private:
    OpRegistry() = default;
    std::unordered_map<std::string, Creator> registry_;
};

// ============================================
// 宏辅助
// ============================================

/**
 * @brief 自动注册算子
 */
#define REGISTER_OP(OP_NAME, CREATOR_FUNC) \
    namespace { \
        struct OP_NAME##_registrar { \
            OP_NAME##_registrar() { \
                OpRegistry::instance().register_op(#OP_NAME, CREATOR_FUNC); \
            } \
        }; \
        static OP_NAME##_registrar OP_NAME##_instance; \
    }

} // namespace turbograph

#endif // TURBOGRAPH_CONFIG_HPP
