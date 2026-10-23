find_package(CURL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

file(GLOB AI_INTERVIEW_LLM_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(ai_interview_llm
    ${AI_INTERVIEW_LLM_SOURCES}
)

target_include_directories(ai_interview_llm
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(ai_interview_llm
    PUBLIC
        ai_interview_core
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

target_compile_features(ai_interview_llm
    PUBLIC
        cxx_std_17
)

if(MSVC)
    target_compile_options(ai_interview_llm PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(ai_interview_llm PRIVATE -Wall -Wextra -Wpedantic)
endif()
