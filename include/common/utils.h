/**
 * @file utils.h
 * @brief 通用工具函数模块
 *
 * 本文件定义了项目中常用的工具函数。
 * 包括UUID生成、JSON解析等实用功能。
 *
 * 核心功能：
 * 1. UUID v4生成（用于会话ID、连接ID）
 * 2. JSON字符串提取和解析
 * 3. 从混合文本中提取JSON对象
 *
 * 设计特点：
 * - 所有函数都是inline函数（头文件实现）
 * - 无需单独编译，包含即可使用
 * - 使用标准库和nlohmann/json
 */

#pragma once

#include <string>
#include <random>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace interview {
namespace common {

/**
 * @brief 生成UUID v4标准格式字符串
 *
 * 生成符合RFC 4122标准的UUID v4（随机UUID）。
 * 格式：xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
 * 其中x是随机十六进制数字，4是版本号，y是8/9/a/b之一。
 *
 * 使用场景：
 * - WebSocket连接ID（X-Api-Connect-Id）
 * - 对话会话ID（session_id）
 * - 唯一标识符生成
 *
 * 实现方式：
 * - 使用std::random_device获取真随机种子
 * - 使用std::mt19937生成伪随机数
 * - 严格按照UUID v4格式构建
 *
 * @return 标准UUID v4字符串，例如："550e8400-e29b-41d4-a716-446655440000"
 */
inline std::string GenerateUUID() {
    std::random_device rd;  // 真随机数种子
    std::mt19937 gen(rd());  // Mersenne Twister随机数生成器
    std::uniform_int_distribution<> dis(0, 15);  // 0-15的均匀分布（十六进制）
    std::uniform_int_distribution<> dis2(8, 11);  // 8-11的均匀分布（UUID v4的y字段）

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    // 格式: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    for (int i = 0; i < 8; i++) oss << dis(gen);  // 8个十六进制数
    oss << '-';
    for (int i = 0; i < 4; i++) oss << dis(gen);  // 4个十六进制数
    oss << "-4";  // 版本号4（UUID v4）
    for (int i = 0; i < 3; i++) oss << dis(gen);  // 3个十六进制数
    oss << '-';
    oss << dis2(gen);  // y字段（8/9/a/b）
    for (int i = 0; i < 3; i++) oss << dis(gen);  // 3个十六进制数
    oss << '-';
    for (int i = 0; i < 12; i++) oss << dis(gen);  // 12个十六进制数

    return oss.str();
}

/**
 * @brief 从文本中提取JSON字符串
 *
 * 在混合文本中查找JSON对象或数组的起始和结束位置，提取完整JSON字符串。
 * 适用于LLM返回的文本中包含JSON数据的情况。
 *
 * 工作原理：
 * - 从左向右查找第一个start_char（如'{'或'['）
 * - 从右向左查找最后一个end_char（如'}'或']'）
 * - 提取两者之间的子字符串
 *
 * 注意事项：
 * - 不验证JSON格式是否正确（只提取字符串）
 * - 如果有嵌套的JSON，会提取最外层
 * - 如果没有找到起始或结束字符，抛出异常
 *
 * 使用场景：
 * - LLM返回："这是一个JSON对象：{\"key\":\"value\"}，请查看。"
 * - 提取结果："{\"key\":\"value\"}"
 *
 * @param response 包含JSON的文本
 * @param start_char JSON起始字符（'{'表示对象，'['表示数组）
 * @param end_char JSON结束字符（'}'表示对象，']'表示数组）
 * @return 提取的JSON字符串
 * @throws std::runtime_error 如果没有找到有效的JSON边界
 */
inline std::string ExtractJSON(const std::string& response, char start_char, char end_char) {
    size_t json_start = response.find(start_char);  // 查找第一个起始字符
    size_t json_end = response.rfind(end_char);  // 查找最后一个结束字符

    if (json_start == std::string::npos || json_end == std::string::npos) {
        throw std::runtime_error("Response does not contain valid JSON markers");
    }

    return response.substr(json_start, json_end - json_start + 1);  // 提取子字符串（包含边界字符）
}

/**
 * @brief 从文本中解析JSON对象
 *
 * 先提取JSON字符串，再解析为nlohmann::json对象。
 * 这是ExtractJSON的增强版，一步完成提取和解析。
 *
 * 工作流程：
 * 1. 调用ExtractJSON提取JSON字符串
 * 2. 使用nlohmann::json::parse解析字符串
 * 3. 返回JSON对象
 *
 * 使用场景：
 * - LLM生成问题："这些是问题：[{\"question\":\"...\"}, ...]"
 * - LLM评估回答："评估结果：{\"score\":75, \"feedback\":\"...\"}"
 * - LLM生成总结："总结如下：{\"summary\":\"...\", \"recommendation\":\"...\"}"
 *
 * @param response 包含JSON的文本
 * @param start_char JSON起始字符，默认'{'（对象）
 * @param end_char JSON结束字符，默认'}'（对象）
 * @return 解析后的JSON对象或数组
 * @throws std::runtime_error JSON提取失败或解析失败
 */
inline nlohmann::json ParseJSONFromResponse(const std::string& response,
                                            char start_char = '{',
                                            char end_char = '}') {
    std::string json_str = ExtractJSON(response, start_char, end_char);  // 提取JSON字符串
    return nlohmann::json::parse(json_str);  // 解析为JSON对象
}

} // namespace common 
} // namespace interview
