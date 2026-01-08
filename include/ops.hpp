#ifndef TURBOGRAPH_OPS_HPP
#define TURBOGRAPH_OPS_HPP

#include <cmath>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <type_traits>

namespace turbograph::ops {

// ============================================
// 基础数学算子
// ============================================

/**
 * @brief 获取数值的符号
 * @tparam T 支持数值类型
 * @param value 输入值
 * @return -1, 0, 或 1
 */
template<typename T>
inline int get_sign(T value) {
    if (value < 0) return -1;
    if (value > 0) return 1;
    return 0;
}

/**
 * @brief 计算价格差值
 * @tparam T 支持数值类型
 * @param discount_price 折扣价格
 * @param dprice_ori 原始价格
 * @return 差价
 */
template<typename T>
inline T price_diff(T discount_price, T dprice_ori) {
    if (discount_price == 0) return 0;
    return discount_price - dprice_ori;
}

// ============================================
// 对数分段算子（用于数据分桶）
// ============================================

/**
 * @brief 对数分段映射算子
 * @tparam T 支持数值类型
 * @param origin 原始值
 * @param inter1 第一区间的间隔
 * @param threshold1 第一区间的阈值
 * @param inter2 第二区间的间隔
 * @param threshold2 第二区间的阈值
 * @return 分段映射结果
 */
template<typename T>
inline int64_t avg_avg_log(T origin, 
                           int32_t inter1 = 1000, 
                           int32_t threshold1 = 15000,
                           int32_t inter2 = 5000, 
                           int32_t threshold2 = 250000) {
    if (origin == 0) return 0;
    int64_t ori_abs = static_cast<int64_t>(std::abs(origin));
    int64_t res;

    if (ori_abs <= threshold1) {
        res = ori_abs / inter1 + 1;
        return origin >= 0 ? res : -res;
    }

    int64_t start;
    if (ori_abs <= threshold2) {
        start = threshold1 / inter1 + 1;
        res = start + (ori_abs - threshold1) / inter2 + 1;
        return origin >= 0 ? res : -res;
    }

    start = threshold1 / inter1 + 1 + (threshold2 - threshold1) / inter2 + 1;
    int64_t realLog = ori_abs / inter2;
    res = start + static_cast<int64_t>(std::log(realLog) / std::log(1.5));
    return origin >= 0 ? res : -res;
}

// ============================================
// 类型转换算子
// ============================================

/**
 * @brief 统一输出为int32_t
 * @tparam T 任意输入类型
 * @param value 输入值
 * @return int32_t类型的结果
 */
template<typename T>
inline int32_t direct_output_int32(T value) {
    if constexpr (std::is_same_v<T, int32_t>) {
        return value;
    } else if constexpr (std::is_same_v<T, int64_t>) {
        return static_cast<int32_t>(value);
    } else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>) {
        return static_cast<int32_t>(value);
    } else {
        return static_cast<int32_t>(value);
    }
}

/**
 * @brief 统一输出为int64_t
 * @tparam T 任意输入类型
 * @param value 输入值
 * @return int64_t类型的结果
 */
template<typename T>
inline int64_t direct_output_int64(T value) {
    if constexpr (std::is_same_v<T, int64_t>) {
        return value;
    } else if constexpr (std::is_same_v<T, int32_t>) {
        return static_cast<int64_t>(value);
    } else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>) {
        return static_cast<int64_t>(value);
    } else {
        return static_cast<int64_t>(value);
    }
}

/**
 * @brief 统一输出为double
 * @tparam T 任意输入类型
 * @param value 输入值
 * @return double类型的结果
 */
template<typename T>
inline double direct_output_double(T value) {
    if constexpr (std::is_same_v<T, double>) {
        return value;
    } else if constexpr (std::is_same_v<T, float>) {
        return static_cast<double>(value);
    } else if constexpr (std::is_same_v<T, int32_t> || std::is_same_v<T, int64_t>) {
        return static_cast<double>(value);
    } else {
        return static_cast<double>(value);
    }
}

/**
 * @brief 任意类型转字符串
 * @tparam T 任意可输出类型
 * @param value 输入值
 * @return 字符串表示
 */
template<typename T>
inline std::string direct_output_string(T value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

// ============================================
// 容器操作算子
// ============================================

/**
 * @brief 获取容器的长度
 * @tparam T 支持size()方法的容器类型
 * @param container 容器实例
 * @return 容器大小
 */
template<typename T>
inline size_t len(const T& container) {
    return container.size();
}

/**
 * @brief 将列表转换为分隔符连接的字符串
 * @tparam LIST_TYPE 支持范围for遍历的容器类型
 * @param list 列表容器
 * @param delimiter 分隔符，默认为"|"
 * @return 连接后的字符串
 */
template<typename LIST_TYPE>
inline std::string list_to_string(const LIST_TYPE& list, const std::string& delimiter = "|") {
    if (list.empty()) return "";
    std::ostringstream oss;
    bool first = true;
    for (const auto& item : list) {
        if (!first) oss << delimiter;
        oss << item;
        first = false;
    }
    return oss.str();
}

/**
 * @brief 检查元素是否在列表中（返回存在性）
 * @tparam LIST_TYPE 列表类型
 * @tparam ITEM_TYPE 元素类型
 * @param list 列表
 * @param item_id 要查找的元素
 * @return 1表示存在，0表示不存在
 */
template<typename LIST_TYPE, typename ITEM_TYPE>
inline int catein_list_cross(const LIST_TYPE& list, ITEM_TYPE item_id) {
    if (list.empty()) return 0;
    for (size_t i = 0; i < list.size(); i++) {
        if (list[i] == item_id) {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief 统计元素在列表中出现的次数
 * @tparam LIST_TYPE 列表类型
 * @tparam ITEM_TYPE 元素类型
 * @param list 列表
 * @param item_id 要统计的元素
 * @return 出现的次数
 */
template<typename LIST_TYPE, typename ITEM_TYPE>
inline int catein_list_cross_count(const LIST_TYPE& list, ITEM_TYPE item_id) {
    if (list.empty()) return 0;
    int count = 0;
    for (size_t i = 0; i < list.size(); i++) {
        if (list[i] == item_id) {
            count++;
        }
    }
    return count;
}

// ============================================
// 扩展算子（为了演示更复杂的场景）
// ============================================

/**
 * @brief 加法算子
 */
template<typename T>
inline T add_op(T a, T b) {
    return a + b;
}

/**
 * @brief 减法算子
 */
template<typename T>
inline T sub_op(T a, T b) {
    return a - b;
}

/**
 * @brief 乘法算子
 */
template<typename T>
inline T mul_op(T a, T b) {
    return a * b;
}

/**
 * @brief 除法算子
 */
template<typename T>
inline T div_op(T a, T b) {
    if (b == 0) return 0;
    return a / b;
}

/**
 * @brief 条件选择算子
 */
template<typename T>
inline T if_else(bool condition, T true_val, T false_val) {
    return condition ? true_val : false_val;
}

/**
 * @brief 最大值算子
 */
template<typename T>
inline T max_op(T a, T b) {
    return a > b ? a : b;
}

/**
 * @brief 最小值算子
 */
template<typename T>
inline T min_op(T a, T b) {
    return a < b ? a : b;
}

/**
 * @brief 绝对值算子
 */
template<typename T>
inline T abs_op(T value) {
    return value >= 0 ? value : -value;
}

/**
 * @brief 平方算子
 */
template<typename T>
inline T square_op(T value) {
    return value * value;
}

/**
 * @brief 平方根算子
 */
template<typename T>
inline double sqrt_op(T value) {
    if (value < 0) return 0;
    return std::sqrt(static_cast<double>(value));
}

/**
 * @brief 取整算子（向下取整）
 */
template<typename T>
inline int32_t floor_op(double value) {
    return static_cast<int32_t>(std::floor(value));
}

/**
 * @brief 取整算子（向上取整）
 */
template<typename T>
inline int32_t ceil_op(double value) {
    return static_cast<int32_t>(std::ceil(value));
}

/**
 * @brief 百分比计算算子
 */
template<typename T>
inline double percent_op(T part, T total) {
    if (total == 0) return 0.0;
    return static_cast<double>(part) / static_cast<double>(total) * 100.0;
}

/**
 * @brief 移动平均算子
 */
inline double moving_average(const std::vector<double>& history, int32_t window) {
    if (history.empty() || window <= 0) return 0.0;
    int32_t start_idx = static_cast<int32_t>(history.size()) - window;
    if (start_idx < 0) start_idx = 0;
    
    double sum = 0.0;
    int32_t count = 0;
    for (size_t i = start_idx; i < history.size(); i++) {
        sum += history[i];
        count++;
    }
    return count > 0 ? sum / count : 0.0;
}

/**
 * @brief 向量元素求和
 */
inline double vector_sum(const std::vector<double>& vec) {
    double sum = 0.0;
    for (const auto& v : vec) {
        sum += v;
    }
    return sum;
}

/**
 * @brief 向量元素平均值
 */
inline double vector_avg(const std::vector<double>& vec) {
    if (vec.empty()) return 0.0;
    return vector_sum(vec) / static_cast<double>(vec.size());
}

} // namespace turbograph::ops

#endif // TURBOGRAPH_OPS_HPP
