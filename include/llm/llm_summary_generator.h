#pragma once

#include <memory>

#include "core/interview_ports.h"
#include "llm/llm_client.h"

namespace interview {
namespace llm {

/**
 * @brief LLM 面试总结生成器
 * @note 实现 core::SummaryGenerator，根据面试记录和简历文本生成总结
 */
class LLMSummaryGenerator final : public core::SummaryGenerator {
public:
    /**
     * @brief 构造 LLM 总结生成器
     * @param client LLM 客户端引用
     */
    explicit LLMSummaryGenerator(LLMClient& client);

    ~LLMSummaryGenerator();

    LLMSummaryGenerator(const LLMSummaryGenerator&) = delete;
    LLMSummaryGenerator& operator=(const LLMSummaryGenerator&) = delete;

    LLMSummaryGenerator(LLMSummaryGenerator&&) noexcept;
    LLMSummaryGenerator& operator=(LLMSummaryGenerator&&) noexcept;

    std::string GenerateSummary(const std::vector<core::InterviewRecord>& records,
                                const std::string& resume_text) override;
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace llm
} // namespace interview