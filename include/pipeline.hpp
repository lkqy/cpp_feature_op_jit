#ifndef TURBOGRAPH_PIPELINE_HPP
#define TURBOGRAPH_PIPELINE_HPP

#include "config.hpp"
#include "types.hpp"
#include "code_generator.hpp"
#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <atomic>

namespace turbograph {

// ============================================
// 管道执行器接口
// ============================================

/**
 * @brief 管道执行器接口
 */
class IPipelineExecutor {
public:
    virtual ~IPipelineExecutor() = default;
    
    /**
     * @brief 执行管道
     * @param context 执行上下文
     * @return 执行是否成功
     */
    virtual bool execute(ExecutionContext& context) = 0;
    
    /**
     * @brief 获取管道名称
     */
    virtual const std::string& name() const = 0;
    
    /**
     * @brief 获取配置指纹
     */
    virtual const std::string& fingerprint() const = 0;
    
    /**
     * @brief 检查是否需要重新编译
     */
    virtual bool needs_recompile() const = 0;
};

// ============================================
// 解释执行器（基线性能对比）
// ============================================

/**
 * @brief 解释执行器
 * 使用反射和虚函数调用，逐条解释执行配置中的算子
 */
class InterpreterExecutor : public IPipelineExecutor {
public:
    explicit InterpreterExecutor(const PipelineConfig& config);
    
    bool execute(ExecutionContext& context) override;
    const std::string& name() const override { return config_.name; }
    const std::string& fingerprint() const override { return config_.fingerprint; }
    bool needs_recompile() const override { return false; }
    
private:
    PipelineConfig config_;
    
    /**
     * @brief 执行单个算子
     */
    bool execute_op(const OpCall& op, ExecutionContext& ctx);
    
    /**
     * @brief 获取参数值
     */
    ValueVariant get_arg_value(const Arg& arg, const ExecutionContext& ctx);
};

// ============================================
// JIT执行器（动态代码生成）
// ============================================

/**
 * @brief JIT编译执行器
 * 动态生成C++代码，编译为SO后加载执行
 */
class JITExecutor : public IPipelineExecutor {
public:
    explicit JITExecutor(const PipelineConfig& config);
    ~JITExecutor() override;
    
    bool execute(ExecutionContext& context) override;
    const std::string& name() const override { return config_.name; }
    const std::string& fingerprint() const override { return fingerprint_; }
    bool needs_recompile() const override;
    
    /**
     * @brief 强制重新编译
     */
    void recompile();
    
    /**
     * @brief 设置代码生成选项
     */
    void set_options(const CodeGenOptions& options);
    
private:
    PipelineConfig config_;
    std::string fingerprint_;
    void* so_handle_ = nullptr;
    bool needs_recompile_ = true;
    
    // 编译选项
    CodeGenOptions gen_options_;
    
    // 函数指针类型
    using ExecuteFunc = bool(*)(void*, void*);
    ExecuteFunc execute_func_ = nullptr;
    
    /**
     * @brief 检查缓存
     */
    bool check_cache();
    
    /**
     * @brief 加载SO
     */
    bool load_so();
    
    /**
     * @brief 卸载SO
     */
    void unload_so();
};

// ============================================
// 管道管理器
// ============================================

/**
 * @brief 管道模式
 */
enum class PipelineMode {
    INTERPRETER,  // 解释执行
    JIT,          // JIT编译执行
    AUTO          // 自动选择（首次解释，后续JIT）
};

/**
 * @brief 管道管理器
 * 负责创建和管理管道执行器
 */
class PipelineManager {
public:
    static PipelineManager& instance();
    
    /**
     * @brief 创建解释执行器
     */
    std::unique_ptr<IPipelineExecutor> create_interpreter(const PipelineConfig& config);
    
    /**
     * @brief 创建JIT执行器
     */
    std::unique_ptr<IPipelineExecutor> create_jit(const PipelineConfig& config);
    
    /**
     * @brief 创建执行器
     * @param config 管道配置
     * @param mode 执行模式
     */
    std::unique_ptr<IPipelineExecutor> create(const PipelineConfig& config, PipelineMode mode);
    
    /**
     * @brief 从配置文件创建执行器
     */
    std::unique_ptr<IPipelineExecutor> create_from_file(
        const std::string& config_path, 
        PipelineMode mode);
    
    /**
     * @brief 设置JIT选项
     */
    void set_jit_options(const CodeGenOptions& options);
    
    /**
     * @brief 设置缓存目录
     */
    void set_cache_dir(const std::string& dir);
    
    /**
     * @brief 清除缓存
     */
    void clear_cache();
    
    /**
     * @brief 获取缓存目录
     */
    const std::string& cache_dir() const { return cache_dir_; }
    
private:
    PipelineManager() = default;
    
    CodeGenOptions jit_options_;
    std::string cache_dir_ = "./generated";
    std::unordered_map<std::string, void*> loaded_handles_;
    
    std::string get_cache_path(const std::string& fingerprint) const;
    std::string get_source_path(const std::string& fingerprint) const;
};

// ============================================
// 工具函数
// ============================================

/**
 * @brief 创建默认配置
 */
PipelineConfig create_demo_config();

/**
 * @brief 创建测试数据
 */
ExecutionContext create_test_context();

} // namespace turbograph

#endif // TURBOGRAPH_PIPELINE_HPP
