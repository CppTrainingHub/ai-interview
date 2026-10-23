#include <algorithm>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "audio/portaudio_audio_device.h"
#include "audio/wav_file.h"

namespace {

/**
 * @brief 将小端序 PCM16 字节转换成有符号采样值
 * @param low 低位字节
 * @param high 高位字节
 * @return int16 采样值
 */
std::int16_t ReadS16Le(unsigned char low, unsigned char high) {
    const auto value = static_cast<std::uint16_t>(low) |
                       static_cast<std::uint16_t>(high << 8U);
    return static_cast<std::int16_t>(value);
}

/**
 * @brief 计算 PCM16 音频中的最大绝对采样值
 * @param pcm_audio 原始 PCM16 little-endian 字节
 * @return 最大绝对采样值，用于粗略判断是否录到非静音
 */
int MaxAbsSample(const std::vector<unsigned char>& pcm_audio) {
    int max_abs = 0;
    for (std::size_t i = 0; i + 1 < pcm_audio.size(); i += 2) {
        const auto sample = static_cast<int>(ReadS16Le(pcm_audio[i], pcm_audio[i + 1]));
        max_abs = std::max(max_abs, std::abs(sample));
    }
    return max_abs;
}

/**
 * @brief 解析正整数参数
 * @param value 参数字符串
 * @param name 参数名称，用于错误提示
 * @return 正整数
 */
int ParsePositiveInt(const std::string& value, const std::string& name) {
    const int parsed = std::stoi(value);
    if (parsed <= 0) {
        throw std::runtime_error(name + " must be positive");
    }
    return parsed;
}

void PrintUsage(const char* program_name) {
    std::cout
        << "Usage:\n"
        << "  " << program_name
        << " [output_wav] [seconds]\n\n"
        << "Example:\n"
        << "  " << program_name << " reports/microphone_test.wav 3\n";
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc > 1 && (std::string(argv[1]) == "--help" ||
                         std::string(argv[1]) == "-h")) {
            PrintUsage(argv[0]);
            return 0;
        }

        const std::string output_path =
            argc > 1 ? argv[1] : "reports/microphone_test.wav";
        const int seconds = argc > 2 ? ParsePositiveInt(argv[2], "seconds") : 3;

        interview::audio::AudioDeviceConfig config;
        config.input_format.sample_rate_hz = 16000;
        config.input_format.channels = 1;
        config.input_frames_per_buffer = 3200;
        config.output_format = config.input_format;
        config.output_frames_per_buffer = config.input_frames_per_buffer;

        interview::audio::PortAudioAudioDevice device(config);
        device.OpenInput();

        const int chunks = seconds * config.input_format.sample_rate_hz /
                           config.input_frames_per_buffer;
        std::vector<unsigned char> recorded;
        recorded.reserve(
            static_cast<std::size_t>(chunks) *
            static_cast<std::size_t>(config.input_frames_per_buffer) *
            config.input_format.BytesPerFrame());

        std::cout << "Recording " << seconds
                  << " second(s). Please speak into the microphone...\n";

        for (int i = 0; i < chunks; ++i) {
            const auto chunk = device.ReadInput();
            recorded.insert(recorded.end(), chunk.begin(), chunk.end());
            std::cout << "chunk " << (i + 1) << "/" << chunks
                      << ", bytes=" << chunk.size() << "\n";
        }

        device.Close();
        interview::audio::WriteWavFile(output_path, recorded, config.input_format);

        std::cout << "Saved WAV: " << output_path << "\n"
                  << "Total bytes: " << recorded.size() << "\n"
                  << "Max abs sample: " << MaxAbsSample(recorded) << "\n";

        if (MaxAbsSample(recorded) == 0) {
            std::cout << "Warning: recorded audio is all silence. Check microphone routing or permissions.\n";
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        PrintUsage(argv[0]);
        return 1;
    }
}
