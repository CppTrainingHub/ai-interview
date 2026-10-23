add_library(ai_interview_config
    app_config.cpp
)

target_include_directories(ai_interview_config
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
) 

target_compile_features(ai_interview_config
    PUBLIC 
        cxx_std_17
)

if(MSVC)
    target_compile_options(ai_interview_config PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(ai_interview_config PRIVATE -Wall -Wextra -Wpedantic)
endif()