add_executable(ai_interview_pdf_resume_parser_test
    pdf_resume_parser_test.cpp
)

target_link_libraries(ai_interview_pdf_resume_parser_test
    PRIVATE
        ai_interview_pdf
)

target_compile_features(ai_interview_pdf_resume_parser_test
    PRIVATE
        cxx_std_17
)

add_test(
    NAME ai_interview_pdf_resume_parser_test
    COMMAND ai_interview_pdf_resume_parser_test
)