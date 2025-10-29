# 目标名称
set(tgt_name logger)

# 收集头文件和源文件
file(GLOB_RECURSE HEADERS include/${tgt_name}/*.h include/${tgt_name}/*.hpp)
file(GLOB_RECURSE SOURCES src/*.c src/*.cpp src/*.cc src/*.cxx)

add_library(${tgt_name} ${HEADERS} ${SOURCES})

# 添加头文件路径
target_include_directories(${tgt_name} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_link_libraries(${tgt_name} PUBLIC spdlog::spdlog)
# C++17 and above can use std::filesystem directly
if (CMAKE_CXX_STANDARD LESS 17)
    message(STATUS "Using ghc::filesystem (C++ standard < 17)")
    target_link_libraries(${tgt_name} PUBLIC ghc_filesystem)
else()
    message(STATUS "Using std::filesystem (C++17 or higher)")
endif()
