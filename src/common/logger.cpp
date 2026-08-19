#include "common/logger.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <vector>
#include <memory>
#include <iostream>

namespace interview {
namespace common {

std::shared_ptr<spdlog::logger> Logger::logger_ = nullptr;

void Logger::Init(const std::string& log_file, bool debug_mode) {
    try {
        // 创建多个 sink：控制台 + 文件
        std::vector<spdlog::sink_ptr> sinks;

        // 1. 彩色控制台输出
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(debug_mode ? spdlog::level::debug : spdlog::level::info);
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        sinks.push_back(console_sink);

        // 2. 文件输出
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file, true);
        file_sink->set_level(spdlog::level::trace);  // 文件记录所有级别
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v");
        sinks.push_back(file_sink);

        // 创建 logger
        logger_ = std::make_shared<spdlog::logger>("interview", sinks.begin(), sinks.end());
        logger_->set_level(debug_mode ? spdlog::level::debug : spdlog::level::info);
        logger_->flush_on(spdlog::level::warn);  // warn 及以上级别立即刷新

        // 设置为默认 logger
        spdlog::set_default_logger(logger_);

        LOG_INFO("Logger initialized - Debug mode: {}", debug_mode);
    }
    catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
    }
}

std::shared_ptr<spdlog::logger>& Logger::GetLogger() {
    if (!logger_) {
        Init();  // 如果未初始化，使用默认配置
    }
    return logger_;
}

} // namespace common 
} // namespace interview
