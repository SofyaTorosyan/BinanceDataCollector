cmake_minimum_required(VERSION 3.10.0)
project(lib-logging)

find_package(spdlog REQUIRED)

add_library(${PROJECT_NAME}
    LogLevel.h
    ILogger.h
    SpdlogLogger.h
    SpdlogLogger.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)
