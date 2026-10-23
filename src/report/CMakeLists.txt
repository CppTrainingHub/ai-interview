# 与 core/CMakeLists.txt 相同写法
file(GLOB AI_INTERVIEW_REPORT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"    
)

add_library(ai_interview_report
    ${AI_INTERVIEW_REPORT_SOURCES}
)

target_include_directories(ai_interview_report
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(ai_interview_report
    PUBLIC
        ai_interview_core
)   

target_compile_features(ai_interview_report
    PUBLIC
        cxx_std_17
)

if(MSVC)
    target_compile_options(ai_interview_report PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(ai_interview_report PRIVATE -Wall -Wextra -Wpedantic)
endif()