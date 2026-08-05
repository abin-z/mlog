# 目标名称
set(tgt_name logger)

add_library(${tgt_name})

target_sources(${tgt_name}
    PRIVATE
        src/log_manager.cc
)

# 添加头文件路径
target_include_directories(${tgt_name}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(${tgt_name} PUBLIC cxx_std_11)
target_link_libraries(${tgt_name} PUBLIC spdlog::spdlog)
target_link_libraries(${tgt_name} PUBLIC date::date)
target_link_libraries(${tgt_name} PUBLIC date::date-tz)
# C++17 and above can use std::filesystem directly
if (CMAKE_CXX_STANDARD LESS 17)
    message(STATUS "Using ghc::filesystem (C++ standard < 17)")
    target_link_libraries(${tgt_name} PUBLIC ghc_filesystem)
else()
    message(STATUS "Using std::filesystem (C++17 or higher)")
endif()
