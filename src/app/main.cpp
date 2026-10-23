#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#include "app/cli_app.h"
#include "config/app_config.h"

namespace {

struct CliOptions {
    std::string config_path;
    std::string resume_path;
    std::string candidate_name = "Candidate";
    int min_questions = 5;
};

void PrintUsage(const char* program_name) {
    std::cout
        << "Usage:\n"
        << "  " << program_name
        << " [--config path] [--resume path] [--name candidate] [--questions n]\n\n"
        << "Default config file:\n"
        << "  config/app.local.env\n\n"
        << "Environment variables:\n"
        << "  AI_INTERVIEW_LLM_API_URL\n"
        << "  AI_INTERVIEW_LLM_API_KEY\n"
        << "  AI_INTERVIEW_LLM_MODEL\n"
        << "  AI_INTERVIEW_REPORT_DIR\n"
        << "  AI_INTERVIEW_RUNTIME_MODE=text|audio\n"
        << "  AI_INTERVIEW_DEPENDENCY_MODE=openai|scripted\n";
}

CliOptions ParseArgs(int argc, char* argv[]) {
    CliOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            std::exit(0);
        }

        if (arg == "--config" && i + 1 < argc) {
            options.config_path = argv[++i];
        } else if (arg == "--resume" && i + 1 < argc) {
            options.resume_path = argv[++i];
        } else if (arg == "--name" && i + 1 < argc) {
            options.candidate_name = argv[++i];
        } else if (arg == "--questions" && i + 1 < argc) {
            options.min_questions = std::stoi(argv[++i]);
            if (options.min_questions <= 0) {
                throw std::runtime_error("--questions must be positive");
            }
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
        auto config = interview::config::LoadAppConfig(options.config_path);

        interview::app::CliApp app(std::move(config));
        return app.Run(
            options.resume_path,
            options.candidate_name,
            options.min_questions
        );
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        PrintUsage(argv[0]);
        return 1;
    }
}
