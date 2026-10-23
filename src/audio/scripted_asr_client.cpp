#include <deque>
#include <utility>

#include "audio/scripted_asr_client.h"

namespace interview {
namespace audio {

class ScriptedASRClient::Impl {
public:
    explicit Impl(std::vector<std::string> transcripts)
        : transcripts_(transcripts.begin(), transcripts.end()) {}

    std::string Transcribe(const PcmAudio&) {
        if (transcripts_.empty()) {
            return "";
        }

        std::string next = std::move(transcripts_.front());
        transcripts_.pop_front();
        return next;
    }

    bool HasPendingTranscripts() const {
        return !transcripts_.empty();
    }

private:
    std::deque<std::string> transcripts_;
};

ScriptedASRClient::ScriptedASRClient(std::vector<std::string> transcripts)
    : pImpl(std::make_unique<Impl>(std::move(transcripts))) {}

ScriptedASRClient::~ScriptedASRClient() = default;

ScriptedASRClient::ScriptedASRClient(ScriptedASRClient&&) noexcept = default;

ScriptedASRClient& ScriptedASRClient::operator=(ScriptedASRClient&&) noexcept = default;

std::string ScriptedASRClient::Transcribe(const PcmAudio& pcm_audio) {
    return pImpl->Transcribe(pcm_audio);
}

bool ScriptedASRClient::HasPendingTranscripts() const {
    return pImpl->HasPendingTranscripts();
}

} // namespace audio
} // namespace interview
