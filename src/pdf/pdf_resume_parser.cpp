#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "pdf/pdf_resume_parser.h"

#if defined(AI_INTERVIEW_ENABLE_PODOFO)
#include <podofo/podofo.h>
#endif

namespace interview {
namespace pdf {
namespace {

/**
 * @brief 判断路径是否指向一个可读取的文件
 * @param path 文件路径
 * @return true 表示文件可以被输入流打开，false 表示路径为空、文件不存在或不可读
 */
bool IsReadableFile(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    std::ifstream file(path, std::ios::binary);
    return file.good();
}

/**
 * @brief 获取路径中的小写扩展名
 * @param path 文件路径
 * @return 小写扩展名，包含点号；没有扩展名时返回空字符串
 */
std::string GetLowerExtension(const std::string& path) {
    const auto slash_pos = path.find_last_of("/\\");        // 查找路径中的最后一个分隔符的位置
    const auto dot_pos = path.find_last_of('.');            // 查找最后一个 . 的位置

    // 空校验
    if (dot_pos == std::string::npos) {
        return "";
    }

    // 避免误判目录名里的点
    if (slash_pos != std::string::npos && dot_pos < slash_pos) {
        return "";
    }

    // 把拓展名里的每个字符转成小写
    std::string extension = path.substr(dot_pos);
    for (char& ch : extension) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }

    return extension;
}

/**
 * @brief 判断路径扩展名是否为 PDF
 * @param path 文件路径
 * @return true 表示扩展名为 .pdf，false 表示不是 PDF 路径
 */
bool HasPdfExtension(const std::string& path) {
    return GetLowerExtension(path) == ".pdf";
}

/**
 * @brief 去除字符串首尾空白字符
 * @param value 原始字符串
 * @return 去除首尾空白后的字符串
 */
std::string TrimCopy(const std::string& value) {
    std::size_t first = 0;

    // 找到第一个非空白字符
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first])) != 0) {
        ++first;
    }

    // 找到最后一个非空白字符的后一位
    std::size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1])) != 0) {
        --last;
    }

    return value.substr(first, last - first);
}

/**
 * @brief 标准化 PDF 提取出的文本
 * @param raw_text PDF 解析库返回的原始文本
 * @return 清理连续空格、页间空白和首尾空白后的文本
 * @note 该函数保留段落换行，避免破坏简历中的项目经历、技能列表和教育经历结构
 */
std::string NormalizeExtractedText(const std::string& raw_text) {
    std::string normalized;
    bool pending_space = false;
    int consecutive_newlines = 0;

    // 逐个字符开始处理原始文本
    for (unsigned char raw_ch : raw_text) {
        const char ch = static_cast<char>(raw_ch);

        // 忽略 \r 因为 windows换行一般是 \r\n
        if (ch == '\r') {
            continue;
        }

        // 这里如果遇到换行符，则进行换行处理
        if (ch == '\n') {
            // 先删除当前结果末尾的空格，避免行尾再加上空格
            while (!normalized.empty() && normalized.back() == ' ') {
                normalized.pop_back();
            }

            // 除了一开始就是换行，以及多个换行，这里最多保留2个连续换行
            if (!normalized.empty() && consecutive_newlines < 2) {
                normalized.push_back('\n');
            }

            // 遇到换行后，取消待写入空格，因为换行优先级高于普通空格
            pending_space = false;
            ++consecutive_newlines;
            continue;
        }

        // 这里处理非换行类空行，比如 空格 tab 垂直制表符
        if (std::isspace(raw_ch) != 0) {
            pending_space = !normalized.empty() && normalized.back() != '\n';
            continue;
        }

        // 当后面真的遇到非空白字符时，如果之前有待处理空白，就补一个普通空格
        if (pending_space) {
            normalized.push_back(' ');
            pending_space = false;
        }

        // 写入当前普通字符，然后将连续换行计数清零
        normalized.push_back(ch);
        consecutive_newlines = 0;
    }

    return TrimCopy(normalized);
}

} // namespace

class PdfResumeParser::Impl {
public:
    /**
     * @brief 从 PDF 简历中提取纯文本
     * @param resume_path PDF 简历文件路径
     * @return 标准化后的简历文本
     * @throws std::runtime_error 当输入路径无效、文件不可读、PDF 解析失败或文本为空时抛出
     */
    std::string ExtractText(const std::string& resume_path) const {
        // 首先验证简历有效性
        ValidateResumePath(resume_path);

        const std::string raw_text = ExtractRawPdfText(resume_path);
        const std::string normalized_text = NormalizeExtractedText(raw_text);

        if (normalized_text.empty()) {
            throw std::runtime_error(
                "failed to extract resume text: PDF contains no selectable text");
        }

        return normalized_text;
    }

    /**
     * @brief 检查 PDF 文件是否可以被解析库打开
     * @param resume_path PDF 简历文件路径
     * @return true 表示文件可打开，false 表示路径非法、文件不可读或解析失败
     */
    bool IsValidPdf(const std::string& resume_path) const {
        if (resume_path.empty() || !IsReadableFile(resume_path) || !HasPdfExtension(resume_path)) {
            return false;
        }

#if defined(AI_INTERVIEW_ENABLE_PODOFO)
        try {
            PoDoFo::PdfMemDocument document;
            document.Load(resume_path);
            return true;
        } catch (...) {
            return false;
        }
#else
        return false;
#endif
    }

private:
    /**
     * @brief 校验简历路径是否满足解析前置条件。
     * @param resume_path PDF 简历文件路径。
     * @throws std::runtime_error 当路径为空、文件不可读或扩展名不是 PDF 时抛出。
     */
    static void ValidateResumePath(const std::string& resume_path) {
        if (resume_path.empty()) {
            throw std::runtime_error("resume path is empty");
        }

        if (!IsReadableFile(resume_path)) {
            throw std::runtime_error("resume file is not readable: " + resume_path);
        }

        if (!HasPdfExtension(resume_path)) {
            throw std::runtime_error("resume file must be a PDF: " + resume_path);
        }
    }

    /**
     * @brief 调用 PDF 解析库提取原始文本
     * @param resume_path PDF 简历文件路径
     * @return PDF 中提取出的原始文本
     * @throws std::runtime_error 当 PDF 打开或解析失败时抛出
     */
    static std::string ExtractRawPdfText(const std::string& resume_path) {
#if defined(AI_INTERVIEW_ENABLE_PODOFO)
        try {
            // 用 PoDoFo 打开 PDF，逐页提取文本，然后把所有页面文本拼成一个字符串返回
            PoDoFo::PdfMemDocument document;
            document.Load(resume_path);

            // 创建一个字符串输出流，来拼接提取到的文本
            std::ostringstream text_stream;
            const auto& pages = document.GetPages();
            const unsigned page_count = pages.GetCount();

            // 遍历每一页
            for (unsigned page_index = 0; page_index < page_count; page_index++) {
                // 获取当前页对象
                const PoDoFo::PdfPage& page = pages.GetPageAt(page_index);

                // 将当前页面的内容提取出来放到 entries 数组中
                std::vector<PoDoFo::PdfTextEntry> entries;
                page.ExtractTextTo(entries);

                for (const auto& entry : entries) {
                    text_stream << entry.Text << " ";
                }

                if (page_index + 1 < page_count) {
                    text_stream << "\n\n";
                }
            }

            return text_stream.str();
        } catch (const PoDoFo::PdfError& error) {
            throw std::runtime_error("failed to parse PDF resume: " + std::string(error.what()));
        } catch (const std::exception& error) {
            throw std::runtime_error("failed to parse PDF resume: " + std::string(error.what()));
        }
#else
        (void)resume_path;
        throw std::runtime_error(
            "PDF parsing requires PoDoFo; rebuild with podofo available to parse resumes");
#endif
    }
};

PdfResumeParser::PdfResumeParser()
    : pImpl(std::make_unique<Impl>()) {}

PdfResumeParser::~PdfResumeParser() = default;

PdfResumeParser::PdfResumeParser(PdfResumeParser&&) noexcept = default;

PdfResumeParser& PdfResumeParser::operator=(PdfResumeParser&&) noexcept = default;

std::string PdfResumeParser::ExtractText(const std::string& resume_path) {
    return pImpl->ExtractText(resume_path);
}

bool PdfResumeParser::IsValidPdf(const std::string& resume_path) const {
    return pImpl->IsValidPdf(resume_path);
}

} // namespace pdf
} // namespace interview
