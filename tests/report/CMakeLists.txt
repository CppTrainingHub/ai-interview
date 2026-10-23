add_executable(json_report_writer_test
    json_report_writer_test.cpp
)

target_link_libraries(json_report_writer_test
    PRIVATE
        ai_interview_report
)

add_test(
    NAME json_report_writer_test
    COMMAND json_report_writer_test
)