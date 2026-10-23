#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#include "config/app_config.h"
#include "net/http_interview_server.h"

namespace {

struct HttpServerCliOptions {
    std::string config_path;
    std::string host;
    int port = 0;
};

void PrintUsage(const char* program_name) {
    std::cout
        << "Usage:\n"
        << "  " << program_name
        << " [--config 配置文件路径] [--host 监听地址] [--port 监听端口]\n\n"
        << "Example:\n"
        << "  " << program_name
        << " --config config/app.local.env --host 127.0.0.1 --port 8080\n\n"
        << "HTTP routes:\n"
        << "  GET  /health\n"
        << "  POST /interview/start   JSON: {\"candidate_name\":\"候选人姓名\",\"resume_path\":\"简历路径，可为空\",\"min_questions\":5}\n"
        << "  POST /interview/answer  JSON: {\"session_id\":\"启动面试返回的会话ID\",\"answer\":\"候选人回答文本\"}\n"
        << "  POST /interview/report  JSON: {\"session_id\":\"启动面试返回的会话ID\"}\n\n"
        << "Required business config:\n"
        << "  AI_INTERVIEW_LLM_API_URL=大模型接口地址\n"
        << "  AI_INTERVIEW_LLM_API_KEY=大模型接口密钥\n"
        << "  AI_INTERVIEW_LLM_MODEL=大模型名称\n"
        << "  AI_INTERVIEW_REPORT_DIR=报告输出目录\n";
}

HttpServerCliOptions ParseArgs(int argc, char* argv[]) {
    HttpServerCliOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            std::exit(0);
        }

        if (arg == "--config" && i + 1 < argc) {
            options.config_path = argv[++i];
        } else if (arg == "--host" && i + 1 < argc) {
            options.host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            options.port = std::stoi(argv[++i]);
        } else {
            throw std::runtime_error("unknown or incomplete argument: " + arg);
        }
    }

    return options;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        const auto options = ParseArgs(argc, argv);
        auto app_config = interview::config::LoadAppConfig(options.config_path);

        interview::net::HttpServerConfig http_config;
        http_config.host = options.host.empty() ? app_config.http_host : options.host;
        http_config.port = options.port == 0 ? app_config.http_port : options.port;

        interview::net::HttpInterviewServer server(std::move(app_config), http_config);
        std::cout << "AI Interview HTTP server listening on "
                  << http_config.host << ":" << http_config.port << "\n";
        server.Run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        PrintUsage(argv[0]);
        return 1;
    }
}
