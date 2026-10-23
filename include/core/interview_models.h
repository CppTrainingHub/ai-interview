#pragma once

#include <string>
#include <vector>

namespace interview {
namespace core {
    
/**
 * @brief 面试流程所处阶段
 * @note  代表面试的状态
 */
enum class InterviewStage {
    kNotStarted,    // 题目已准备或者尚未准备，但是面试还没有开始
    kIntro,         // 自我介绍阶段
    kQuestioning,   // 正常技术题阶段
    kFollowUp,      // 追问阶段
    kCompleted      // 面试完成
};

/**
 * @brief 单个面试题
 * @note 该结构只描述题目本身，不包含候选人的回答和评分。
 */
struct InterviewQuestion {
    std::string question;                         // 题目正文，外层可以直接展示或交给TTS朗读
    std::string category = "general";             // 题目类别，例如 memory、stl、thread、project
    std::string level = "intermediate";           // 难度级别，例如 basic、intermediate、advanced
    std::vector<std::string> key_points;          // 该题希望考察的核心知识点
    std::vector<std::string> expected_keywords;   // 期望回答中可能出现的关键词，用于规则评分或LLM提示
};

/**
 * @brief 一次回答的评分结果
 * @note 评分结果由外部 AnswerEvaluator 生成，InterviewSession 只负责记录。
 */
struct AnswerEvaluation {
    int score = 0;                         // 本次回答得分，建议范围为0-100
    std::string feedback;                  // 对回答的简短反馈
    bool need_followup = false;            // 是否需要针对当前回答继续追问
    std::string followup_question;         // 追问内容，只有 need_followup 为 true 时才有意义
};

/**
 * @brief 候选人的一条回答记录
 */
struct InterviewRecord {
    InterviewQuestion question;         // 面试提问
    std::string answer;                 // 候选人回答文本，可以来自ASR，也可以来自CLI输入
    std::string timestamp;              // 回答时间，建议由Session生成，格式可由实现层统一
    AnswerEvaluation evaluation;        // 回答的评分效果
    bool is_followup = false;           // true表示这条记录来自追问问题
};

/**
 * @brief 面试报告的数据模型，这部分只表示报告内容，不负责JSON序列化以及文件写入
 */
struct InterviewReport {
    std::string candidate_name;             // 候选人姓名
    std::string interview_date;             // 面试开始或报告生成时间
    double duration_minutes = 0.0;          // 面试持续时间，单位：分钟
    int total_question = 0;                 // 已记录的问题数量，包含追问记录
    int total_score = 0;                    // 所有回答分数总和
    double average_score = 0.0;             // 平均分，通常由 total_score / total_question 得到
    std::string evaluation;                 // 综合评价，例如“优秀”“良好”“待提高”
    std::string summary_text;               // 总结文本
    std::vector<InterviewRecord> records;       // 面试记录
};


} // namespace core
} // namespace interview
