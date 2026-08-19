#include "interview/interview_manager.h"
#include "common/logger.h"
#include "services/llm_client.h"
#include "services/pdf_parser.h"
#include <fstream>
#include <sstream>
#include <ctime>
#include <cmath>
#include <iomanip>
#include <memory>

namespace interview {
namespace session {

struct InterviewRecord {
    std::string question;
    std::string category;
    std::string level;
    std::string answer;
    std::string timestamp;
    int score;
    std::string feedback;

    nlohmann::json ToJson() const {
        nlohmann::json j;
        j["question"] = question;
        j["category"] = category;
        j["level"] = level;
        j["answer"] = answer;
        j["timestamp"] = timestamp;
        j["score"] = score;
        j["feedback"] = feedback;
        return j;
    }
};

namespace {
// 安全获取本地时间，避免线程安全问题
inline std::tm SafeLocalTime(std::time_t time) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    return tm;
}

} // namespace

class InterviewSession::InterviewSessionImpl {
public:
    std::string candidate_name;
    std::vector<InterviewRecord> records;
    int max_questions;
    int total_score;
    std::time_t start_time;

    std::string resume_text;
    nlohmann::json llm_questions;
    int current_question_index;

    std::unique_ptr<services::LLMClient> llm_client;

    std::string followup_question;
    std::string summary_text;
    bool has_followed_up; 

    InterviewSessionImpl(const std::string& name)
        : candidate_name(name)
        , max_questions(8)
        , total_score(0)
        , current_question_index(0)
        , has_followed_up(false) {
        start_time = std::time(nullptr);
        llm_client = std::make_unique<services::LLMClient>();
    }

    std::string GetCurrentTime() const {
        auto now = std::time(nullptr);
        auto tm = SafeLocalTime(now);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%H:%M:%S");
        return oss.str();
    }
};

InterviewSession::InterviewSession(const std::string& candidate_name)
    : pimpl_(std::make_unique<InterviewSessionImpl>(candidate_name)) {
    LOG_INFO("Interview session created for: {}", candidate_name);
}

InterviewSession::~InterviewSession() = default;

void InterviewSession::GenerateDefaultQuestions(int min_questions) {
    try {
        LOG_INFO("Generating {} default C++ interview questions using LLM...", min_questions);

        // 生成通用C++面试题 - 简洁描述
        std::string generic_resume = R"(C++开发工程师岗位，要求掌握：
C++基础(指针/引用/const)、内存管理(智能指针/RAII)、STL(容器/算法)、
面向对象(继承/多态/虚函数)、C++11/14/17新特性、多线程、设计模式)";

        pimpl_->llm_questions = pimpl_->llm_client->GenerateQuestionsFromResume(
            generic_resume, min_questions
        );

        if (pimpl_->llm_questions.empty()) {
            throw std::runtime_error("Failed to generate default questions");
        }

        pimpl_->current_question_index = 0;
        pimpl_->max_questions = static_cast<int>(pimpl_->llm_questions.size());

        LOG_INFO("Generated {} default C++ questions", pimpl_->max_questions);

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to generate default questions: {}", e.what());
        throw;
    }
}

void InterviewSession::LoadQuestionsFromResume(const std::string& resume_pdf_path, int min_questions) {
    try {
        LOG_INFO("Loading resume from: {}", resume_pdf_path);

        // 解析PDF简历
        services::PDFParser pdf_parser;
        if (!pdf_parser.IsValidPDF(resume_pdf_path)) {
            throw std::runtime_error("Invalid PDF file: " + resume_pdf_path);
        }

        pimpl_->resume_text = pdf_parser.ExtractText(resume_pdf_path);
        LOG_INFO("Resume text extracted: {} characters", pimpl_->resume_text.length());

        // 创建LLM客户端
        if (!pimpl_->llm_client) {
            pimpl_->llm_client = std::make_unique<services::LLMClient>();
        }

        // 生成问题
        LOG_INFO("Generating questions from resume using LLM...");
        pimpl_->llm_questions = pimpl_->llm_client->GenerateQuestionsFromResume(
            pimpl_->resume_text, min_questions
        );

        if (pimpl_->llm_questions.empty()) {
            throw std::runtime_error("Failed to generate questions from resume");
        }

        pimpl_->current_question_index = 0;
        pimpl_->max_questions = static_cast<int>(pimpl_->llm_questions.size());

        LOG_INFO("Loaded {} questions from resume", pimpl_->max_questions);

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to load questions from resume: {}", e.what());
        throw;
    }
}

std::string InterviewSession::GetIntroPrompt() const {
    // 触发AI主动开场：告诉AI应该做什么
    return "现在开始C++技术面试。请你作为一位专业且友好的面试官，用简短温和的语气向候选人问好（比如'你好，欢迎参加今天的面试'），然后礼貌地邀请候选人做一个简单的自我介绍。记住要简短，不超过3句话。后续对应于面试者的回答都只回复(好的，我们继续)。";
}

std::string InterviewSession::GetFirstQuestion() {
    // 从LLM生成的问题中获取
    if (!pimpl_->llm_questions.empty()) {
        pimpl_->current_question_index = 0;
        auto& q = pimpl_->llm_questions[pimpl_->current_question_index];
        std::string question_text = q["question"].get<std::string>();
        return "第1题：" + question_text;
    }

    // 如果没有LLM问题，抛出错误
    throw std::runtime_error("No questions loaded. Call GenerateDefaultQuestions() or LoadQuestionsFromResume() first.");
}

std::string InterviewSession::GetNextQuestion() {
    if (!pimpl_->llm_questions.empty()) {
        // 移到下一个问题索引
        pimpl_->current_question_index++;
        pimpl_->has_followed_up = false;  // 重置追问标志，进入新问题

        if (pimpl_->current_question_index >= pimpl_->max_questions) {
            return "";  // 所有问题已问完
        }

        auto& q = pimpl_->llm_questions[pimpl_->current_question_index];
        std::string question_text = q["question"].get<std::string>();

        LOG_INFO("Moving to question index: {}", pimpl_->current_question_index);
        return "第" + std::to_string(pimpl_->current_question_index + 1) + "题：" + question_text;
    }

    throw std::runtime_error("No questions loaded. Call GenerateDefaultQuestions() or LoadQuestionsFromResume() first.");
}

std::string InterviewSession::GetFollowUpQuestion() {
    if (!pimpl_->followup_question.empty()) {
        std::string question = pimpl_->followup_question;
        pimpl_->followup_question.clear();
        pimpl_->has_followed_up = true;
        return "追问：" + question;
    }
    return "";
}

void InterviewSession::RecordAnswer(const std::string& answer) {
    InterviewRecord record;
    record.answer = answer;
    record.timestamp = pimpl_->GetCurrentTime();

    if (!pimpl_->llm_questions.empty() &&
        pimpl_->current_question_index < pimpl_->max_questions) {

        auto& q = pimpl_->llm_questions[pimpl_->current_question_index];
        // 根据是否已有followup_question判断是否是追问
        bool is_followup = !pimpl_->followup_question.empty();
        
        if (!is_followup) {
            record.question = q["question"].get<std::string>();
        } else {
            record.question = "追问 - " + q["question"].get<std::string>();
        }
        
        record.category = q.value("category", "general");
        record.level = q.value("level", "intermediate");

        // 使用LLM评分（同时判断是否需要追问）
        try {
            LOG_DEBUG("Evaluating answer with LLM...");
            auto llm_eval = pimpl_->llm_client->EvaluateAnswer(record.question, answer);
            record.score = llm_eval["score"].get<int>();
            record.feedback = llm_eval["feedback"].get<std::string>();

            if (!is_followup && !pimpl_->has_followed_up && llm_eval.value("need_followup", false)) {
                if (llm_eval.contains("followup_question")) {
                    pimpl_->followup_question = llm_eval["followup_question"].get<std::string>();
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("LLM evaluation failed: {}", e.what());
            record.score = 50;
            record.feedback = "评分失败，给予中等分数";
            pimpl_->followup_question.clear();
        }
    } else {
        LOG_WARNING("RecordAnswer called but no current question available");
        return;  // 无当前问题，不记录
    }

    pimpl_->records.push_back(record);
    pimpl_->total_score += record.score;

    LOG_INFO("Answer recorded - Score: {} ({}), Total records: {}", 
             record.score, record.feedback, pimpl_->records.size());
}

int InterviewSession::GetLastScore() const {
    if (pimpl_->records.empty()) {
        return 0;
    }
    return pimpl_->records.back().score;
}

bool InterviewSession::ShouldFollowUp() const {
    return !pimpl_->followup_question.empty();
}

bool InterviewSession::IsComplete() const {
    return (pimpl_->current_question_index + 1) >= pimpl_->max_questions;
}

std::string InterviewSession::GenerateSummary() const {
    if (!pimpl_->summary_text.empty()) {
        return pimpl_->summary_text;
    }

    if (pimpl_->records.empty()) {
        return "本次面试尚未开始，暂无总结。";
    }

    // 使用LLM生成总结
    if (pimpl_->llm_client) {
        try {
            LOG_INFO("Generating interview summary using LLM...");

            // 准备面试记录JSON
            nlohmann::json records_json = nlohmann::json::array();
            for (const auto& record : pimpl_->records) {
                records_json.push_back(record.ToJson());
            }

            // 调用LLM生成总结
            auto llm_summary = pimpl_->llm_client->GenerateSummary(
                records_json,
                pimpl_->resume_text
            );

            // 格式化输出
            std::ostringstream oss;
            oss << "本次面试已结束。以下是整体总结：\n\n";
            oss << llm_summary["summary"].get<std::string>() << "\n\n";

            if (llm_summary.contains("recommendation")) {
                oss << "录用建议：" << llm_summary["recommendation"].get<std::string>() << "\n";
            }

            if (llm_summary.contains("hiring_level")) {
                oss << "建议级别：" << llm_summary["hiring_level"].get<std::string>() << "\n";
            }

            oss << "\n感谢您参加本次面试，祝您工作顺利！";

            pimpl_->summary_text = oss.str();
            return pimpl_->summary_text;

        } catch (const std::exception& e) {
            LOG_ERROR("Failed to generate LLM summary: {}", e.what());
            // 失败则降级到默认总结
        }
    }

    // 默认总结（无LLM）
    double avg_score = static_cast<double>(pimpl_->total_score) / static_cast<double>(pimpl_->records.size());
    auto now = std::time(nullptr);
    double duration = std::difftime(now, pimpl_->start_time) / 60.0;

    std::ostringstream oss;
    oss << "本次面试已结束。以下是整体总结：\n\n";
    oss << "本次面试时长约" << std::fixed << std::setprecision(1) << duration << "分钟，";
    oss << "共" << pimpl_->records.size() << "道题目。\n";
    oss << "综合评分：" << std::fixed << std::setprecision(0) << avg_score << "分\n\n";
    oss << "总体评价：";

    if (avg_score >= 80) {
        oss << "您的C++基础非常扎实，对各个知识点理解深入，表现优秀。";
    } else if (avg_score >= 65) {
        oss << "您具备良好的C++基础，但部分知识点还需要加深理解。";
    } else {
        oss << "您的C++基础还需要进一步加强，建议系统地学习核心概念。";
    }

    oss << "\n\n感谢您参加本次面试，祝您工作顺利！";

    pimpl_->summary_text = oss.str();
    return pimpl_->summary_text;
}

nlohmann::json InterviewSession::GenerateReport() const {
    nlohmann::json report;

    if (pimpl_->records.empty()) {
        report["error"] = "没有面试记录";
        return report;
    }

    float avg_score = static_cast<float>(pimpl_->total_score) / pimpl_->records.size();
    auto now = std::time(nullptr);
    double duration = std::difftime(now, pimpl_->start_time) / 60.0;

    auto start_tm = SafeLocalTime(pimpl_->start_time);
    std::ostringstream date_oss;
    date_oss << std::put_time(&start_tm, "%Y-%m-%d %H:%M:%S");

    report["candidate_name"] = pimpl_->candidate_name;
    report["interview_date"] = date_oss.str();
    report["duration_minutes"] = std::round(duration * 10) / 10.0;
    report["total_questions"] = pimpl_->records.size();
    report["average_score"] = std::round(avg_score * 10) / 10.0;
    report["total_score"] = pimpl_->total_score;

    // 记录列表
    nlohmann::json records_array = nlohmann::json::array();
    for (const auto& record : pimpl_->records) {
        records_array.push_back(record.ToJson());
    }
    report["records"] = records_array;

    // 评价
    std::string evaluation;
    if (avg_score >= 85) {
        evaluation = "优秀 - C++技术功底深厚，强烈推荐";
    } else if (avg_score >= 70) {
        evaluation = "良好 - C++基础扎实，推荐录用";
    } else if (avg_score >= 60) {
        evaluation = "中等 - 基础尚可，需继续学习";
    } else {
        evaluation = "待提高 - 基础薄弱，建议加强学习";
    }
    report["evaluation"] = evaluation;

    // 总结文本
    report["summary_text"] = GenerateSummary();

    return report;
}

std::string InterviewSession::SaveReport(const std::string& filename) const {
    std::string output_file = filename;

    if (output_file.empty()) {
        auto now = std::time(nullptr);
        auto tm = SafeLocalTime(now);
        std::ostringstream oss;
        oss << "interview_report_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".json";
        output_file = oss.str();
    }

    auto report = GenerateReport();

    std::ofstream ofs(output_file);
    if (!ofs) {
        throw std::runtime_error("Failed to open file: " + output_file);
    }

    ofs << report.dump(2);
    ofs.close();

    LOG_INFO("Interview report saved to: {}", output_file);

    return output_file;
}

int InterviewSession::GetCurrentQuestionIndex() const {
    return pimpl_->current_question_index;
}

} // namespace session
} // namespace interview
