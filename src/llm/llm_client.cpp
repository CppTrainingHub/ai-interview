#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <memory>
#include <stdexcept>
#include <utility>

#include "llm/llm_client.h"

namespace interview {
namespace llm {
namespace {

/**
 * @brief CURL 响应写入回调
 * @param contents 当前收到的数据块起始地址
 * @param size 单个数据单元的字节大小
 * @param nmemb 数据单元数量
 * @param userp 用户自定义指针，当前实现要求其指向 std::string
 * @return 成功处理的字节数；返回值小于 size * nmemb 会被 libcurl 视为写入失败
 * @note 该函数会被 libcurl 在接收到 HTTP 响应体数据时自动调用
 *       调用方应通过 CURLOPT_WRITEDATA 传入 std::string 对象地址
 *       本函数将每次收到的数据块追加到该字符串中
 */
std::size_t WriteCallback(void* contents, std::size_t size, std::size_t nmemb, void* userp) {
    if (contents == nullptr || userp == nullptr) {
        return 0;
    }

    // 总的字节数
    const std::size_t byte_count = size * nmemb;
    // 这里把 HTTP 响应完整保存到 std::string，所以需要将 userp 转回 string。
    auto* response = static_cast<std::string*>(userp);
    // 将当前收到的一块内容追加到返回结果
    response->append(static_cast<char*>(contents), byte_count);

    return byte_count;
}

}

/**
 * @brief OpenAICompatibleLLMClient 的私有实现类
 * @note 通过 PImpl 隐藏 libcurl 与 nlohmann::json 依赖，避免公共头文件暴露第三方库细节。
 */
class OpenAICompatibleLLMClient::Impl {
public:
    /**
     * @brief 构造 LLM 客户端实现
     * @param config_value LLM API 配置，包括 URL、API key、模型名、温度、token数和超时
     * @note curl_global_init 初始化 libcurl 全局资源
     */
    explicit Impl(LLMConfig config_value) : config(std::move(config_value)) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    /**
     * @brief 析构时采用 curl_global_cleanup 释放全局资源
     */
    ~Impl() {
        curl_global_cleanup();
    }

    /**
     * @brief 发送单条用户消息
     * @param message 用户输入内容
     * @param system_prompt 系统提示词；为空时不添加 system 消息
     * @return LLM assistant 返回的文本内容
     */
    std::string SendMessage(const std::string& message, const std::string& system_prompt) {
        std::vector<LLMMessage> messages;

        if (!system_prompt.empty()) {
            messages.push_back({"system", system_prompt});
        }

        messages.push_back({"user", message});
        return SendConversation(messages);
    }


    /**
     * @brief 发送多轮对话消息
     * @param messages 按顺序排列的对话消息列表
     * @return LLM assistant 返回的文本内容
     *
     * @note 流程是：构造 JSON 请求体 -> 调用 HTTP API -> 从响应 JSON 中提取 assistant.content。
     */
    std::string SendConversation(const std::vector<LLMMessage>& messages) {
        const auto& request = BuildRequest(messages);
        const auto& response = CallAPI(request);
        return ExtractAssistantContent(response);
    }

private:
    /**
     * @brief 构造 OpenAI-compatible Chat Completions 请求体
     * @param messages 对话消息列表
     * @return 可发送给 LLM API 的 JSON 请求体
     * @note 当前使用常见字段：model、temperature、max_tokens、messages
     *       messages 中每条消息包含 role 和 content
     */
    nlohmann::json BuildRequest(const std::vector<LLMMessage>& messages) const {
        nlohmann::json body;
        body["model"] = config.model;
        body["temperature"] = config.temperature;
        body["max_tokens"] = config.max_tokens;
        body["messages"] = nlohmann::json::array();
        for (const auto& message : messages) {
            body["messages"].push_back({
                {"role", message.role},
                {"content", message.content}
            });
        }

        return body;
    }

    /**
     * @brief 调用 LLM HTTP API
     * @param body BuildRequest 构建出的 JSON 请求体
     * @return HTTP 响应体字符串
     * @throws std::runtime_error 当配置缺失、CURL初始化失败、请求失败或者  HTTP 状态码异常时抛出
     * @note 该函数只负责 HTTP 层。
     *       它不解析 assistant 内容，也不理解面试业务语义
     */
    std::string CallAPI(const nlohmann::json& body) const {
        // 首先判断配置是否为空
        if (config.api_url.empty()) {
            throw std::runtime_error("LLM api_url is empty");
        }

        if (config.api_key.empty()) {
            throw std::runtime_error("LLM api_key is empty");
        }

        CURL* curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("failed to initialize CURL");
        }

        std::string response;
        std::string request_json = body.dump();
        struct curl_slist* headers = nullptr;
        
        try {
            headers = curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");

            std::string auth = "Authorization: Bearer " + config.api_key;
            headers = curl_slist_append(headers, auth.c_str());

            curl_easy_setopt(curl, CURLOPT_URL, config.api_url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            // POST 请求体必须在 curl_easy_perform 调用期间保持存活
            // request_json 是局部 std::string, 生命周期覆盖整个请求过程
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_json.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request_json.size());

            // 设置响应体接收方式：libcurl 每收到一块数据就调用 WriteCallback，
            // 并把 &response 作为 userp 传入回调。
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

            curl_easy_setopt(curl, CURLOPT_TIMEOUT, config.timeout_seconds);

            CURLcode code = curl_easy_perform(curl);
            if (code != CURLE_OK) {
                throw std::runtime_error(std::string("CURL resquest failed: ") + curl_easy_strerror(code));
            }

            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            if (http_code < 200 || http_code >= 300) {
                throw std::runtime_error("LLM API returned HTTP " + std::to_string(http_code));
            }

            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            return response;
        } catch(...) {
            if (headers) {
                curl_slist_free_all(headers);
            }

            curl_easy_cleanup(curl);
            throw;
        }
        
    }

    /**
     * @brief 从 OpenAI-compatible 响应中提取 assistant.content
     * @param response HTTP 响应体 JSON 字符串
     * @return assistant 返回的文本内容
     * @throws std::runtime_error 当响应结构缺少 choices/message/content 时抛出
     *
     * @note 典型响应结构：
     *       {
     *         "choices": [
     *           {
     *             "message": {
     *               "role": "assistant",
     *               "content": "..."
     *             }
     *           }
     *         ]
     *       }
     */
    std::string ExtractAssistantContent(const std::string& response) const {
        auto json = nlohmann::json::parse(response);

        if (!json.contains("choices") || json["choices"].empty()) {
            throw std::runtime_error("LLM response missing choices");
        }

        const auto& choice = json["choices"][0];
        if (!choice.contains("message") || !choice["message"].contains("content")) {
            throw std::runtime_error("LLM response missing assistant content");
        }

        return choice["message"]["content"].get<std::string>();
    }

    LLMConfig config;
};

OpenAICompatibleLLMClient::OpenAICompatibleLLMClient(LLMConfig config)
    : pImpl(std::make_unique<Impl>(std::move(config))) {}

OpenAICompatibleLLMClient::~OpenAICompatibleLLMClient() = default;

OpenAICompatibleLLMClient::OpenAICompatibleLLMClient(OpenAICompatibleLLMClient&&) noexcept = default;

OpenAICompatibleLLMClient& OpenAICompatibleLLMClient::operator=(
    OpenAICompatibleLLMClient&&) noexcept = default;

std::string OpenAICompatibleLLMClient::SendMessage(const std::string& message,
                                                   const std::string& system_prompt) {
    return pImpl->SendMessage(message, system_prompt);
}

std::string OpenAICompatibleLLMClient::SendConversation(
    const std::vector<LLMMessage>& messages) {
    return pImpl->SendConversation(messages);
}

} // namespace llm
} // namespace interview
