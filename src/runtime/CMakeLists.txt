add_library(ai_interview_runtime
    interview_runtime.cpp
)

target_include_directories(ai_interview_runtime
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(ai_interview_runtime
    PUBLIC
        ai_interview_audio
        ai_interview_config
        ai_interview_core
        ai_interview_llm
        ai_interview_pdf
        ai_interview_report
)

target_compile_features(ai_interview_runtime
    PUBLIC
        cxx_std_17
)

if(MSVC)
    target_compile_options(ai_interview_runtime PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(ai_interview_runtime PRIVATE -Wall -Wextra -Wpedantic)
endif()
