/**
 * @file pdf_parser.h
 * @brief PDF文档解析模块
 *
 * PDF文件解析器类。
 * 使用PoDoFo库从PDF文件中提取文本内容，用于简历驱动的面试问题生成。
 *
 * 核心功能：
 * 1. 验证PDF文件有效性
 * 2. 提取PDF中的所有文本内容
 * 3. 支持多页PDF文档
 * 4. 处理各种PDF编码格式
 */

#pragma once

#include <string>
#include <memory>

namespace interview {
namespace services {

/**
 * @brief PDF解析器类
 *
 * 使用PoDoFo库解析PDF文件并提取文本内容。
 *
 * 支持功能：
 * - PDF格式验证
 * - 多页文本提取
 * - 自动处理页面分隔
 * - UTF-8文本编码
 *
 * 使用示例：
 * @code
 * pdf::PDFParser parser;
 *
 * // 检查PDF是否有效
 * if (!parser.IsValidPDF("resume.pdf")) {
 *     std::cerr << "Invalid PDF file" << std::endl;
 *     return;
 * }
 *
 * // 提取文本
 * std::string text = parser.ExtractText("resume.pdf");
 * std::cout << "Extracted " << text.length() << " characters" << std::endl;
 * @endcode
 */
class PDFParser {
public:
    /**
     * @brief 构造函数
     *
     * 创建PDF解析器实例。
     * 内部初始化PoDoFo库。
     */
    PDFParser();

    /**
     * @brief 析构函数
     *
     * 清理资源，释放PoDoFo库。
     */
    ~PDFParser();

    /**
     * @brief 解析PDF文件并提取文本
     * @param pdf_path PDF文件的完整路径
     * @return 提取的文本内容
     * @throws std::runtime_error PDF打开失败或格式不支持
     */
    std::string ExtractText(const std::string& pdf_path);

    /**
     * @brief 检查PDF文件是否有效
     * @param pdf_path PDF文件路径
     * @return true=有效的PDF文件，false=无效或无法解析
     */
    bool IsValidPDF(const std::string& pdf_path);

private:
    class PDFParserImpl;
    std::unique_ptr<PDFParserImpl> pimpl_;
};

} // namespace services
} // namespace interview
