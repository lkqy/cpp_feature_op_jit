#include "loader.hpp"
#include <dlfcn.h>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <cctype>

namespace turbograph {

// ============================================
// 辅助函数：生成合法的C++标识符（与code_generator保持一致）
// ============================================

static std::string make_valid_identifier(const std::string& str) {
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
// 动态库加载器实现
// ============================================

bool DllLoader::load(const std::string& path) {
    // 如果已加载，先卸载
    if (handle_) {
        unload();
    }
    
    // 打开动态库
    handle_ = dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (!handle_) {
        std::cerr << "Failed to load library: " << dlerror() << std::endl;
        return false;
    }
    
    path_ = path;
    return true;
}

void DllLoader::unload() {
    if (handle_) {
        dlclose(handle_);
        handle_ = nullptr;
    }
    path_.clear();
}

bool DllLoader::is_loaded() const {
    return handle_ != nullptr;
}

void* DllLoader::get_symbol(const std::string& name) {
    if (!handle_) {
        std::cerr << "Library not loaded" << std::endl;
        return nullptr;
    }
    
    dlerror();  // 清除错误
    void* symbol = dlsym(handle_, name.c_str());
    const char* error = dlerror();
    
    if (error) {
        std::cerr << "Symbol not found: " << name << " - " << error << std::endl;
        return nullptr;
    }
    
    return symbol;
}

const std::string& DllLoader::path() const {
    return path_;
}

// ============================================
// 管道加载器实现
// ============================================

PipelineLoader::PipelineLoader() = default;

PipelineLoader::~PipelineLoader() {
    unload_all();
}

bool PipelineLoader::load(const std::string& fingerprint, const std::string& so_path) {
    auto it = loaders_.find(fingerprint);
    if (it != loaders_.end()) {
        // 已加载，检查路径是否相同
        if (it->second.path() == so_path) {
            return true;
        }
        // 路径不同，重新加载
        it->second.unload();
    }
    
    DllLoader loader;
    if (!loader.load(so_path)) {
        std::cerr << "Failed to load pipeline: " << fingerprint << std::endl;
        return false;
    }
    
    // 获取函数指针（使用转换后的标识符）
    std::string func_name = "pipeline_execute_" + make_valid_identifier(fingerprint);
    auto exec_func = loader.get_symbol(func_name.c_str());
    if (!exec_func) {
        // 尝试原始名称
        exec_func = loader.get_symbol(("pipeline_execute_" + fingerprint).c_str());
    }
    if (!exec_func) {
        // 尝试通用名称
        exec_func = loader.get_symbol("pipeline_execute");
    }
    
    if (!exec_func) {
        std::cerr << "Failed to find execute function for: " << fingerprint << std::endl;
        return false;
    }
    
    // 获取元数据
    const char* name = nullptr;
    auto name_func = loader.get_symbol("pipeline_name");
    if (name_func) {
        // 获取函数指针
        using NameFunc = const char*(*)();
        auto name_ptr = reinterpret_cast<NameFunc>(name_func);
        if (name_ptr) {
            name = name_ptr();
        }
    }
    
    // 存储加载器
    loaders_[fingerprint] = std::move(loader);
    
    // 存储函数指针
    auto func_ptr = reinterpret_cast<ExecuteFunc>(exec_func);
    functions_[fingerprint] = func_ptr;
    
    // 存储名称
    names_[fingerprint] = name ? std::string(name) : fingerprint;
    
    return true;
}

void PipelineLoader::unload(const std::string& fingerprint) {
    loaders_.erase(fingerprint);
    functions_.erase(fingerprint);
    names_.erase(fingerprint);
}

void PipelineLoader::unload_all() {
    loaders_.clear();
    functions_.clear();
    names_.clear();
}

bool PipelineLoader::is_loaded(const std::string& fingerprint) const {
    return functions_.find(fingerprint) != functions_.end();
}

PipelineLoader::ExecuteFunc PipelineLoader::get_function(const std::string& fingerprint) {
    auto it = functions_.find(fingerprint);
    if (it != functions_.end()) {
        return it->second;
    }
    return nullptr;
}

const std::string& PipelineLoader::get_name(const std::string& fingerprint) const {
    static const std::string empty;
    auto it = names_.find(fingerprint);
    if (it != names_.end()) {
        return it->second;
    }
    return empty;
}

bool PipelineLoader::execute(const std::string& fingerprint, 
                            void* input_data, void* output_data) {
    auto func = get_function(fingerprint);
    if (!func) {
        std::cerr << "Pipeline not loaded: " << fingerprint << std::endl;
        return false;
    }
    
    return func(input_data, output_data);
}

size_t PipelineLoader::loaded_count() const {
    return functions_.size();
}

// ============================================
// 加载管理器实现
// ============================================

LoadManager& LoadManager::instance() {
    static LoadManager manager;
    return manager;
}

bool LoadManager::load_pipeline(const PipelineConfig& config) {
    std::string fingerprint = config.fingerprint;
    if (fingerprint.empty()) {
        // 指纹为空时不使用缓存，直接编译
        fingerprint = "dynamic_" + std::to_string(std::hash<std::string>{}(config.name));
    }
    
    // 检查是否已加载
    if (loader_.is_loaded(fingerprint)) {
        return true;
    }
    
    // 获取SO路径
    auto so_path = JITCompiler::instance().get_so_path(fingerprint);
    if (!so_path.has_value()) {
        // 需要编译
        CodeGenOptions gen_opts;
        gen_opts.output_dir = cache_dir_;
        gen_opts.verbose = false;
        
        CompileOptions comp_opts;
        comp_opts.include_dir = "/workspace/turbograph_jit/include";
        comp_opts.keep_source = true;
        
        if (!JITCompiler::instance().compile(config, gen_opts, comp_opts)) {
            std::cerr << "Failed to compile pipeline: " << fingerprint << std::endl;
            return false;
        }
        
        so_path = JITCompiler::instance().get_so_path(fingerprint);
        if (!so_path.has_value()) {
            std::cerr << "Failed to get SO path: " << fingerprint << std::endl;
            return false;
        }
    }
    
    // 加载
    return loader_.load(fingerprint, so_path.value());
}

bool LoadManager::execute(const PipelineConfig& config,
                         void* input_data, void* output_data) {
    std::string fingerprint = config.fingerprint;
    if (fingerprint.empty()) {
        const_cast<PipelineConfig&>(config).compute_fingerprint();
        fingerprint = config.fingerprint;
    }
    
    if (!loader_.is_loaded(fingerprint)) {
        if (!load_pipeline(config)) {
            return false;
        }
    }
    
    return loader_.execute(fingerprint, input_data, output_data);
}

void LoadManager::unload_pipeline(const std::string& fingerprint) {
    loader_.unload(fingerprint);
}

void LoadManager::unload_all() {
    loader_.unload_all();
}

void LoadManager::set_cache_dir(const std::string& dir) {
    cache_dir_ = dir;
    JITCompiler::instance().set_cache_dir(dir);
}

void LoadManager::set_include_dir(const std::string& dir) {
    include_dir_ = dir;
}

bool LoadManager::is_loaded(const PipelineConfig& config) const {
    std::string fingerprint = config.fingerprint;
    if (fingerprint.empty()) {
        return false;
    }
    return loader_.is_loaded(fingerprint);
}

size_t LoadManager::loaded_count() const {
    return loader_.loaded_count();
}

} // namespace turbograph
