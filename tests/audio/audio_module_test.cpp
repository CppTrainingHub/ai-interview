#include <cassert>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#include <atomic>

#include "audio/audio_models.h"
#include "audio/answer_audio_capture.h"
#include "audio/async_speech_controller.h"
#include "audio/null_audio_device.h"
#include "audio/realtime_speech_protocol.h"
#include "audio/scripted_asr_client.h"
#include "audio/speech_pipeline.h"
#include "audio/text_to_pcm_tts_client.h"
#include "audio/wav_audio_device.h"
#include "audio/wav_file.h"

using interview::audio::AudioDeviceConfig;
using interview::audio::AudioFormat;
using interview::audio::NullAudioDevice;
using interview::audio::ReadWavFile;
using interview::audio::BuildRealtimeAudioRequest;
using interview::audio::BuildRealtimeFullRequest;
using interview::audio::ExtractFinalAsrText;
using interview::audio::ScriptedASRClient;
using interview::audio::SpeechPipeline;
using interview::audio::TextToPcmTTSClient;
using interview::audio::WavAudioDevice;
using interview::audio::WriteWavFile;
namespace realtime_events = interview::audio::realtime_events;

namespace {

template <typename Operation>
void ExpectRuntimeError(Operation operation) {
    bool thrown = false;
    try {
        operation();
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    assert(thrown);
}

void TestAudioFormatSizes() {
    AudioFormat format;
    assert(format.BytesPerSample() == 2U);
    assert(format.BytesPerFrame() == 2U);

    format.channels = 2;
    assert(format.BytesPerFrame() == 4U);
}

void TestScriptedAsrClient() {
    ScriptedASRClient asr({"first answer", "second answer"});
    interview::audio::PcmAudio audio;
    audio.bytes.assign({0, 1, 2});

    assert(asr.HasPendingTranscripts());
    assert(asr.Transcribe(audio) == "first answer");
    assert(asr.HasPendingTranscripts());
    assert(asr.Transcribe(audio) == "second answer");
    assert(!asr.HasPendingTranscripts());
    assert(asr.Transcribe(audio) == "");
}

void TestTextToPcmTtsClient() {
    AudioFormat format;
    format.sample_rate_hz = 1000;
    format.channels = 1;

    TextToPcmTTSClient tts(format, 10);
    const auto audio = tts.Synthesize("abcd");

    assert(audio.bytes.size() == 80U);
    assert(audio.format.sample_rate_hz == 1000);
    assert(tts.Format().sample_rate_hz == 1000);
    assert(tts.Synthesize("").bytes.empty());

    ExpectRuntimeError([]() {
        TextToPcmTTSClient invalid(AudioFormat{}, 0);
    });
}

void TestWavRoundTrip() {
    AudioFormat format;
    format.sample_rate_hz = 8000;
    format.channels = 1;

    const std::vector<unsigned char> pcm{0, 0, 255, 127, 0, 128, 1, 0};
    const std::string path =
        (std::filesystem::temp_directory_path() / "ai_interview_audio_test.wav").string();

    WriteWavFile(path, pcm, format);
    const auto loaded = ReadWavFile(path);

    assert(loaded.format.sample_rate_hz == format.sample_rate_hz);
    assert(loaded.format.channels == format.channels);
    assert(std::vector<unsigned char>(loaded.bytes.begin(), loaded.bytes.end()) == pcm);

    ExpectRuntimeError([&format]() {
        WriteWavFile("/tmp/ai_interview_bad.wav", {0}, format);
    });
}

void TestNullAudioDevice() {
    AudioDeviceConfig config;
    config.input_format.sample_rate_hz = 8000;
    config.output_format.sample_rate_hz = 8000;
    config.input_frames_per_buffer = 4;

    NullAudioDevice device(config);
    device.OpenInput();
    device.OpenOutput();

    const std::vector<unsigned char> queued{0, 0, 1, 0};
    device.QueueInput(queued);
    assert(device.ReadInput() == queued);
    assert(device.ReadInput().size() == 8U);

    device.WriteOutput({0, 0, 1, 0});
    assert(device.WrittenOutput().size() == 4U);
    device.Close();

    ExpectRuntimeError([&device]() {
        device.ReadInput();
    });
}

void TestSpeechPipeline() {
    AudioDeviceConfig config;
    config.input_format.sample_rate_hz = 1000;
    config.output_format.sample_rate_hz = 1000;
    config.input_frames_per_buffer = 2;

    NullAudioDevice device(config);
    ScriptedASRClient asr({"pipeline answer"});
    TextToPcmTTSClient tts(config.output_format, 10);
    SpeechPipeline pipeline(device, asr, tts);

    pipeline.Start();
    device.QueueInput({0, 0, 1, 0});
    assert(pipeline.CaptureAnswerText() == "pipeline answer");

    pipeline.SpeakText("ok");
    assert(!device.WrittenOutput().empty());
    pipeline.Stop();
    assert(!pipeline.IsStarted());
    ExpectRuntimeError([&pipeline]() { pipeline.CaptureAnswerText(); });
}

void TestManualMultiChunkCapture() {
    AudioDeviceConfig config;
    config.input_format.sample_rate_hz = 1000;
    config.output_format.sample_rate_hz = 1000;
    config.input_frames_per_buffer = 2;

    NullAudioDevice device(config);
    device.OpenInput();
    interview::audio::AnswerAudioCapture capture(device);
    device.QueueInput({0, 0, 1, 0});
    device.QueueInput({2, 0, 3, 0});

    capture.Begin();
    capture.CaptureNextChunk();
    capture.CaptureNextChunk();
    const auto audio = capture.Finish();
    assert(audio.bytes.size() == 8U);
    assert(audio.format.sample_rate_hz == 1000);
    assert(!capture.IsCapturing());

    capture.Begin();
    capture.Cancel();
    assert(!capture.IsCapturing());
    device.Close();
}

void TestAsyncSpeechController() {
    AudioDeviceConfig config;
    config.input_format.sample_rate_hz = 1000;
    config.output_format.sample_rate_hz = 1000;
    config.input_frames_per_buffer = 2;
    NullAudioDevice device(config);
    ScriptedASRClient asr({"async answer"});
    TextToPcmTTSClient tts(config.output_format, 1);
    SpeechPipeline pipeline(device, asr, tts);
    pipeline.Start();

    interview::audio::AsyncSpeechController controller(pipeline);
    std::atomic<bool> capturing{false};
    std::string transcript;
    controller.SetStateCallback([&capturing](interview::audio::AsyncSpeechState state) {
        if (state == interview::audio::AsyncSpeechState::kCapturing) {
            capturing = true;
        }
    });
    controller.StartCapture([&transcript](const std::string& value) { transcript = value; });
    while (!capturing.load()) {
    }
    controller.RequestStopCapture();
    controller.Wait();
    assert(transcript == "async answer");

    bool spoken = false;
    controller.Speak("hello", [&spoken]() { spoken = true; });
    controller.Wait();
    assert(spoken);
    assert(controller.State() == interview::audio::AsyncSpeechState::kIdle);
    pipeline.Stop();
}

void TestRealtimeSpeechProtocolHelpers() {
    const auto full_request = BuildRealtimeFullRequest(
        realtime_events::kStartSession,
        "session-1",
        nlohmann::json{{"hello", "world"}}
    );
    assert(full_request.size() > 20U);

    const auto audio_request = BuildRealtimeAudioRequest(
        realtime_events::kTaskRequest,
        "session-1",
        {0, 1, 2, 3}
    );
    assert(audio_request.size() > 20U);

    const nlohmann::json final_payload = {
        {"results", nlohmann::json::array({
            nlohmann::json{{"is_interim", false}, {"text", "最终回答"}}
        })}
    };
    assert(ExtractFinalAsrText(final_payload) == "最终回答");

    const nlohmann::json interim_payload = {
        {"results", nlohmann::json::array({
            nlohmann::json{{"is_interim", true}, {"text", "临时回答"}}
        })}
    };
    assert(ExtractFinalAsrText(interim_payload).empty());
}

void TestWavAudioDevice() {
    AudioDeviceConfig config;
    config.input_format.sample_rate_hz = 8000;
    config.output_format.sample_rate_hz = 8000;
    config.input_frames_per_buffer = 2;

    const auto dir = std::filesystem::temp_directory_path();
    const std::string input_path = (dir / "ai_interview_audio_input.wav").string();
    const std::string output_path = (dir / "ai_interview_audio_output.wav").string();
    const std::vector<unsigned char> input_pcm{0, 0, 1, 0, 2, 0, 3, 0};

    WriteWavFile(input_path, input_pcm, config.input_format);

    WavAudioDevice device(input_path, output_path, config);
    device.OpenInput();
    device.OpenOutput();

    assert(device.ReadInput() == std::vector<unsigned char>({0, 0, 1, 0}));
    assert(device.ReadInput() == std::vector<unsigned char>({2, 0, 3, 0}));
    device.WriteOutput({4, 0, 5, 0});
    device.Close();

    const auto output = ReadWavFile(output_path);
    assert(std::vector<unsigned char>(output.bytes.begin(), output.bytes.end()) ==
           std::vector<unsigned char>({4, 0, 5, 0}));
}

} // namespace

int main() {
    TestAudioFormatSizes();
    TestScriptedAsrClient();
    TestTextToPcmTtsClient();
    TestWavRoundTrip();
    TestNullAudioDevice();
    TestSpeechPipeline();
    TestManualMultiChunkCapture();
    TestAsyncSpeechController();
    TestRealtimeSpeechProtocolHelpers();
    TestWavAudioDevice();
    return 0;
}
