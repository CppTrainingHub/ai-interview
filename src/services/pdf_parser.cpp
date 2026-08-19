#include "services/pdf_parser.h"
#include "common/logger.h"
#include <podofo/podofo.h>
#include <stdexcept>
#include <sstream>

namespace interview {
namespace services {

class PDFParser::PDFParserImpl {
public:
    std::string ExtractText(const std::string& pdf_path) {
        try {
            // 打开PDF文档
            PoDoFo::PdfMemDocument document;
            document.Load(pdf_path);

            std::ostringstream text_stream;
            const auto& pages = document.GetPages();
            unsigned page_count = pages.GetCount();

            LOG_DEBUG("PDF has {} pages", page_count);

            // 遍历所有页面提取文本
            for (unsigned i = 0; i < page_count; ++i) {
                const PoDoFo::PdfPage& page = pages.GetPageAt(i);

                // 提取文本条目
                std::vector<PoDoFo::PdfTextEntry> entries;
                page.ExtractTextTo(entries);

                // 将文本条目连接成字符串
                for (const auto& entry : entries) {
                    text_stream << entry.Text << " ";
                }

                // 页面分隔
                if (i < page_count - 1) {
                    text_stream << "\n\n";
                }
            }

            std::string extracted_text = text_stream.str();
            LOG_INFO("Extracted {} characters from PDF", extracted_text.length());

            return extracted_text;

        } catch (const PoDoFo::PdfError& e) {
            LOG_ERROR("PoDoFo error: {}", e.what());
            throw std::runtime_error("Failed to parse PDF: " + std::string(e.what()));
        } catch (const std::exception& e) {
            LOG_ERROR("PDF parsing error: {}", e.what());
            throw std::runtime_error("Failed to parse PDF: " + std::string(e.what()));
        }
    }

    bool IsValidPDF(const std::string& pdf_path) {
        try {
            PoDoFo::PdfMemDocument document;
            document.Load(pdf_path.c_str());
            return true;
        } catch (...) {
            return false;
        }
    }
};

PDFParser::PDFParser()
    : pimpl_(std::make_unique<PDFParserImpl>()) {
}

PDFParser::~PDFParser() = default;

std::string PDFParser::ExtractText(const std::string& pdf_path) {
    return pimpl_->ExtractText(pdf_path);
}

bool PDFParser::IsValidPDF(const std::string& pdf_path) {
    return pimpl_->IsValidPDF(pdf_path);
}

} // namespace services
} // namespace interview
