#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "app/cli_app.h"
#include "runtime/interview_runtime.h"

namespace interview {
namespace app {
namespace {

/**
 * @brief 将面试动作打印到 CLI 终端
 * @param action core 层返回的面试动作，包含动作类型和展示文本
 * @note 该函数只负责命令行展示，不推进面试状态，也不调用 LLM、TTS 或报告模块
 */
void PrintAction(const core::InterviewAction& action) {
    std::cout << "\n";

    switch (action.type) {
        case core::InterviewActionType::kSpeakIntro:
            std::cout << "[Intro] " << action.text << "\n";
            break;
        case core::InterviewActionType::kAskQuestion:
            std::cout << "[Question] " << action.text << "\n";
            break;
        case core::InterviewActionType::kAskFollowUp:
            std::cout << "[FollowUp] " << action.text << "\n";
            break;
        case core::InterviewActionType::kFinish:
            std::cout << "[Summary]\n" << action.text << "\n";
            break;
        case core::InterviewActionType::kNone:
            break;
    }
}

} // namespace

class CliApp::Impl {
public:
    explicit Impl(config::AppConfig app_config) : config(std::move(app_config)){}

    // 实现 Run 函数
    int Run(const std::string& resume_path,
            const std::string& candidate_name,
            int min_questions) {
        if (min_questions <= 0) {
            throw std::runtime_error("min_questions must be positive");
        }

        runtime::InterviewRuntime runtime(config, candidate_name);

        auto action = runtime.Start(resume_path, min_questions);
        PrintAction(action);
        runtime.SpeakAction(action);

        while (action.type != core::InterviewActionType::kFinish) {
            if (config.runtime_mode == "audio") {
                action = runtime.CaptureAndSubmitAudioAnswer();
            } else {
                std::cout << "\nCandidate> ";
                std::string answer;

                if (!std::getline(std::cin, answer)) {
                    throw std::runtime_error("failed to read candidate answer");
                }

                action = runtime.SubmitTextAnswer(answer);
            }
            PrintAction(action);
            runtime.SpeakAction(action);
        }

        const std::string written_path = runtime.GenerateReport("cli");

        std::cout << "\nReport written to: " << written_path << "\n";
        return 0;
    }
private:
    config::AppConfig config;
};


CliApp::CliApp(config::AppConfig config)
    : pImpl(std::make_unique<Impl>(std::move(config))) {}

CliApp::~CliApp() = default;

CliApp::CliApp(CliApp&&) noexcept = default;

CliApp& CliApp::operator=(CliApp&&) noexcept = default;

int CliApp::Run(const std::string& resume_path,
                const std::string& candidate_name,
                int min_questions) {
    return pImpl->Run(resume_path, candidate_name, min_questions);
}
} // namespace app
} // namespace interview
