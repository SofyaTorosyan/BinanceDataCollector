cmake_minimum_required(VERSION 3.10.0)
project(lib-config)

find_package(nlohmann_json REQUIRED)

add_library(${PROJECT_NAME} 
    AppConfig.h
    IConfigReader.h
    JsonConfigReader.cpp
    JsonConfigReader.h
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} PUBLIC nlohmann_json::nlohmann_json)
