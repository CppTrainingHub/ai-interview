add_executable(interview_server_test
    interview_server_test.cpp
)

target_link_libraries(interview_server_test
    PRIVATE
        ai_interview_net
)

add_test(
    NAME interview_server_test
    COMMAND interview_server_test
)
