option(AI_INTERVIEW_ENABLE_PORTAUDIO "Build PortAudio-backed audio device" OFF)
option(AI_INTERVIEW_ENABLE_VOLCENGINE_REALTIME "Build Volcengine realtime ASR/TTS client" OFF)

find_package(nlohmann_json CONFIG REQUIRED)

set(AI_INTERVIEW_AUDIO_SOURCES
    answer_audio_capture.cpp
    async_speech_controller.cpp
    audio_models.cpp
    null_audio_device.cpp
    realtime_speech_protocol.cpp
    scripted_asr_client.cpp
    speech_pipeline.cpp
    text_to_pcm_tts_client.cpp
    volcengine_realtime_speech_client.cpp
    wav_audio_device.cpp
    wav_file.cpp
)

add_library(ai_interview_audio
    ${AI_INTERVIEW_AUDIO_SOURCES}
)

target_include_directories(ai_interview_audio
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(ai_interview_audio
    PUBLIC
        ai_interview_core
        nlohmann_json::nlohmann_json
)

if(AI_INTERVIEW_ENABLE_PORTAUDIO)
    find_package(portaudio CONFIG REQUIRED)
    target_sources(ai_interview_audio PRIVATE portaudio_audio_device.cpp)
    target_link_libraries(ai_interview_audio PUBLIC portaudio_static)
    target_compile_definitions(ai_interview_audio PUBLIC AI_INTERVIEW_ENABLE_PORTAUDIO=1)
endif()

if(AI_INTERVIEW_ENABLE_VOLCENGINE_REALTIME)
    find_package(CURL REQUIRED)
    target_link_libraries(ai_interview_audio PUBLIC CURL::libcurl)
    target_compile_definitions(ai_interview_audio PUBLIC AI_INTERVIEW_ENABLE_VOLCENGINE_REALTIME=1)
endif()

target_compile_features(ai_interview_audio
    PUBLIC
        cxx_std_17
)

if(MSVC)
    target_compile_options(ai_interview_audio PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(ai_interview_audio PRIVATE -Wall -Wextra -Wpedantic)
endif()
