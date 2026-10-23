#pragma once

#include <memory>
#include <string>

#include "core/interview_ports.h"

namespace interview {
namespace pdf {

/**
 * @brief PDF 简历解析器，负责从 PDF 文件中提取可供面试题生成使用的纯文本。
 * @note 该类实现 core::ResumeParser 接口，属于外部适配层；不会调用 LLM、不会管理面试状态
 */
class PdfResumeParser final : public core::ResumeParser {
public:
    PdfResumeParser();

    ~PdfResumeParser();

    PdfResumeParser(const PdfResumeParser&) = delete;
    PdfResumeParser& operator=(const PdfResumeParser&) = delete;

    PdfResumeParser(PdfResumeParser&&) noexcept;
    PdfResumeParser& operator=(PdfResumeParser&&) noexcept;

    /**
     * @brief 从 PDF 简历文件，提取纯文本内容
     * @param resume_text PDF 简历文件路径
     * @return 标准化后的简历文本，保留基本段落换行
     * @throws std::runtime_error 当路径为空、文件不可读、拓展名不是 PDF 时无法解析或者文件为空
     */
    std::string ExtractText(const std::string& resume_path) override;

    /**
     * @brief 检查文件是否是可读取且可被解析库打开的 pdf
     * @param resume_path PDF 简历文件路径
     * @return 是否成功
     * @note 这里只是做格式检查和是否可打开的检查，并不保证pdf中一定包含可提取文本
     */
    bool IsValidPdf(const std::string& resume_path) const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace pdf
} // namespace interview