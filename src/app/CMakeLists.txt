add_executable(ai_interview_cli
    main.cpp
    cli_app.cpp
)

target_include_directories(ai_interview_cli
    PRIVATE 
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(ai_interview_cli
    PRIVATE
        ai_interview_config
        ai_interview_core
        ai_interview_pdf
        ai_interview_llm
        ai_interview_report
        ai_interview_runtime
)

target_compile_features(ai_interview_cli
    PRIVATE
        cxx_std_17
)

if(MSVC)
    target_compile_options(ai_interview_cli PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(ai_interview_cli PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(ai_interview_http_server
    http_server_main.cpp
)

target_include_directories(ai_interview_http_server
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(ai_interview_http_server
    PRIVATE
        ai_interview_config
        ai_interview_net
)

target_compile_features(ai_interview_http_server
    PRIVATE
        cxx_std_17
)

if(WIN32)
    target_link_libraries(ai_interview_http_server PRIVATE ws2_32)
endif()

if(MSVC)
    target_compile_options(ai_interview_http_server PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(ai_interview_http_server PRIVATE -Wall -Wextra -Wpedantic)
endif()
