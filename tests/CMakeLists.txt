cmake_minimum_required(VERSION 3.10.0)

project(BinanceTests)

find_package(GTest REQUIRED)

add_executable(${PROJECT_NAME}
    test.cpp
    config/ConfigLoaderTests.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    GTest::gtest_main
    GTest::gmock
    lib-config
)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})
