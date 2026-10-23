file(GLOB AI_INTERVIEW_NET_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(ai_interview_net
    ${AI_INTERVIEW_NET_SOURCES}
)

target_include_directories(ai_interview_net PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(ai_interview_net
    PUBLIC 
        cxx_std_17
)

find_package(nlohmann_json CONFIG REQUIRED)

target_link_libraries(ai_interview_net
    PUBLIC 
        ai_interview_core
        ai_interview_config
        ai_interview_llm
        ai_interview_pdf
        ai_interview_report
        ai_interview_runtime
        nlohmann_json::nlohmann_json
)

if(MSVC)
    target_compile_options(ai_interview_net PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(ai_interview_net PRIVATE -Wall -Wextra -Wpedantic)
endif()
