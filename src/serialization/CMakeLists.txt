cmake_minimum_required(VERSION 3.10.0)
project(lib-serialization)

add_library(${PROJECT_NAME}
    ISerializer.h
    FileSerializer.cpp
    FileSerializer.h
    WindowStats.h
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} PUBLIC lib-config)
