#pragma once

#include <memory>
#include <string>

#include "config/app_config.h"

namespace interview {
namespace net {

/**
 * @brief 面试服务入口，提供 JSON 字符串级别的请求处理接口
 * @note 第一版不绑定具体 HTTP 库；真实 HTTP Server 后续只需要转发 body 到这些方法
 */
class InterviewServer {
public:
    /**
     * @brief 构造面试服务
     * @param config 应用配置
     */
    explicit InterviewServer(config::AppConfig config);

    ~InterviewServer();

    InterviewServer(const InterviewServer&) = delete;
    InterviewServer& operator=(const InterviewServer&) = delete;

    InterviewServer(InterviewServer&&) noexcept;
    InterviewServer& operator=(InterviewServer&&) noexcept;

    /**
     * @brief 处理健康检查请求
     * @return JSON 字符串响应
     */
    std::string HandleHealth() const;

    /**
     * @brief 处理启动面试请求
     * @param json_body 请求 JSON 字符串
     * @return JSON 字符串响应
     */
    std::string HandleStart(const std::string& json_body);

    /**
     * @brief 处理提交回答请求
     * @param json_body 请求 JSON 字符串
     * @return JSON 字符串响应
     */
    std::string HandleAnswer(const std::string& json_body);

    /**
     * @brief 处理生成报告请求
     * @param json_body 请求 JSON 字符串
     * @return JSON 字符串响应
     */
    std::string HandleReport(const std::string& json_body);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace net
} // namespace interview