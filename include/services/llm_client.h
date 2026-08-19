/**
 * @file llm_client.h
 * @brief 大语言模型(LLM) API客户端模块
 *
 * 与大语言模型API交互的客户端类。
 * 负责调用LLM API完成面试相关的智能任务。
 *
 * 核心功能：
 * 1. 根据简历生成面试问题
 * 2. 智能评估候选人回答
 * 3. 生成面试总结和建议
 * 4. 支持OpenAI兼容的API接口
 */

#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace interview {
namespace services {

/**
 * @brief LLM消息结构体
 *
 * 表示LLM对话中的单条消息。
 * 符合OpenAI Chat Completion API的消息格式。
 */
struct Message {
    std::string role;  ///< 角色：system(系统提示), user(用户输入), assistant(AI回复)
    std::string content;  ///< 消息内容文本
};

/**
 * @brief LLM API客户端类
 *
 * 封装与大语言模型API的所有交互逻辑。
 * 提供高层次的面试相关功能接口。
 *
 * 主要功能：
 * 1. 简历解析和问题生成
 * 2. 回答评估和追问决策
 * 3. 面试总结和建议生成
 *
 * 配置管理：
 * - API配置从Config::Instance().llm_config读取
 * - 支持通过命令行或环境变量配置
 * - 默认使用Token Pony API + Qwen3模型
 *
 * 错误处理：
 * - HTTP请求失败时抛出std::runtime_error
 * - JSON解析失败时抛出std::runtime_error
 * - 自动重试不包括在内(调用者需自行实现)
 *
 * 性能考虑：
 * - 每次调用都是同步阻塞的(1-30秒)
 * - 生成问题最慢(10-30秒)
 * - 评估回答较快(1-5秒)
 * - 生成总结中等(5-15秒)
 *
 * 使用示例：
 * @code
 * services::LLMClient client;
 *
 * // 从简历生成问题
 * nlohmann::json questions = client.GenerateQuestionsFromResume(resume_text, 20);
 *
 * // 评估回答
 * nlohmann::json eval = client.EvaluateAnswer(question, answer);
 * int score = eval["score"].get<int>();
 * bool need_followup = eval["need_followup"].get<bool>();
 *
 * // 生成总结
 * nlohmann::json summary = client.GenerateSummary(records, resume_text);
 * @endcode
 */
class LLMClient {
public:
    /**
     * @brief 构造函数
     *
     * 创建LLM客户端实例，初始化HTTP库(libcurl)。
     */
    LLMClient();

    /**
     * @brief 析构函数
     *
     * 清理HTTP库资源。
     */
    ~LLMClient();

    /**
     * @brief 发送单条消息并获取回复
     *
     * 最简单的LLM调用方法，发送一条用户消息和可选的系统提示。
     * 内部会构建完整的消息数组并调用SendConversation。
     *
     * @param message 用户消息内容
     * @param system_prompt 系统提示(可选)，定义AI的行为和角色
     * @return LLM的回复文本
     * @throws std::runtime_error API调用失败或解析失败
     */
    std::string SendMessage(const std::string& message, const std::string& system_prompt = "");

    /**
     * @brief 发送多轮对话并获取回复
     *
     * 支持完整的多轮对话历史。
     * 适用于需要上下文的复杂对话。
     *
     * 工作流程：
     * 1. 构建请求JSON(包含model, temperature, messages等)
     * 2. 使用libcurl发送POST请求到LLM API
     * 3. 解析响应JSON
     * 4. 提取assistant的回复文本
     *
     * @param messages 消息数组，按时间顺序排列
     * @return LLM的回复文本
     * @throws std::runtime_error API调用失败或解析失败
     */
    std::string SendConversation(const std::vector<Message>& messages);

    /**
     * @brief 解析简历内容并生成面试问题
     *
     * 这是面试系统的核心功能之一。
     * LLM会分析简历内容，生成针对性的技术问题。
     *
     * 系统提示词要求：
     * - 问题必须与简历中的项目经验、技术栈相关
     * - 难度分布：30%基础、50%中级、20%高级
     * - 覆盖C++核心知识点
     * - 问题具体、可量化评估
     *
     * 返回JSON格式：
     * [
     *   {
     *     "question": "在你的XX项目中，如何管理内存？",
     *     "category": "内存管理",
     *     "level": "intermediate",
     *     "key_points": ["智能指针", "RAII"],
     *     "expected_keywords": ["shared_ptr", "unique_ptr"]
     *   },
     *   ...
     * ]
     *
     * 注意事项：
     * - 这是最耗时的LLM调用(10-30秒)
     * - 返回的问题数量可能多于min_questions(保证质量)
     * - 如果简历内容不足，会补充通用问题
     *
     * @param resume_text 简历文本内容(由PDFParser提取)
     * @param min_questions 最少生成的问题数量，默认50个
     * @return JSON数组，包含问题列表
     * @throws std::runtime_error LLM调用失败或JSON解析失败
     */
    nlohmann::json GenerateQuestionsFromResume(const std::string& resume_text, int min_questions = 50);

    /**
     * @brief 评估候选人的回答
     *
     * 使用LLM智能评估回答质量并决定是否追问。
     *
     * 评分标准：
     * - 90-100分：深入准确，无需追问
     * - 70-89分：正确但不够深入
     * - 50-69分：基本理解但有遗漏
     * - 30-49分：理解不足
     * - 0-29分：答非所问或完全错误
     *
     * 追问决策：
     * - 分数40-85：需要追问以深入了解
     * - 其他：不追问
     *
     * 返回JSON格式：
     * {
     *   "score": 75,
     *   "feedback": "理解基本概念，但对底层实现掌握不够",
     *   "need_followup": true,
     *   "followup_question": "能详细说明shared_ptr的引用计数实现吗？",
     *   "strengths": ["理解了智能指针的基本用法"],
     *   "weaknesses": ["未提及线程安全问题"]
     * }
     *
     * @param question 面试问题
     * @param answer 候选人的回答
     * @return JSON对象，包含score(分数)、feedback(反馈)、need_followup(是否追问)等
     * @throws std::runtime_error LLM调用失败，返回默认评分(50分)
     */
    nlohmann::json EvaluateAnswer(const std::string& question, const std::string& answer);

    /**
     * @brief 生成面试总结和建议
     *
     * 分析整个面试过程，生成全面的总结报告。
     *
     * 总结内容：
     * 1. 综合评价候选人的技术水平
     * 2. 指出技术优势和薄弱环节
     * 3. 给出具体的学习建议
     * 4. 评估是否推荐录用
     * 5. 建议录用级别
     *
     * 返回JSON格式：
     * {
     *   "overall_score": 75,
     *   "summary": "候选人具备扎实的C++基础...",
     *   "strengths": ["内存管理理解深入", "熟悉STL容器"],
     *   "weaknesses": ["多线程经验不足"],
     *   "suggestions": ["系统学习C++11/14/17新特性"],
     *   "recommendation": "推荐",
     *   "hiring_level": "中级C++工程师"
     * }
     *
     * @param interview_records 面试记录JSON数组(所有问题和回答)
     * @param resume_text 简历文本(可选)，帮助LLM更好地评估
     * @return JSON对象，包含summary(总结)、suggestions(建议)等
     * @throws std::runtime_error LLM调用失败或JSON解析失败
     */
    nlohmann::json GenerateSummary(const nlohmann::json& interview_records,
                                   const std::string& resume_text = "");

private:
    class LLMClientImpl;
    std::unique_ptr<LLMClientImpl> pimpl_;
};

} // namespace services
} // namespace interview
