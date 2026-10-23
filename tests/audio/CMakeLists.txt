add_executable(audio_module_test
    audio_module_test.cpp
)

target_link_libraries(audio_module_test
    PRIVATE
        ai_interview_audio
)

add_test(
    NAME audio_module_test
    COMMAND audio_module_test
)

add_executable(audio_file_pipeline_demo
    audio_file_pipeline_demo.cpp
)

target_link_libraries(audio_file_pipeline_demo
    PRIVATE
        ai_interview_audio
)

if(AI_INTERVIEW_ENABLE_PORTAUDIO)
    add_executable(audio_microphone_smoke_test
        audio_microphone_smoke_test.cpp
    )

    target_link_libraries(audio_microphone_smoke_test
        PRIVATE
            ai_interview_audio
    )
endif()
