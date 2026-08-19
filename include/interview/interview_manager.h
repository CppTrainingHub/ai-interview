/**
 * @file interview_manager.h
 * @brief 面试管理模块
 *
 * 本文件定义了面试会话管理和流程控制的核心类。
 * 负责管理整个技术面试的生命周期，包括问题生成、答案记录、评分和报告生成。
 *
 * 核心职责：
 * 1. 面试问题管理（生成、加载、获取）
 * 2. 候选人回答记录和评分
 * 3. 面试流程控制（状态机管理）
 * 4. 面试报告生成和保存
 * 5. 简历驱动的智能问题生成
 * 6. LLM智能评分和追问
 *
 * 架构设计：
 * - InterviewSession: 面试会话，管理问题和记录
 * - 使用Pimpl模式隐藏实现细节
 * - 支持简历驱动和默认问题两种模式
 * - 集成LLM实现智能评分和追问
 */

#pragma once

#include <memory>
#include <string>
#include <nlohmann/json.hpp>

namespace interview {
namespace session {
/**
 * @brief 面试会话类
 *
 * 管理一次完整的技术面试会话，负责：
 * - 面试问题的生成和管理
 * - 候选人回答的记录和评分
 * - 面试进度跟踪和状态管理
 * - 面试报告的生成和导出
 *
 * 工作模式：
 * 1. 简历驱动模式：根据PDF简历生成针对性问题（推荐）
 * 2. 默认问题模式：生成通用C++技术面试题
 *
 * 评分模式：
 * 1. LLM智能评分：使用大模型评估回答质量并判断是否追问（推荐）
 * 2. 简单规则评分：基于关键词的简单打分
 *
 * 使用流程：
 * 1. 创建面试会话实例
 * 2. 加载问题（LoadQuestionsFromResume 或 GenerateDefaultQuestions）
 * 3. 启用LLM评分（可选）
 * 4. 通过DialogSession控制面试流程
 * 5. 面试结束后生成报告
 *
 * 线程安全：
 * - 本类不是线程安全的，应在单线程中使用
 * - 通常由DialogSession的主线程调用
 *
 * 使用示例：
 * @code
 * // 创建面试会话
 * auto session = std::make_shared<InterviewSession>("张三");
 *
 * // 从简历生成问题
 * session->LoadQuestionsFromResume("resume.pdf", 20);
 *
 * // 启用LLM评分
 * session->EnableLLMScoring(true);
 *
 * // 获取第一个问题
 * std::string intro = session->GetIntroPrompt();
 * std::string first_q = session->GetFirstQuestion();
 *
 * // 记录回答
 * session->RecordAnswer("我认为多态是...");
 *
 * // 检查是否需要追问
 * if (session->ShouldFollowUp()) {
 *     std::string followup = session->GetFollowUpQuestion();
 * }
 *
 * // 生成报告
 * std::string report_file = session->SaveReport();
 * @endcode
 */
class InterviewSession {
public:
    /**
     * @brief 构造函数
     *
     * 创建面试会话实例，初始化内部状态。
     *
     * @param candidate_name 候选人姓名，用于个性化交互和报告生成
     */
    InterviewSession(const std::string& candidate_name = "候选人");

    /**
     * @brief 析构函数
     *
     * 自动清理资源，保存未保存的数据。
     */
    ~InterviewSession();

    /**
     * @brief 生成默认的C++面试题
     *
     * 当没有简历时使用此方法生成通用C++技术面试题。
     * 使用LLM根据通用C++岗位要求生成问题。
     *
     * 问题涵盖：
     * - C++基础语法（指针、引用、const）
     * - 内存管理（智能指针、RAII）
     * - STL容器和算法
     * - 面向对象（继承、多态、虚函数）
     * - C++11/14/17新特性
     * - 多线程编程
     * - 设计模式
     *
     * 难度分布：
     * - 30% 基础题（WARM_UP阶段）
     * - 50% 中级题（TECHNICAL阶段）
     * - 20% 高级题（CHALLENGE阶段）
     *
     * 注意事项：
     * - 调用LLM API生成问题，可能耗时5-30秒
     * - 生成的问题数量可能略多于num_questions（保证质量）
     * - 必须在面试开始前调用
     * - 不能与LoadQuestionsFromResume同时使用
     *
     * @param num_questions 期望生成的问题数量，默认15个
     * @throws std::runtime_error LLM调用失败或问题生成失败
     */
    void GenerateDefaultQuestions(int num_questions = 15);

    /**
     * @brief 从PDF简历加载问题（简历驱动面试）
     *
     * 解析PDF简历，提取内容，使用LLM生成针对性的技术问题。
     * 这是推荐的面试模式，问题与候选人背景高度相关。
     *
     * 工作流程：
     * 1. 使用PDFParser解析PDF文件
     * 2. 提取简历文本内容
     * 3. 调用LLM API分析简历
     * 4. 根据项目经验、技术栈生成问题
     * 5. 存储问题列表供面试使用
     *
     * 生成的问题特点：
     * - 针对简历中提到的项目提问
     * - 基于候选人声称掌握的技术栈
     * - 考察实际项目经验和深度
     * - 包含技术细节和实践场景
     *
     * 前置条件：
     * - PDF文件必须存在且可读
     * - PDF内容必须可解析（不能是扫描件）
     * - LLM API配置正确且可用
     *
     * @param resume_pdf_path PDF简历文件的绝对路径
     * @param min_questions 最少生成的问题数量，默认15个
     * @throws std::runtime_error PDF解析失败或LLM生成失败
     */
    void LoadQuestionsFromResume(const std::string& resume_pdf_path, int min_questions = 15);

    /**
     * @brief 获取开场白提示词
     *
     * 返回触发AI说开场白的提示文本。
     * 告诉AI应该主动问候候选人并要求自我介绍。
     *
     * @return 开场白提示词，用于SendTextQuery
     */
    std::string GetIntroPrompt() const;

    /**
     * @brief 获取第一个技术问题
     *
     * 返回第一个技术问题的提示文本，进入WARM_UP阶段。
     *
     * @return 第一个问题的提示词
     * @throws std::runtime_error 如果问题未加载
     */
    std::string GetFirstQuestion();

    /**
     * @brief 获取下一个问题
     *
     * 按顺序返回下一个问题。根据进度自动调整面试阶段。
     *
     * @return 下一个问题的提示词，如果所有问题已问完则返回空字符串
     * @throws std::runtime_error 如果问题未加载
     */
    std::string GetNextQuestion();

    /**
     * @brief 获取追问问题
     *
     * 返回LLM生成的追问问题（如果ShouldFollowUp()为true）。
     * 追问后自动清除追问状态，避免重复追问。
     *
     * @return 追问问题的提示词，如果无需追问则返回空字符串
     */
    std::string GetFollowUpQuestion();

    /**
     * @brief 记录候选人的回答
     *
     * 保存回答并使用LLM评分（如果启用）。
     * 评分结果包括分数、反馈、是否需要追问等。
     *
     * 工作流程：
     * 1. 获取当前问题信息
     * 2. 创建面试记录
     * 3. 调用LLM评估回答（如果启用）
     * 4. 保存评分和反馈
     * 5. 判断是否需要追问
     * 6. 累计总分
     *
     * @param answer 候选人的回答文本（ASR识别结果）
     */
    void RecordAnswer(const std::string& answer);

    /**
     * @brief 获取最后一次回答的分数
     *
     * @return 最后一次回答的分数（0-100），如果无记录则返回0
     */
    int GetLastScore() const;

    /**
     * @brief 判断是否需要追问
     *
     * 基于LLM评估结果判断是否应该对上一次回答进行追问。
     *
     * @return true=需要追问，false=不需要追问
     */
    bool ShouldFollowUp() const;

    /**
     * @brief 检查面试是否已完成
     *
     * 当所有问题都已提问完毕时返回true。
     *
     * @return true=所有问题已问完，false=还有问题未提问
     */
    bool IsComplete() const;

    /**
     * @brief 生成面试总结
     *
     * 使用LLM分析所有面试记录，生成详细的面试总结。
     * 包括整体评价、技术优势、薄弱环节、学习建议和录用建议。
     *
     * @return 面试总结文本（中文自然语言）
     */
    std::string GenerateSummary() const;

    /**
     * @brief 生成面试报告JSON
     *
     * 将所有面试数据整理成结构化的JSON报告。
     *
     * JSON结构：
     * - candidate_name: 候选人姓名
     * - interview_date: 面试日期时间
     * - duration_minutes: 面试时长（分钟）
     * - total_questions: 问题总数
     * - average_score: 平均分
     * - total_score: 总分
     * - records: 每个问题的详细记录数组
     * - evaluation: 综合评价
     *
     * @return 面试报告JSON对象
     */
    nlohmann::json GenerateReport() const;

    /**
     * @brief 保存面试报告到文件
     *
     * 将报告保存为格式化的JSON文件。
     *
     * @param filename 输出文件名，默认为空（自动生成：interview_report_YYYYMMDD_HHMMSS.json）
     * @return 实际保存的文件路径
     * @throws std::runtime_error 文件写入失败
     */
    std::string SaveReport(const std::string& filename = "") const;

    int GetCurrentQuestionIndex() const;

private:
    class InterviewSessionImpl;
    std::unique_ptr<InterviewSessionImpl> pimpl_; 
};

} // namespace session
} // namespace interview
