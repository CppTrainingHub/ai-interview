/**
 * @file logger.h
 * @brief 日志系统模块
 *
 * 本文件定义了基于spdlog的日志封装类。
 * 提供统一的日志接口，支持多级别日志、彩色输出、文件记录等功能。
 *
 * 核心功能：
 * 1. 多级别日志（TRACE/DEBUG/INFO/WARN/ERROR/CRITICAL）
 * 2. 彩色控制台输出（便于区分日志级别）
 * 3. 文件持久化（所有日志写入文件）
 * 4. 格式化输出（使用spdlog的fmt格式化语法）
 * 5. 调试模式切换（开发/生产环境不同日志级别）
 *
 * 依赖库：
 * - spdlog: 高性能C++日志库
 *
 * 架构设计：
 * - 单例模式管理全局日志实例
 * - 支持多个sink（控制台+文件）
 * - 宏定义简化日志调用
 * - 支持spdlog的fmt格式化语法
 *
 * 使用方式：
 * @code
 * // 初始化日志系统
 * Logger::Init("app.log", true);  // debug模式
 *
 * // 使用宏记录日志
 * LOG_INFO("Application started");
 * LOG_DEBUG("Processing item {}/{}", i, total);
 * LOG_ERROR("Failed to connect: {}", error_msg);
 * @endcode
 */

#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>

namespace interview {
namespace common {

/**
 * @brief 日志管理类（单例）
 *
 * 封装spdlog日志库，提供统一的日志接口。
 *
 * 功能特性：
 * 1. 双sink设计：控制台（彩色）+ 文件（完整）
 * 2. 级别控制：调试模式DEBUG，生产模式INFO
 * 3. 自动刷新：WARN及以上级别立即刷新到文件
 * 4. 格式化：支持fmt库格式化语法
 *
 * 日志级别说明：
 * - TRACE: 非常详细的调试信息，通常不启用
 * - DEBUG: 调试信息，开发环境启用
 * - INFO: 一般信息，记录程序运行状态
 * - WARN: 警告信息，不影响运行但需要注意
 * - ERROR: 错误信息，功能失败但程序可继续
 * - CRITICAL: 严重错误，程序可能崩溃
 *
 * 输出格式：
 * - 控制台: [2025-01-15 14:30:25.123] [INFO] 消息内容
 * - 文件: [2025-01-15 14:30:25.123] [INFO] [文件名:行号] 消息内容
 */
class Logger {
public:
    /**
     * @brief 初始化日志系统
     *
     * 创建日志实例，配置sink和格式。
     * 应在程序启动时调用一次。
     *
     * Sink配置：
     * 1. 控制台sink（彩色）：
     *    - 日志级别：DEBUG（调试模式）或INFO（生产模式）
     *    - 格式：[时间] [级别] 消息
     *    - 颜色：ERROR=红色，WARN=黄色，INFO=绿色等
     *
     * 2. 文件sink：
     *    - 日志级别：TRACE（记录所有）
     *    - 格式：[时间] [级别] [文件:行号] 消息
     *    - 追加模式：不覆盖原有日志
     *
     * @param log_file 日志文件路径，默认"interview.log"
     * @param debug_mode 是否启用调试模式，默认false
     */
    static void Init(const std::string& log_file = "interview.log", bool debug_mode = false);

    /**
     * @brief 获取日志实例
     *
     * 返回全局单例日志实例。
     * 如果未初始化，会自动调用Init()使用默认配置。
     *
     * @return spdlog日志实例的共享指针
     */
    static std::shared_ptr<spdlog::logger>& GetLogger();

private:
    static std::shared_ptr<spdlog::logger> logger_;  ///< 全局日志实例（单例）
};

// ========== 便捷日志宏 ==========
// 这些宏简化日志调用，自动获取日志实例
// 使用示例：LOG_INFO("User {} logged in", username);

/// @brief 记录TRACE级别日志（最详细的调试信息）
#define LOG_TRACE(...)    ::interview::common::Logger::GetLogger()->trace(__VA_ARGS__)

/// @brief 记录DEBUG级别日志（调试信息）
#define LOG_DEBUG(...)    ::interview::common::Logger::GetLogger()->debug(__VA_ARGS__)

/// @brief 记录INFO级别日志（一般信息）
#define LOG_INFO(...)     ::interview::common::Logger::GetLogger()->info(__VA_ARGS__)

/// @brief 记录WARN级别日志（警告信息）
#define LOG_WARN(...)     ::interview::common::Logger::GetLogger()->warn(__VA_ARGS__)

/// @brief 记录WARNING级别日志（同WARN）
#define LOG_WARNING(...)  ::interview::common::Logger::GetLogger()->warn(__VA_ARGS__)

/// @brief 记录ERROR级别日志（错误信息）
#define LOG_ERROR(...)    ::interview::common::Logger::GetLogger()->error(__VA_ARGS__)

/// @brief 记录CRITICAL级别日志（严重错误）
#define LOG_CRITICAL(...) ::interview::common::Logger::GetLogger()->critical(__VA_ARGS__)

} // namespace common 
} // namespace interview
