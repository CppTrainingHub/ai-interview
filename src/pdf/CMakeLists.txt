find_package(podofo CONFIG QUIET)

file(GLOB AI_INTERVIEW_PDF_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(ai_interview_pdf
    ${AI_INTERVIEW_PDF_SOURCES}
)

target_include_directories(ai_interview_pdf
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(ai_interview_pdf
    PUBLIC
        ai_interview_core
)

if(podofo_FOUND)
    target_link_libraries(ai_interview_pdf
        PRIVATE
            podofo::podofo
    )
    target_compile_definitions(ai_interview_pdf
        PRIVATE
            AI_INTERVIEW_ENABLE_PODOFO=1
    )
endif()

target_compile_features(ai_interview_pdf
    PUBLIC
        cxx_std_17
)

if(MSVC)
    target_compile_options(ai_interview_pdf PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(ai_interview_pdf PRIVATE -Wall -Wextra -Wpedantic)
endif()
