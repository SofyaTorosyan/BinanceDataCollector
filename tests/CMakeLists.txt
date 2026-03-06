cmake_minimum_required(VERSION 3.10.0)

project(BinanceTests)

find_package(GTest REQUIRED)

add_executable(${PROJECT_NAME}
    test.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    GTest::gtest_main
    GTest::gmock
)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})
