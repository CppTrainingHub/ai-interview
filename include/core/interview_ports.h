#pragma once

#include <string>
#include <vector>

#include "audio/audio_models.h"
#include "core/interview_models.h"
/**
 * @brief 这部分整体是一个适配器，因为遵循：
 *          核心业务只依赖抽象接口，不依赖第三方库实现。这个原则
 *   比如之后可以像如下这种方式来接：
 *      ResumeParser       -> PoDoFoResumeParser
        QuestionGenerator  -> LLMQuestionGenerator
        AnswerEvaluator    -> LLMAnswerEvaluator
        SummaryGenerator   -> LLMSummaryGenerator
        ReportWriter       -> JsonReportWriter
 */

namespace interview {
namespace core {

/**
 * @brief 简历解析接口
 * @note 这里只需要根据一个路径拿到简历文本，不关心底层是什么库
 */
class ResumeParser {
public:
    virtual ~ResumeParser() = default;

    /**
     * @brief 从简历文件中提取纯文本
     * @param resume_path 简历路径，可能是PDF、TXT或其他外层支持的格式
     * @return 简历纯文本内容
     * @throws 具体实现可以在文件不存在、解析失败时抛出 std::runtime_error
     */
    virtual std::string ExtractText(const std::string& resume_path) = 0;
};

/**
 * @brief 题目生成接口
 * @note 这里抽象成一个接口，后续可以接 LLM,也可以在测试中用MockQuestionGenerator
 */
class QuestionGenerator {
public:
    virtual ~QuestionGenerator() = default;

    /**
     * @brief 根据简历文本生成面试题
     * @param resume_text 简历解析后的纯文本
     * @param min_questions 希望生成的最少题目数量
     * @return 面试题列表，不能为空；具体失败策略由实现类决定
     */
    virtual std::vector<InterviewQuestion> GenerateFromResume( 
        const std::string& resume_text,
        int min_questions
    ) = 0;

    /**
     * @brief 生成默认通用面试题
     * @param min_questions 希望生成的最少题目数量
     * @return 通用题目列表，通常用于没有简历的场景
     */
    virtual std::vector<InterviewQuestion> GenerateDefault(int min_questions) = 0;
};

/**
 * @brief 答案评分接口
 * @note 外部evaluator 负责评分，InterviewSession 只记录评分结果
 */
class AnswerEvaluator {
public:
    virtual ~AnswerEvaluator() = default;

    /**
     * @brief 对候选人的一次回答进行评分
     * @param question 当前问题；如果 is_followup 为 true，仍然传入追问所关联的原始题目
     * @param answer 候选人的回答文本
     * @param is_followup 当前回答是否是追问回答
     * @return 评分结果，包括分数、反馈、是否继续追问
     */
    virtual AnswerEvaluation Evaluate(
        const InterviewQuestion& question,
        const std::string& answer,
        bool is_followup
    ) = 0;
};

/**
 * @brief 面试总结生成接口
 * @note 可以由 LLM 实现，也可以由规则生成，不关心总结生成
 */
class SummaryGenerator {
public:
    virtual ~SummaryGenerator() = default;

    /**
     * @brief 根据所有回答记录生成面试总结
     * @param records 面试过程中的全部回答记录
     * @param resume_text 简历文本，可为空；有简历时可辅助生成更贴合背景的总结
     * @return 面试总结文本，供最终动作或报告使用
     */
    virtual std::string GenerateSummary(
        const std::vector<InterviewRecord>& records,
        const std::string& resume_text
    ) = 0;
};

/**
 * @brief 报告写入接口
 * @note ReportBuilder 负责生成报告数据，ReportWriter 负责写成文件，
 *      这样核心流程不直接依赖 ofstream 或 JSON 库
 */
class ReportWriter {
public:
    virtual ~ReportWriter() = default;

    /**
     * @brief 将结构化报告写入外部存储
     * @param report ReportBuilder 或其他核心逻辑生成的报告数据
     * @param output_path 输出路径；为空时实现类可以自行生成默认路径
     * @return 实际写入的文件路径或资源标识
     */
    virtual std::string WriteReport(
        const InterviewReport& report,
        const std::string& output_path
    ) = 0;
};

/**
 * @brief 语音识别接口
 * @note core 只依赖识别后的文本，不依赖麦克风、音频设备或云服务 SDK。
 */
class ASRClient {
public:
    virtual ~ASRClient() = default;

    /**
     * @brief 将一段 PCM 音频识别为文本
     * @param pcm_audio 原始 PCM 音频字节，具体采样率和格式由外层适配器约定
     * @return 识别得到的文本；无法识别时可以返回空字符串
     * @throws 具体实现可以在音频格式非法、服务不可用或认证失败时抛出 std::runtime_error
     */
    virtual std::string Transcribe(const audio::PcmAudio& pcm_audio) = 0;
};

/**
 * @brief 语音合成接口
 * @note core 只产生要朗读的文本，是否播放、保存或走云端 TTS 由外层实现决定。
 */
class TTSClient {
public:
    virtual ~TTSClient() = default;

    /**
     * @brief 将文本合成为 PCM 音频
     * @param text 待合成文本
     * @return 合成后的 PCM 音频字节；空文本通常返回空数组
     * @throws 具体实现可以在服务不可用或输入非法时抛出 std::runtime_error
     */
    virtual audio::PcmAudio Synthesize(const std::string& text) = 0;
};

} // namespace core
} // namespace interview 
