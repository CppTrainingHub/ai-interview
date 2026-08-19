#include "interview/dialog_session.h"
#include "services/audio_manager.h"
#include "services/realtime_client.h"
#include "interview/interview_manager.h"
#include "common/config.h"
#include "common/logger.h"
#include "common/interview_state.h"
#include "common/protocol.h"
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <sstream>
#include <string>

namespace interview {
namespace session {

class DialogSession::DialogSessionImpl {
public:
    std::unique_ptr<services::AudioDeviceManager> audio_manager;
    std::unique_ptr<services::RealtimeClient> realtime_client;
    std::shared_ptr<InterviewSession> interview_session;
    
    // 流程状态
    bool is_intro_done;
    std::atomic<bool> is_running;
    std::atomic<bool> is_playing_audio;
    std::atomic<int> tts_cnt;

    // 待处理的答案
    std::string pending_answer;
    std::mutex pending_answer_mutex;

    std::string final_summary;
    std::mutex final_summary_mutex;

    // 对话内容回调
    DialogSession::DialogContentCallback dialog_content_callback;

    std::thread microphone_thread;
    std::thread playback_thread;

    std::queue<std::vector<float>> audio_queue;
    std::mutex audio_queue_mutex;
    std::condition_variable audio_queue_cv;

    DialogSessionImpl(const std::string& name)
        : is_intro_done(false)
        , is_running(false)
        , is_playing_audio(false)
        , tts_cnt(0) {

        interview_session = std::make_shared<InterviewSession>(name);
        
        // 初始化全局状态机
        common::InterviewStateMachine::Instance().Reset();
    }

    ~DialogSessionImpl() {
        Stop();
    }

    void TransitionToState(common::InterviewState new_state) {
        common::InterviewStateMachine::Instance().SetState(new_state);
        LOG_INFO("状态: %s", common::InterviewStateMachine::GetStateName(new_state));
    }

    void Start() {
        if (is_running) {
            LOG_WARNING("Session already running");
            return;
        }

        auto& cfg = common::Config::Instance();

        // 创建音频管理器
        services::AudioConfig input_cfg;
        input_cfg.sample_rate = cfg.input_audio_config.sample_rate;
        input_cfg.channels = cfg.input_audio_config.channels;
        input_cfg.chunk = cfg.input_audio_config.chunk;

        services::AudioConfig output_cfg;
        output_cfg.sample_rate = cfg.output_audio_config.sample_rate;
        output_cfg.channels = cfg.output_audio_config.channels;
        output_cfg.chunk = cfg.output_audio_config.chunk;

        audio_manager = std::make_unique<services::AudioDeviceManager>(input_cfg, output_cfg);

        // 创建WebSocket客户端
        auto& ws_cfg = cfg.ws_config;
        realtime_client = std::make_unique<services::RealtimeClient>(ws_cfg.base_url, ws_cfg.headers);

        // 设置响应回调
        realtime_client->SetResponseCallback([this](const common::ParsedResponse& resp) {
            HandleServerResponse(resp);
        });

        // 连接到服务器
        LOG_INFO("Connecting to server...");
        TransitionToState(common::InterviewState::kConnecting);
        realtime_client->Connect();

        // 等待一小段时间让连接稳定
        std::this_thread::sleep_for(common::timing::CONNECTION_STABILIZE_DELAY);

        // 打开音频流
        LOG_INFO("Opening audio streams...");
        audio_manager->OpenInputStream();
        audio_manager->OpenOutputStream();

        is_running = true;

        // 启动播放线程
        playback_thread = std::thread([this]() {
            PlaybackThreadFunc();
        });

        // 启动麦克风线程
        microphone_thread = std::thread([this]() {
            MicrophoneThreadFunc();
        });

        is_playing_audio = true;
        
        // 发送开场白
        if (dialog_content_callback) {
            dialog_content_callback("interviewer", "你好，欢迎参加今天的面试，先做一个简单的自我介绍。", 0);
        }
        std::string intro = interview_session->GetIntroPrompt();
        SendInterviewerPrompt(intro);

        LOG_INFO("Dialog session started");
    }

    void Stop() {
        if (!is_running) {
            return;
        }

        LOG_INFO("Stopping dialog session");
        is_running = false;

        audio_queue_cv.notify_all();

        // 等待线程结束
        if (microphone_thread.joinable()) {
            microphone_thread.join();
        }

        if (playback_thread.joinable()) {
            playback_thread.join();
        }

        // 关闭连接
        if (realtime_client) {
            realtime_client->Close();
        }

        // 清理音频
        if (audio_manager) {
            audio_manager->Cleanup();
        }

        // 保存面试报告
        if (interview_session) {
            try {
                std::string report_file = interview_session->SaveReport();
                LOG_INFO("Interview report saved: {}", report_file);
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to save report: {}", e.what());
            }
        }

        if (interview_session) {
            std::string summary_copy;
            {
                std::lock_guard<std::mutex> summary_lock(final_summary_mutex);
                summary_copy = final_summary;
            }

            if (summary_copy.empty() && interview_session) {
                try {
                    summary_copy = interview_session->GenerateSummary();
                    if (!summary_copy.empty()) {
                        std::lock_guard<std::mutex> summary_lock(final_summary_mutex);
                        final_summary = summary_copy;
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("Failed to generate summary during shutdown: {}", e.what());
                }
            }

            if (!summary_copy.empty()) {
                LogMultilineBlock("[面试总结回顾]", summary_copy);
            }
        }

        LOG_INFO("Dialog session stopped");
    }

    void HandleServerResponse(const common::ParsedResponse& response) {

        if (response.message_type == "SERVER_ACK" && !response.payload_bytes.empty()) {
            // 服务器返回的PCM格式是Float32
            size_t float_count = response.payload_bytes.size() / sizeof(float);
            const float* float_data = reinterpret_cast<const float*>(response.payload_bytes.data());

            std::vector<float> audio_float(float_data, float_data + float_count);

            // LOG_DEBUG("Received audio: {} bytes = {} float32 samples", response.payload_bytes.size(), float_count);

            // 加入播放队列
            {
                std::lock_guard<std::mutex> lock(audio_queue_mutex);
                audio_queue.push(audio_float);
            }
            audio_queue_cv.notify_one();
            return;
        }

        // 服务器事件
        if (response.message_type == "SERVER_FULL_RESPONSE") {
            // LOG_DEBUG("Received event: {} with payload size: {}", response.event, response.payload.dump().size());
            HandleEvent(response.event, response.payload);
        } else if (response.message_type == "SERVER_ERROR_RESPONSE" || response.message_type == "SERVER_ERROR") {
            LOG_ERROR("========================================");
            LOG_ERROR("[服务器错误]");
            LOG_ERROR("错误码: {}", response.code);
            LOG_ERROR("错误详情: {}", response.payload.dump(2));
            LOG_ERROR("========================================");

            // 服务器错误时，如果正在等待TTS响应，切换到空闲状态
            if (is_playing_audio && tts_cnt == 0) {
                LOG_WARNING("服务器错误导致无法播放TTS，切换到空闲状态");
                is_playing_audio = false;
                // TransitionToState(common::InterviewState::kIdle);
            }
        } else {
            LOG_WARNING("Received unexpected message type: {}", response.message_type);
        }
    }

    void HandleEvent(int event, const nlohmann::json& payload) {

        // Event TTS_START: TTS开始
        if (event == common::events::TTS_START) {
            // 增加TTS_START计数
            tts_cnt++;
            // 确保麦克风已静音,然后转换状态
            is_playing_audio = true;
            TransitionToState(common::InterviewState::kInterviewerSpeaking);
        }
        // Event TTS_END: TTS结束（但音频队列可能还有数据）
        else if (event == common::events::TTS_END) {
            if (tts_cnt > 0) {
                tts_cnt--;
                // 注意：不要在这里立即切换到 kIdle 状态
                // 因为音频队列中可能还有数据在播放
                // 状态切换应该在 PlaybackThreadFunc 中音频队列真正为空时处理
                LOG_DEBUG("TTS_END received, tts_cnt now: {}", tts_cnt.load());
            } 
        }
        // Event USER_START_SPEAKING: 用户开始说话
        else if (event == common::events::USER_START_SPEAKING) {
            // 如果面试官正在说话，忽略此事件（可能是误触发）
            if (is_playing_audio) {
                LOG_WARNING("[忽略] USER_START_SPEAKING during TTS playback (likely false trigger)");
                return;
            }
            
            TransitionToState(common::InterviewState::kCandidateSpeaking);
            // 清空播放队列
            {
                std::lock_guard<std::mutex> lock(audio_queue_mutex);
                while (!audio_queue.empty()) {
                    audio_queue.pop();
                }
            }
            tts_cnt = 0; // 强制重置计数器，立即结束TTS状态
        }
        // Event ASR_RESULT: ASR识别结果
        else if (event == common::events::ASR_RESULT && !payload.empty()) {
            // 如果面试官正在说话，忽略ASR结果（应该是静音数据的误识别）
            if (is_playing_audio) {
                LOG_DEBUG("[忽略] ASR_RESULT during TTS playback");
                return;
            }
            
            auto results = payload.value("results", nlohmann::json::array());
            if (!results.empty()) {
                auto result = results[0];
                bool is_final = !result.value("is_interim", true);
                std::string text = result.value("text", std::string());

                if (is_final) {
                    if (dialog_content_callback) {
                        int qidx = interview_session ? interview_session->GetCurrentQuestionIndex() + 1 : 1;
                        dialog_content_callback("candidate", text, qidx);
                    }
                    
                    {
                        std::lock_guard<std::mutex> lock(pending_answer_mutex);
                        pending_answer = std::move(text);
                    }
                    TransitionToState(common::InterviewState::kInterviewerThinking);
                } else {
                    LOG_DEBUG("[ASR临时结果]: {}", text);
                }
            } else {
                LOG_DEBUG("ASR_RESULT event received but results array is empty");
            }
        }
        // Event USER_STOP_SPEAKING: 用户说话结束
        else if (event == common::events::USER_STOP_SPEAKING) {
            // 如果面试官正在说话，忽略此事件（可能是误触发）
            if (is_playing_audio) {
                LOG_WARNING("[忽略] USER_STOP_SPEAKING during TTS playback (likely false trigger)");
                return;
            }
            TransitionToState(common::InterviewState::kInterviewerThinking);
            LOG_INFO("[候选人说话结束]");
        }
        // Event SESSION_FINISHED/SESSION_ENDED: 会话结束
        else if (event == common::events::SESSION_FINISHED || event == common::events::SESSION_ENDED) {
            LOG_INFO("Session finished event: {}", event);
            TransitionToState(common::InterviewState::kSessionEnding);
            Stop();
            TransitionToState(common::InterviewState::kCompleted);
        }
    }

    void MicrophoneThreadFunc() {
        LOG_INFO("Microphone thread started");

        while (is_running) {
            try {
                // 读取麦克风数据（始终读取以清空缓冲区）
                auto audio_data = audio_manager->ReadAudio();

                // 准备要发送的音频数据
                std::vector<uint8_t> audio_bytes(audio_data.size() * 2);

                if (!is_playing_audio) {
                    // 面试官不说话时：发送真实的麦克风录音
                    std::memcpy(audio_bytes.data(), audio_data.data(), audio_bytes.size());
                } else {
                    // 面试官说话时：发送静音数据（全零），保持音频流连续性避免服务器超时
                    std::memset(audio_bytes.data(), 0, audio_bytes.size());
                }

                // 发送到服务器（持续发送以避免 DialogAudioIdleTimeoutError）
                realtime_client->SendAudioData(audio_bytes);

                // 定期打印麦克风正在工作的日志（每5秒打印一次）
                static auto last_log_time = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::seconds>(now - last_log_time).count() >= 5) {
                    if (!is_playing_audio) {
                        LOG_DEBUG("Microphone active - sending real audio data");
                    } else {
                        LOG_DEBUG("Microphone muted - sending silence data");
                    }
                    last_log_time = now;
                }

                std::this_thread::sleep_for(common::timing::AUDIO_SEND_INTERVAL);

            } catch (const std::exception& e) {
                LOG_ERROR("Microphone thread error: {}", e.what());
                std::this_thread::sleep_for(common::timing::ERROR_RETRY_DELAY);
            }
        }

        LOG_INFO("Microphone thread stopped");
    }

    void PlaybackThreadFunc() {
        LOG_INFO("Playback thread started");

        while (is_running) {
            std::vector<float> audio_data;
            bool queue_empty = false;

            {
                std::unique_lock<std::mutex> lock(audio_queue_mutex);
                audio_queue_cv.wait_for(lock, common::timing::AUDIO_QUEUE_WAIT, [this]() {
                    return !audio_queue.empty() || !is_running;
                });

                if (!is_running) {
                    break;
                }

                if (!audio_queue.empty()) {
                    audio_data = audio_queue.front();
                    audio_queue.pop();
                } else {
                    queue_empty = true;
                }
            }

            if (!audio_data.empty()) {
                try {
                    audio_manager->WriteAudio(audio_data);
                } catch (const std::exception& e) {
                    LOG_ERROR("Playback error: {}", e.what());
                }
            }

            if (queue_empty && tts_cnt == 0) {
                bool was_playing_audio = is_playing_audio.exchange(false);

                if (was_playing_audio) {
                    LOG_INFO("[面试官说完了，请候选人回答] - 麦克风已恢复录音");
                    TransitionToState(common::InterviewState::kIdle);
                }
                // 检查是否有待处理的答案
                ProcessPendingAnswer();
            }
        }

        LOG_INFO("Playback thread stopped");
    }

    bool IsRunning() const {
        return is_running;
    }

    void ProcessPendingAnswer() {
        if (!interview_session) {
            return;
        }

        std::string answer_to_process;
        {
            std::lock_guard<std::mutex> lock(pending_answer_mutex);
            if (pending_answer.empty()) {
                return;
            }
            answer_to_process = std::move(pending_answer);
            pending_answer.clear();
        }

        if (answer_to_process.empty()) {
            return;
        }

        // 处理开场白后的自我介绍
        if (!is_intro_done) {
            is_intro_done = true;
            std::string prompt = interview_session->GetFirstQuestion();
            SendNextPrompt(prompt);
            return;
        }

        // 记录回答并评分
        interview_session->RecordAnswer(answer_to_process);
        
        // 判断是否需要追问
        std::string next_prompt;
        
        if (interview_session->ShouldFollowUp()) {
            next_prompt = interview_session->GetFollowUpQuestion();
        } else if (!interview_session->IsComplete()) {
            next_prompt = interview_session->GetNextQuestion();
        } else {
            std::string summary = interview_session->GenerateSummary();
            {
                std::lock_guard<std::mutex> summary_lock(final_summary_mutex);
                final_summary = summary;
            }
            
            SendNextPrompt(summary);
            TransitionToState(common::InterviewState::kIdle);
            // 保证总结说完后停止 因为语音异步，所以这里延迟一段时间后停止
            std::thread([this]() {
                std::this_thread::sleep_for(common::timing::INTERVIEW_END_DELAY);
                Stop();
            }).detach();
            return;
        }

        SendNextPrompt(next_prompt);
    }
    
    void SendNextPrompt(const std::string& prompt) {
        is_playing_audio = true;
        int qidx = interview_session ? interview_session->GetCurrentQuestionIndex() + 1 : 1;
        if (dialog_content_callback) {
            dialog_content_callback("interviewer", prompt, qidx);
        }
        SendInterviewerPrompt(prompt);
    }

    void SendInterviewerPrompt(const std::string& text) {
        if (!realtime_client) {
            LOG_WARNING("Realtime client not ready, cannot send prompt");
            return;
        }
        if (text.empty()) {
            LOG_WARNING("Attempted to send empty interviewer prompt");
            return;
        }
        
        // 再发送到服务器进行TTS
        realtime_client->SendTextQuery(text);
    }

    void LogMultilineBlock(const std::string& title, const std::string& text) const {
        LOG_INFO("========================================");
        LOG_INFO("{}", title);
        LOG_INFO("========================================");

        std::istringstream iss(text);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            LOG_INFO("{}", line);
        }

        LOG_INFO("========================================");
    }
};

DialogSession::DialogSession(const std::string& candidate_name)
    : pimpl_(std::make_unique<DialogSessionImpl>(candidate_name)) {}

DialogSession::~DialogSession() = default;

void DialogSession::ConfigureResumeInterview(const std::string& resume_pdf_path, int min_questions) {
    pimpl_->interview_session->LoadQuestionsFromResume(resume_pdf_path, min_questions);
}

void DialogSession::ConfigureDefaultInterview(int min_questions) {
    pimpl_->interview_session->GenerateDefaultQuestions(min_questions);
}

void DialogSession::Start() {
    pimpl_->Start();
}

void DialogSession::SetDialogContentCallback(DialogContentCallback callback) {
    pimpl_->dialog_content_callback = std::move(callback);
}

void DialogSession::HandleServerResponse(const common::ParsedResponse& response) {
    pimpl_->HandleServerResponse(response);
}

void DialogSession::Stop() {
    pimpl_->Stop();
}

bool DialogSession::IsRunning() const {
    return pimpl_->IsRunning();
}

} // namespace session
} // namespace interview

