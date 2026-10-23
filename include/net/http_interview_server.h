#pragma once

#include <memory>
#include <string>

#include "config/app_config.h"

namespace interview {
namespace net {

/**
 * @brief HTTP 服务监听配置
 * @note 该结构只描述本地监听参数，不包含 LLM 密钥或业务配置。
 */
struct HttpServerConfig {
    std::string host = "0.0.0.0";
    int port = 8080;
    int backlog = 16;
};

/**
 * @brief 面试 HTTP 服务适配器
 * @note 该类负责真实 HTTP 监听和路由分发；业务请求仍转发给 InterviewServer。
 */
class HttpInterviewServer {
public:
    /**
     * @brief 构造 HTTP 面试服务
     * @param app_config 应用业务配置，包含 LLM、报告目录等参数
     * @param http_config HTTP 监听配置
     */
    explicit HttpInterviewServer(config::AppConfig app_config,
                                 HttpServerConfig http_config = {});

    ~HttpInterviewServer();

    HttpInterviewServer(const HttpInterviewServer&) = delete;
    HttpInterviewServer& operator=(const HttpInterviewServer&) = delete;

    HttpInterviewServer(HttpInterviewServer&&) noexcept;
    HttpInterviewServer& operator=(HttpInterviewServer&&) noexcept;

    /**
     * @brief 阻塞运行 HTTP 服务
     * @throws std::runtime_error 当监听端口失败或 socket 初始化失败时抛出
     */
    void Run();

    /**
     * @brief 请求停止 HTTP 服务
     * @note 可从另一个线程调用；已经停止时重复调用安全。
     */
    void Stop();

    /**
     * @brief 获取 HTTP 监听配置
     * @return 当前监听配置
     */
    HttpServerConfig Config() const;

    /**
     * @brief 处理一段完整 HTTP 请求文本
     * @param raw_request 原始 HTTP 请求文本
     * @return 原始 HTTP 响应文本
     * @note 该方法用于单元测试和上层网络库复用，不直接访问 socket。
     */
    std::string HandleRawRequest(const std::string& raw_request);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace net
} // namespace interview
