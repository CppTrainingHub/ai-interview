#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "audio/audio_device.h"
#include "audio/scripted_asr_client.h"
#include "audio/speech_pipeline.h"
#include "audio/text_to_pcm_tts_client.h"
#include "audio/wav_audio_device.h"
#include "audio/wav_file.h"

namespace {

std::vector<unsigned char> GenerateTonePcm(const interview::audio::AudioFormat& format,
                                           int seconds) {
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kFrequencyHz = 660.0;
    constexpr double kAmplitude = 9000.0;

    const int total_frames = format.sample_rate_hz * seconds;
    std::vector<unsigned char> pcm;
    pcm.reserve(static_cast<std::size_t>(total_frames) * format.BytesPerFrame());

    for (int frame = 0; frame < total_frames; ++frame) {
        const double t = static_cast<double>(frame) / format.sample_rate_hz;
        const bool pause = (frame / (format.sample_rate_hz / 2)) % 3 == 2;
        const auto sample = static_cast<short>(
            pause ? 0.0 : std::sin(2.0 * kPi * kFrequencyHz * t) * kAmplitude);

        pcm.push_back(static_cast<unsigned char>(sample & 0xFF));
        pcm.push_back(static_cast<unsigned char>((sample >> 8) & 0xFF));
    }

    return pcm;
}

} // namespace

int main() {
    const std::filesystem::path output_dir = "reports/audio_samples";
    std::filesystem::create_directories(output_dir);

    interview::audio::AudioDeviceConfig config;
    config.input_format.sample_rate_hz = 16000;
    config.input_format.channels = 1;
    config.output_format = config.input_format;
    config.input_frames_per_buffer = 3200;
    config.output_frames_per_buffer = 3200;

    const auto input_path = (output_dir / "cpp_raii_answer_input.wav").string();
    const auto output_path = (output_dir / "cpp_raii_pipeline_output.wav").string();

    const auto input_pcm = GenerateTonePcm(config.input_format, 3);
    interview::audio::WriteWavFile(input_path, input_pcm, config.input_format);

    const std::string answer =
        "RAII means Resource Acquisition Is Initialization. In C plus plus, "
        "a resource is bound to an object's lifetime. The constructor acquires "
        "the resource, and the destructor releases it automatically.";

    interview::audio::WavAudioDevice device(input_path, output_path, config);
    interview::audio::ScriptedASRClient asr({answer});
    interview::audio::TextToPcmTTSClient tts(config.output_format, 20);
    interview::audio::SpeechPipeline pipeline(device, asr, tts);

    pipeline.Start();
    const auto recognized = pipeline.CaptureAnswerText();
    pipeline.SpeakText("Good. Please continue with an example using lock guard.");
    pipeline.Stop();

    const auto generated_input = interview::audio::ReadWavFile(input_path);
    const auto generated_output = interview::audio::ReadWavFile(output_path);

    std::cout << "Generated input WAV: " << input_path << "\n";
    std::cout << "Input bytes: " << generated_input.bytes.size() << "\n";
    std::cout << "Recognized answer text: " << recognized << "\n";
    std::cout << "Generated pipeline output WAV: " << output_path << "\n";
    std::cout << "Output bytes: " << generated_output.bytes.size() << "\n";

    if (generated_input.bytes.empty() || generated_output.bytes.empty() || recognized.empty()) {
        std::cerr << "audio file pipeline demo failed\n";
        return 1;
    }

    return 0;
}
