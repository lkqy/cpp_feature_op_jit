#ifndef TURBOGRAPH_COMPILER_HPP
#define TURBOGRAPH_COMPILER_HPP

#include "config.hpp"
#include "code_generator.hpp"
#include <string>
#include <chrono>
#include <unordered_map>
#include <optional>
#include <iostream>

namespace turbograph {

// ============================================
// 编译选项
// ============================================

/**
 * @brief 编译选项
 */
struct CompileOptions {
    std::string compiler_path = "g++";
    std::string include_dir = ".";
    std::string extra_flags = "";
    bool verbose = false;
    bool keep_source = true;  // 是否保留源文件
};

// ============================================
// 编译器封装
// ============================================

/**
 * @brief 编译器封装
 * 封装系统编译器调用
 */
class Compiler {
public:
    /**
     * @brief 编译源文件
     * @param source_path 源文件路径
     * @param output_path 输出SO路径
     * @param options 编译选项
     * @return 是否成功
     */
    static bool compile(const std::string& source_path, 
                       const std::string& output_path,
                       const CompileOptions& options = {});
    
    /**
     * @brief 从字符串编译
     * @param source_code 源代码
     * @param output_path 输出SO路径
     * @param options 编译选项
     * @return 是否成功
     */
    static bool compile_from_string(const std::string& source_code,
                                   const std::string& output_path,
                                   const CompileOptions& options = {});
    
    /**
     * @brief 构建编译命令
     */
    static std::string build_compile_command(const std::string& source_path,
                                             const std::string& output_path,
                                             const CompileOptions& options);
    
    /**
     * @brief 执行命令
     */
    static int execute_command(const std::string& cmd);
    
    /**
     * @brief 检查文件是否存在
     */
    static bool file_exists(const std::string& path);
    
    /**
     * @brief 获取文件大小
     */
    static long long file_size(const std::string& path);
    
    /**
     * @brief 读取文件
     */
    static std::string read_file(const std::string& path);
    
    /**
     * @brief 写入文件
     */
    static bool write_file(const std::string& path, const std::string& content);
    
    /**
     * @brief 创建目录
     */
    static bool create_directory(const std::string& path);
};

// ============================================
// 编译缓存
// ============================================

/**
 * @brief 编译缓存条目
 */
struct CacheEntry {
    std::string fingerprint;
    std::string source_path;
    std::string so_path;
    std::chrono::steady_clock::time_point compile_time;
};

/**
 * @brief 编译缓存
 */
class CompilationCache {
public:
    /**
     * @brief 检查缓存是否有效
     */
    bool is_valid(const std::string& fingerprint) const;
    
    /**
     * @brief 添加缓存
     */
    void add(const std::string& fingerprint, const CacheEntry& entry);
    
    /**
     * @brief 移除缓存
     */
    void remove(const std::string& fingerprint);
    
    /**
     * @brief 清空缓存
     */
    void clear();
    
    /**
     * @brief 获取缓存
     */
    std::optional<CacheEntry> get(const std::string& fingerprint) const;
    
    /**
     * @brief 获取缓存大小
     */
    size_t size() const;
    
private:
    std::unordered_map<std::string, CacheEntry> cache_;
};

// ============================================
// JIT编译器
// ============================================

/**
 * @brief JIT编译器
 * 负责将配置编译为SO
 */
class JITCompiler {
public:
    static JITCompiler& instance();
    
    /**
     * @brief 编译管道配置
     * @param config 管道配置
     * @param gen_options 代码生成选项
     * @param comp_options 编译选项
     * @return 是否成功
     */
    bool compile(const PipelineConfig& config,
                const CodeGenOptions& gen_options = {},
                const CompileOptions& comp_options = {});
    
    /**
     * @brief 获取SO路径
     */
    std::optional<std::string> get_so_path(const std::string& fingerprint) const;
    
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
    JITCompiler() = default;
    
    std::string get_cache_path(const std::string& fingerprint) const;
    
    std::string cache_dir_ = "./generated";
    CompilationCache cache_;
};

} // namespace turbograph

#endif // TURBOGRAPH_COMPILER_HPP
