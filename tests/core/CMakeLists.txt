# 构建 InterviewSession 的单元测试可执行文件
# 该测试主要验证面试会话状态、题目推进、回答记录、追问状态等核心逻辑
add_executable(interview_session_test
    interview_session_test.cpp
)

# 测试目标需要调用 ai_interview_core 中的核心代码，因此链接核心库
# PRIVATE 表示这个链接关系只对 interview_session_test 自己有效，不会继续传递给其他目标。
target_link_libraries(interview_session_test
    PRIVATE
        ai_interview_core
)

# 将 interview_session_test 注册到 CTest
# 之后可以通过 ctest 或 ctest --preset linux-debug 统一运行该测试
add_test(
    NAME interview_session_test
    COMMAND interview_session_test
)

# 构建 InterviewOrchestrator 的单元测试可执行文件
# 该测试主要验证面试流程编排、默认题目模式、简历驱动模式、
# 评分调用、追问分支和结束总结等逻辑
add_executable(interview_orchestrator_test
    interview_orchestrator_test.cpp
)

# Orchestrator 测试同样依赖核心库中的 InterviewOrchestrator、
# InterviewSession、ports 和 models，因此链接 ai_interview_core
target_link_libraries(interview_orchestrator_test
    PRIVATE
        ai_interview_core
)

# 将 interview_orchestrator_test 注册到 CTest。
add_test(
    NAME interview_orchestrator_test
    COMMAND interview_orchestrator_test
)

# 构建 ReportBuilder 的单元测试可执行文件。
# 该测试主要验证能否从 InterviewSession 生成 InterviewReport，
# 包括总题数、总分、平均分、评价文本、summary_text 和 records。
add_executable(report_builder_test
    report_builder_test.cpp
)

target_link_libraries(report_builder_test
    PRIVATE
        ai_interview_core    
)

add_test(
    NAME report_builder_test
    COMMAND report_builder_test
)