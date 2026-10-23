#include <stdexcept>
#include <utility>

#include "audio/text_to_pcm_tts_client.h"

namespace interview {
namespace audio {
namespace {

void ValidateFormat(const AudioFormat& format) {
    if (format.sample_rate_hz <= 0) {
        throw std::runtime_error("audio sample_rate_hz must be positive");
    }
    if (format.channels <= 0) {
        throw std::runtime_error("audio channels must be positive");
    }
    if (format.sample_format != SampleFormat::kPcmS16Le) {
        throw std::runtime_error("only 16-bit little-endian PCM is supported");
    }
}

} // namespace

class TextToPcmTTSClient::Impl {
public:
    Impl(AudioFormat format, int milliseconds_per_character)
        : format_(format),
          milliseconds_per_character_(milliseconds_per_character) {
        ValidateFormat(format_);
        if (milliseconds_per_character_ <= 0) {
            throw std::runtime_error("milliseconds_per_character must be positive");
        }
    }

    PcmAudio Synthesize(const std::string& text) const {
        PcmAudio audio;
        audio.format = format_;
        if (text.empty()) {
            return audio;
        }

        const auto frames =
            static_cast<std::size_t>(format_.sample_rate_hz) *
            static_cast<std::size_t>(milliseconds_per_character_) *
            text.size() / 1000U;
        audio.bytes.assign(frames * format_.BytesPerFrame(), 0);
        return audio;
    }

    AudioFormat Format() const {
        return format_;
    }

private:
    AudioFormat format_;
    int milliseconds_per_character_;
};

TextToPcmTTSClient::TextToPcmTTSClient(AudioFormat format,
                                       int milliseconds_per_character)
    : pImpl(std::make_unique<Impl>(format, milliseconds_per_character)) {}

TextToPcmTTSClient::~TextToPcmTTSClient() = default;

TextToPcmTTSClient::TextToPcmTTSClient(TextToPcmTTSClient&&) noexcept = default;

TextToPcmTTSClient& TextToPcmTTSClient::operator=(TextToPcmTTSClient&&) noexcept = default;

PcmAudio TextToPcmTTSClient::Synthesize(const std::string& text) {
    return pImpl->Synthesize(text);
}

AudioFormat TextToPcmTTSClient::Format() const {
    return pImpl->Format();
}

} // namespace audio
} // namespace interview
