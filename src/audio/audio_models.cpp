#include "audio/audio_models.h"

namespace interview {
namespace audio {

std::size_t AudioFormat::BytesPerSample() const {
    switch (sample_format) {
        case SampleFormat::kPcmS16Le:
            return 2;
    }

    return 0;
}

std::size_t AudioFormat::BytesPerFrame() const {
    return static_cast<std::size_t>(channels) * BytesPerSample();
}

} // namespace audio
} // namespace interview
