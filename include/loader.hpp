#ifndef TURBOGRAPH_LOADER_HPP
#define TURBOGRAPH_LOADER_HPP

#include "compiler.hpp"
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>

namespace turbograph {

// ============================================
// 动态库加载器
// ============================================

/**
 * @brief 动态库加载器
 * 封装dlopen/dlsym等操作
 */
class DllLoader {
public:
    DllLoader() = default;
    ~DllLoader() { unload(); }
    
    // 禁止拷贝
    DllLoader(const DllLoader&) = delete;
    DllLoader& operator=(const DllLoader&) = delete;
    
    // 允许移动
    DllLoader(DllLoader&& other) noexcept 
        : handle_(other.handle_), path_(std::move(other.path_)) {
        other.handle_ = nullptr;
    }
    
    DllLoader& operator=(DllLoader&& other) noexcept {
        if (this != &other) {
            unload();
            handle_ = other.handle_;
            path_ = std::move(other.path_);
            other.handle_ = nullptr;
        }
        return *this;
    }
    
    /**
     * @brief 加载动态库
     * @param path 动态库路径
     * @return 是否成功
     */
    bool load(const std::string& path);
    
    /**
     * @brief 卸载动态库
     */
    void unload();
    
    /**
     * @brief 检查是否已加载
     */
    bool is_loaded() const;
    
    /**
     * @brief 获取符号地址
     * @param name 符号名称
     * @return 符号地址，失败返回nullptr
     */
    void* get_symbol(const std::string& name);
    
    /**
     * @brief 获取加载路径
     */
    const std::string& path() const;
    
private:
    void* handle_ = nullptr;
    std::string path_;
};

// ============================================
// 管道加载器
// ============================================

/**
 * @brief 管道加载器
 * 负责加载和管理已编译的管道SO
 */
class PipelineLoader {
public:
    using ExecuteFunc = bool(*)(void*, void*);
    
    PipelineLoader();
    ~PipelineLoader();
    
    /**
     * @brief 加载管道
     * @param fingerprint 管道指纹
     * @param so_path SO文件路径
     * @return 是否成功
     */
    bool load(const std::string& fingerprint, const std::string& so_path);
    
    /**
     * @brief 卸载管道
     */
    void unload(const std::string& fingerprint);
    
    /**
     * @brief 卸载所有
     */
    void unload_all();
    
    /**
     * @brief 检查是否已加载
     */
    bool is_loaded(const std::string& fingerprint) const;
    
    /**
     * @brief 获取执行函数
     */
    ExecuteFunc get_function(const std::string& fingerprint);
    
    /**
     * @brief 获取管道名称
     */
    const std::string& get_name(const std::string& fingerprint) const;
    
    /**
     * @brief 执行管道
     */
    bool execute(const std::string& fingerprint,
                void* input_data, void* output_data);
    
    /**
     * @brief 获取已加载数量
     */
    size_t loaded_count() const;
    
private:
    std::unordered_map<std::string, DllLoader> loaders_;
    std::unordered_map<std::string, ExecuteFunc> functions_;
    std::unordered_map<std::string, std::string> names_;
};

// ============================================
// 加载管理器
// ============================================

/**
 * @brief 加载管理器
 * 单例模式，管理全局管道加载
 */
class LoadManager {
public:
    static LoadManager& instance();
    
    /**
     * @brief 加载管道
     */
    bool load_pipeline(const PipelineConfig& config);
    
    /**
     * @brief 执行管道
     */
    bool execute(const PipelineConfig& config,
                void* input_data, void* output_data);
    
    /**
     * @brief 卸载管道
     */
    void unload_pipeline(const std::string& fingerprint);
    
    /**
     * @brief 卸载所有
     */
    void unload_all();
    
    /**
     * @brief 检查是否已加载
     */
    bool is_loaded(const PipelineConfig& config) const;
    
    /**
     * @brief 设置缓存目录
     */
    void set_cache_dir(const std::string& dir);
    
    /**
     * @brief 设置包含目录
     */
    void set_include_dir(const std::string& dir);
    
    /**
     * @brief 获取已加载数量
     */
    size_t loaded_count() const;
    
private:
    LoadManager() = default;
    
    PipelineLoader loader_;
    std::string cache_dir_ = "./generated";
    std::string include_dir_ = ".";
};

} // namespace turbograph

#endif // TURBOGRAPH_LOADER_HPP
