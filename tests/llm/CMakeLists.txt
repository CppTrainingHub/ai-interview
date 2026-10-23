add_executable(llm_response_parser_test
    llm_response_parser_test.cpp
)

target_link_libraries(llm_response_parser_test
    PRIVATE
        ai_interview_llm
)

add_test(
    NAME llm_response_parser_test
    COMMAND llm_response_parser_test
)

add_executable(llm_question_generator_test
    llm_question_generator_test.cpp
)

target_link_libraries(llm_question_generator_test
    PRIVATE
        ai_interview_llm
)

add_test(
    NAME llm_question_generator_test
    COMMAND llm_question_generator_test
)

add_executable(llm_answer_evaluator_test
    llm_answer_evaluator_test.cpp
)

target_link_libraries(llm_answer_evaluator_test
    PRIVATE
        ai_interview_llm
)

add_test(
    NAME llm_answer_evaluator_test
    COMMAND llm_answer_evaluator_test
)

add_executable(llm_summary_generator_test
    llm_summary_generator_test.cpp
)

target_link_libraries(llm_summary_generator_test
    PRIVATE
        ai_interview_llm
)

add_test(
    NAME llm_summary_generator_test
    COMMAND llm_summary_generator_test
)
