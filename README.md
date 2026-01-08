
## 概述

Feature-Graph-JIT 是一个高性能的动态算子执行引擎，通过将配置驱动的管道转换为动态编译的原生代码，显著提升执行性能。

### 核心特性

- **配置驱动**：通过JSON配置定义算子执行流程，无需硬编码
- **动态编译**：运行时生成优化后的C++代码，编译为动态链接库
- **极致性能**：相比解释执行，性能提升可达10-100倍
- **灵活扩展**：支持自定义算子和类型

## 架构设计

### 执行模式

1. **解释执行模式 (Interpreter Mode)**
   - 通过反射和虚函数调用逐条解释执行配置中的算子
   - 实现简单，但性能较低
   - 适用于开发调试和小规模计算

2. **JIT编译模式 (JIT Mode)**
   - 动态生成C++代码，编译为SO后加载执行
   - 编译器可以进行深度优化（内联、向量化等）
   - 首次执行有编译开销，后续执行直接使用缓存的SO
   - 适用于生产环境和高频计算

### 系统架构

```
+------------------+     +------------------+     +------------------+
|   配置文件        | --> |   配置解析器      | --> |   管道配置       |
| (JSON/YAML)      |     | ConfigParser    |     | PipelineConfig  |
+------------------+     +------------------+     +--------+---------+
                                                           |
                                                           v
+------------------+     +------------------+     +------------------+
|   动态编译        | <-- |   代码生成器      | <-- |   算子库         |
|   编译器          |     | CodeGenerator   |     | Ops Library     |
+------------------+     +--------+---------+     +--------+---------+
                                  |                         |
                                  v                         |
                         +--------+---------+               |
                         |   生成的C++代码  | <--------------+
                         +--------+---------+
                                  |
                                  v
                         +--------+---------+
                         |   g++ -O3 编译   |
                         +--------+---------+
                                  |
                                  v
                         +--------+---------+
                         |   libXXX.so     |
                         +--------+---------+
                                  |
                                  v
                         +--------+---------+
                         |   SO加载器       |
                         |   DllLoader    |
                         +--------+---------+
                                  |
                                  v
                         +--------+---------+
                         |   管道执行       |
                         |   Pipeline     |
                         +----------------+
```

## 快速开始

### 环境要求

- C++17 编译器 (g++ 9+ 或 clang++ 10+)
- CMake 3.15+
- nlohmann/json (头文件库)

### 编译项目

```bash
# 创建构建目录
mkdir build && cd build

# 配置项目
cmake ..

# 编译
make -j4

# 运行测试
ctest

# 运行性能测试
./benchmark
```

### 使用示例

#### 1. 创建配置文件

```json
{
  "name": "my_strategy",
  "inputs": [
    {"name": "price_a", "type": "double", "required": true},
    {"name": "price_b", "type": "double", "required": true},
    {"name": "volume", "type": "int32", "required": true}
  ],
  "variables": [
    {"name": "temp_sum", "type": "double", "required": false},
    {"name": "final_score", "type": "double", "required": false}
  ],
  "steps": [
    {
      "op": "add",
      "args": ["$price_a", "$price_b"],
      "output": "temp_sum"
    },
    {
      "op": "mul",
      "args": ["$temp_sum", "$volume"],
      "output": "final_score"
    }
  ],
  "outputs": [
    {"name": "final_score", "type": "double", "required": true}
  ]
}
```

#### 2. 编写代码

```cpp
#include "pipeline.hpp"
#include "config.hpp"

int main() {
    using namespace turbograph;
    
    // 解析配置
    JsonConfigParser parser;
    auto config = parser.parse("strategy.json");
    
    // 创建JIT执行器
    auto executor = PipelineManager::instance().create(config, PipelineMode::JIT);
    
    // 创建执行上下文
    ExecutionContext ctx;
    ctx.set_variable("price_a", DataType::DOUBLE, 100.5);
    ctx.set_variable("price_b", DataType::DOUBLE, 50.25);
    ctx.set_variable("volume", DataType::INT32, 10);
    
    // 执行管道
    if (executor->execute(ctx)) {
        double result = ctx.get<double>("final_score");
        std::cout << "Result: " << result << std::endl;
    }
    
    return 0;
}
```

## 内置算子

### 数学算子

| 算子 | 说明 | 示例 |
|------|------|------|
| `add` | 加法 | `add(a, b)` |
| `sub` | 减法 | `sub(a, b)` |
| `mul` | 乘法 | `mul(a, b)` |
| `div` | 除法 | `div(a, b)` |
| `abs` | 绝对值 | `abs(a)` |
| `sqrt` | 平方根 | `sqrt(a)` |
| `square` | 平方 | `square(a)` |
| `max` | 最大值 | `max(a, b)` |
| `min` | 最小值 | `min(a, b)` |

### 逻辑算子

| 算子 | 说明 | 示例 |
|------|------|------|
| `if_else` | 条件选择 | `if_else(condition, true_val, false_val)` |

### 类型转换算子

| 算子 | 说明 | 示例 |
|------|------|------|
| `direct_output_int32` | 转换为int32 | `direct_output_int32(a)` |
| `direct_output_int64` | 转换为int64 | `direct_output_int64(a)` |
| `direct_output_double` | 转换为double | `direct_output_double(a)` |

### 扩展算子

| 算子 | 说明 | 示例 |
|------|------|------|
| `get_sign` | 获取符号 | `get_sign(a)` |
| `price_diff` | 价格差 | `price_diff(discount, original)` |
| `avg_avg_log` | 对数分段 | `avg_avg_log(value, 1000, 15000, 5000, 250000)` |
| `percent` | 百分比 | `percent(part, total)` |
| `floor` | 向下取整 | `floor(a)` |
| `ceil` | 向上取整 | `ceil(a)` |

## 性能优化

### 编译选项

```cpp
CodeGenOptions options;
options.enable_inline = true;      // 启用内联优化
options.enable_vectorize = true;   // 启用向量化
options.use_fast_math = true;      // 启用快速数学
options.compiler_flags = "-O3 -march=native -std=c++17";
options.use_cache = true;          // 启用缓存

PipelineManager::instance().set_jit_options(options);
```

### 缓存管理

```cpp
// 设置缓存目录
PipelineManager::instance().set_cache_dir("./cache");

// 清除缓存
PipelineManager::instance().clear_cache();
```

## 扩展开发

### 自定义算子

1. 在 `include/ops.hpp` 中添加新算子：

```cpp
namespace turbograph::ops {
    inline double my_op(double a, double b) {
        return a * b + 1.0;
    }
}
```

2. 在代码生成器中注册算子映射（在 `src/code_generator.cpp` 中）：

```cpp
std::string CodeGenerator::map_operator_name(const std::string& op_name) {
    if (op_name == "my_op") {
        return "::turbograph::ops::my_op";
    }
    return op_name;
}
```

### 自定义类型

1. 在 `include/types.hpp` 中扩展 `DataType` 枚举
2. 更新类型转换工具函数
3. 在代码生成器中添加对应的C++类型处理

## 性能测试结果

典型测试环境：Intel i7-10700, 32GB RAM, g++ -O3

| 测试场景 | 解释执行 | JIT执行 | 加速比 |
|---------|---------|---------|--------|
| 5个算子 | 15ms | 2ms | 7.5x |
| 20个算子 | 60ms | 5ms | 12x |
| 50个算子 | 150ms | 10ms | 15x |
| 100个算子 | 300ms | 18ms | 16x |

*测试条件：100000次迭代，迭代次数越多，JIT模式优势越明显*

## 项目结构

```
turbograph_jit/
├── CMakeLists.txt          # CMake配置
├── README.md               # 项目说明
├── include/
│   ├── ops.hpp            # 算子库
│   ├── types.hpp          # 类型系统
│   ├── config.hpp         # 配置解析
│   ├── pipeline.hpp       # 管道接口
│   ├── code_generator.hpp # 代码生成器
│   ├── compiler.hpp       # 编译器封装
│   └── loader.hpp         # SO加载器
├── src/
│   ├── ops.cpp            # 算子实现
│   ├── config_parser.cpp  # 配置解析实现
│   ├── code_generator.cpp # 代码生成实现
│   ├── compiler.cpp       # 编译器实现
│   ├── loader.cpp         # 加载器实现
│   └── pipeline.cpp       # 管道管理实现
├── examples/
│   ├── sample_config.json  # 示例配置
│   └── benchmark.cpp      # 性能测试
└── tests/
    └── test_runner.cpp    # 单元测试
```
