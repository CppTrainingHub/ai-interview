#include <cassert>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>

#include "pdf/pdf_resume_parser.h"

using interview::pdf::PdfResumeParser;

namespace {

/**
 * @brief 断言指定函数会抛出 std::runtime_error
 * @param operation 待执行的测试操作
 */
template <typename Operation>
void ExpectRuntimeError(Operation operation) {
    bool thrown = false;

    try {
        operation();
    } catch (const std::runtime_error&) {
        thrown = true;
    }

    assert(thrown);
}

/**
 * @brief 创建一个用于测试的非 PDF 文本文件
 * @return 测试文件路径
 */
std::string CreateNonPdfFile() {
    const std::string path = "/tmp/ai_interview_resume_parser_test.txt";

    std::ofstream file(path);
    assert(file);
    file << "not a pdf";

    return path;
}

/**
 * @brief 测试空路径会被拒绝
 */
void TestEmptyPathThrows() {
    PdfResumeParser parser;

    ExpectRuntimeError([&parser]() {
        parser.ExtractText("");
    });

    assert(!parser.IsValidPdf(""));
}

/**
 * @brief 测试不存在的 PDF 路径会被拒绝
 */
void TestMissingFileThrows() {
    PdfResumeParser parser;

    ExpectRuntimeError([&parser]() {
        parser.ExtractText("/tmp/ai_interview_missing_resume.pdf");
    });

    assert(!parser.IsValidPdf("/tmp/ai_interview_missing_resume.pdf"));
}

/**
 * @brief 测试非 PDF 扩展名文件会被拒绝
 */
void TestNonPdfPathThrows() {
    PdfResumeParser parser;
    const std::string path = CreateNonPdfFile();

    ExpectRuntimeError([&parser, &path]() {
        parser.ExtractText(path);
    });

    assert(!parser.IsValidPdf(path));
}

/**
 * @brief 使用环境变量指定的真实 PDF 简历执行可选集成测试
 * @note 设置 AI_INTERVIEW_TEST_RESUME_PDF=/path/to/resume.pdf 后才会执行真实解析；
 *       未设置时跳过该测试，避免单元测试依赖本地私有简历文件
 */
void TestOptionalRealPdfExtraction() {
    const char* path = std::getenv("AI_INTERVIEW_TEST_RESUME_PDF");
    if (path == nullptr || std::string(path).empty()) {
        return;
    }

    PdfResumeParser parser;
    assert(parser.IsValidPdf(path));

    const std::string text = parser.ExtractText(path);
    assert(!text.empty());
}

} // namespace

int main() {
    TestEmptyPathThrows();
    TestMissingFileThrows();
    TestNonPdfPathThrows();
    TestOptionalRealPdfExtraction();
    return 0;
}