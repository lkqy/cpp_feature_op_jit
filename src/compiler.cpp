#include "compiler.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sstream>

namespace turbograph {

// ============================================
// 编译器封装实现
// ============================================

bool Compiler::compile(const std::string& source_path, const std::string& output_path, 
                       const CompileOptions& options) {
    // 检查源文件是否存在
    if (!file_exists(source_path)) {
        std::cerr << "Source file not found: " << source_path << std::endl;
        return false;
    }
    
    // 构建编译命令
    std::string cmd = build_compile_command(source_path, output_path, options);
    
    if (options.verbose) {
        std::cout << "Compiling: " << cmd << std::endl;
    }
    
    // 执行编译
    int result = execute_command(cmd);
    
    if (result != 0) {
        std::cerr << "Compilation failed with exit code: " << result << std::endl;
        return false;
    }
    
    // 验证输出文件
    if (!file_exists(output_path)) {
        std::cerr << "Output file was not created: " << output_path << std::endl;
        return false;
    }
    
    return true;
}

bool Compiler::compile_from_string(const std::string& source_code, 
                                   const std::string& output_path,
                                   const CompileOptions& options) {
    // 创建临时源文件
    std::string temp_path = output_path + ".cpp";
    
    std::ofstream file(temp_path);
    if (!file.is_open()) {
        std::cerr << "Failed to create temp source file: " << temp_path << std::endl;
        return false;
    }
    
    file << source_code;
    file.close();
    
    // 编译
    bool result = compile(temp_path, output_path, options);
    
    // 清理临时文件（可选，保留源文件便于调试）
    if (!options.keep_source) {
        std::remove(temp_path.c_str());
    }
    
    return result;
}

std::string Compiler::build_compile_command(const std::string& source_path, 
                                            const std::string& output_path,
                                            const CompileOptions& options) {
    std::ostringstream cmd;
    
    // 编译器
    cmd << options.compiler_path << " ";
    
    // 优化选项
    cmd << "-O3 ";
    
    // 位置无关代码（生成SO必需）
    cmd << "-shared -fPIC ";
    
    // 架构优化
    cmd << "-march=native ";
    
    // C++标准
    cmd << "-std=c++17 ";
    
    // 包含路径
    cmd << "-I" << options.include_dir << " ";
    
    // 额外标志
    if (!options.extra_flags.empty()) {
        cmd << options.extra_flags << " ";
    }
    
    // 警告抑制（可选）
    cmd << "-w ";
    
    // 输入输出
    cmd << source_path << " ";
    cmd << "-o " << output_path;
    
    // 重定向错误输出
    cmd << " 2>&1";
    
    return cmd.str();
}

int Compiler::execute_command(const std::string& cmd) {
    // 使用popen执行命令
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "Failed to execute command: " << cmd << std::endl;
        return -1;
    }
    
    // 读取输出
    char buffer[4096];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    
    // 关闭管道
    int result = pclose(pipe);
    
    // 如果有错误输出，显示它
    if (!output.empty()) {
        if (result != 0) {
            std::cerr << "Compiler output:\n" << output << std::endl;
        }
    }
    
    return result;
}

bool Compiler::file_exists(const std::string& path) {
    struct stat buffer;
    return stat(path.c_str(), &buffer) == 0;
}

long long Compiler::file_size(const std::string& path) {
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0) {
        return -1;
    }
    return buffer.st_size;
}

std::string Compiler::read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

bool Compiler::write_file(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    file << content;
    return true;
}

bool Compiler::create_directory(const std::string& path) {
    // 使用mkdir -p创建目录
    std::string cmd = "mkdir -p " + path;
    return system(cmd.c_str()) == 0;
}

// ============================================
// 编译缓存实现
// ============================================

bool CompilationCache::is_valid(const std::string& fingerprint) const {
    auto it = cache_.find(fingerprint);
    if (it == cache_.end()) {
        return false;
    }
    
    // 检查文件是否存在且有效
    if (!Compiler::file_exists(it->second.so_path)) {
        return false;
    }
    
    // 检查文件修改时间
    long long src_time = Compiler::file_size(it->second.source_path);
    long long so_time = Compiler::file_size(it->second.so_path);
    
    if (src_time < 0 || so_time < 0) {
        return false;
    }
    
    // SO应该比源文件新
    // 这里简化处理，实际应该比较修改时间
    
    return true;
}

void CompilationCache::add(const std::string& fingerprint, 
                          const CacheEntry& entry) {
    cache_[fingerprint] = entry;
}

void CompilationCache::remove(const std::string& fingerprint) {
    cache_.erase(fingerprint);
}

void CompilationCache::clear() {
    cache_.clear();
}

std::optional<CacheEntry> CompilationCache::get(const std::string& fingerprint) const {
    auto it = cache_.find(fingerprint);
    if (it != cache_.end()) {
        return it->second;
    }
    return std::nullopt;
}

size_t CompilationCache::size() const {
    return cache_.size();
}

// ============================================
// JIT编译器实现
// ============================================

JITCompiler& JITCompiler::instance() {
    static JITCompiler compiler;
    return compiler;
}

bool JITCompiler::compile(const PipelineConfig& config, 
                          const CodeGenOptions& gen_options,
                          const CompileOptions& comp_options) {
    // 生成代码
    CodeGenerator generator(config, gen_options);
    std::string code = generator.generate();
    
    // 生成指纹
    std::string fingerprint = config.fingerprint;
    if (fingerprint.empty()) {
        // 需要修改config来计算指纹，但config是const引用
        // 这里使用config的数据重新计算
        std::ostringstream oss;
        oss << config.name << "|";
        for (const auto& step : config.steps) {
            oss << step.op_name << "(";
            for (size_t i = 0; i < step.args.size(); i++) {
                oss << step.args[i].value;
                if (i < step.args.size() - 1) oss << ",";
            }
            oss << ")->" << step.output_var << ";";
        }
        std::hash<std::string> hasher;
        size_t hash = hasher(oss.str());
        std::ostringstream hash_oss;
        hash_oss << std::hex << hash;
        fingerprint = hash_oss.str();
    }
    
    // 确定输出路径
    std::string so_path = get_cache_path(fingerprint);
    std::string source_path = so_path + ".cpp";
    
    // 确保目录存在
    std::string dir = so_path.substr(0, so_path.find_last_of('/'));
    if (!dir.empty() && !Compiler::create_directory(dir)) {
        std::cerr << "Failed to create cache directory: " << dir << std::endl;
        return false;
    }
    
    // 保存源文件
    if (!Compiler::write_file(source_path, code)) {
        std::cerr << "Failed to write source file: " << source_path << std::endl;
        return false;
    }
    
    // 编译
    if (!Compiler::compile(source_path, so_path, comp_options)) {
        std::cerr << "Compilation failed for: " << fingerprint << std::endl;
        // 只有在不保留源文件时才删除
        if (!comp_options.keep_source) {
            std::remove(source_path.c_str());
        }
        return false;
    }
    
    // 添加到缓存
    CacheEntry entry;
    entry.fingerprint = fingerprint;
    entry.source_path = source_path;
    entry.so_path = so_path;
    entry.compile_time = std::chrono::steady_clock::now();
    
    cache_.add(fingerprint, entry);
    
    if (gen_options.verbose) {
        std::cout << "Compiled: " << fingerprint << " -> " << so_path << std::endl;
    }
    
    return true;
}

std::optional<std::string> JITCompiler::get_so_path(const std::string& fingerprint) const {
    auto entry = cache_.get(fingerprint);
    if (entry.has_value() && Compiler::file_exists(entry->so_path)) {
        return entry->so_path;
    }
    return std::nullopt;
}

std::string JITCompiler::get_cache_path(const std::string& fingerprint) const {
    return cache_dir_ + "/libpipeline_" + fingerprint + ".so";
}

void JITCompiler::set_cache_dir(const std::string& dir) {
    cache_dir_ = dir;
    Compiler::create_directory(cache_dir_);
}

void JITCompiler::clear_cache() {
    // 清理缓存
    cache_.clear();
    
    // 可选：删除缓存目录中的文件
    // 这里简化处理，只清理内存缓存
}

} // namespace turbograph
