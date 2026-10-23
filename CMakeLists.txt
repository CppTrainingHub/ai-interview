# 现在根目录只负责：
#   项目声明；c++标准；全局测试开关；串联src；串联tests

cmake_minimum_required(VERSION 3.20)

project(AIInterviewCore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 是否构建测试。默认开启，后续做安装包或只构建库时可以关闭
option(AI_INTERVIEW_BUILD_TESTS "Build AI interview unit tests" ON)
option(AI_INTERVIEW_BUILD_GUI "Build the cross-platform Qt GUI" OFF)

add_subdirectory(src)

if(AI_INTERVIEW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(AI_INTERVIEW_BUILD_GUI)
    set(CPACK_PACKAGE_NAME "AIInterview")
    set(CPACK_PACKAGE_VENDOR "AIInterview")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "AIInterview")

    if(WIN32)
        set(CPACK_GENERATOR "ZIP;NSIS")
    elseif(APPLE)
        set(CPACK_GENERATOR "DragNDrop")
    elseif(UNIX)
        set(CPACK_GENERATOR "TGZ;DEB")
        set(CPACK_DEBIAN_PACKAGE_MAINTAINER "AIInterview")
        set(CPACK_DEBIAN_PACKAGE_SECTION "education")
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    endif()

    include(CPack)
endif()
