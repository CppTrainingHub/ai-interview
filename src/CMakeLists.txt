#  这里负责串联源码模块，每当新增一个模块，就追加
add_subdirectory(core)
add_subdirectory(report)
add_subdirectory(llm)
add_subdirectory(pdf)
add_subdirectory(config)
add_subdirectory(audio)
add_subdirectory(runtime)
add_subdirectory(net)
add_subdirectory(app)

if(AI_INTERVIEW_BUILD_GUI)
    add_subdirectory(gui)
endif()
