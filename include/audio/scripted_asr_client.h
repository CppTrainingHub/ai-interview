#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/interview_ports.h"

namespace interview {
namespace audio {

/**
 * @brief 基于预置文本队列的 ASR 实现
 * @note 该类用于 CLI 自动化、单元测试和无麦克风环境，不访问音频设备或外部服务。
 */
class ScriptedASRClient final : public core::ASRClient {
public:
    /**
     * @brief 创建脚本化 ASR 客户端
     * @param transcripts 按调用顺序返回的识别文本列表
     */
    explicit ScriptedASRClient(std::vector<std::string> transcripts);

    ~ScriptedASRClient() override;

    ScriptedASRClient(const ScriptedASRClient&) = delete;
    ScriptedASRClient& operator=(const ScriptedASRClient&) = delete;

    ScriptedASRClient(ScriptedASRClient&&) noexcept;
    ScriptedASRClient& operator=(ScriptedASRClient&&) noexcept;

    /**
     * @brief 返回下一条预置识别结果
     * @param pcm_audio 原始 PCM 音频字节；该实现仅校验调用流程，不解析内容
     * @return 下一条预置文本；队列耗尽时返回空字符串
     */
    std::string Transcribe(const PcmAudio& pcm_audio) override;

    /**
     * @brief 查询是否仍有未消费的脚本文本
     * @return 有剩余文本时返回 true
     */
    bool HasPendingTranscripts() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace audio
} // namespace interview
