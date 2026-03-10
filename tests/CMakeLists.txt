cmake_minimum_required(VERSION 3.10.0)

project(BinanceTests)

find_package(GTest REQUIRED)

add_executable(${PROJECT_NAME}
    config/ConfigLoaderTests.cpp
    logging/LoggerTests.cpp
    market/TradeAggregatorTests.cpp
    serialization/FileSerializerTests.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    GTest::gtest_main
    GTest::gmock
    lib-config
    lib-logging
    lib-market
    lib-serialization
)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})

# Integration tests — require real network access to Binance.
# Run separately: ./build/tests/Release/BinanceIntegrationTests.exe
add_executable(BinanceIntegrationTests
    network/BinanceWebSocketClientIntegrationTests.cpp
)

target_link_libraries(BinanceIntegrationTests PRIVATE
    GTest::gtest_main
    lib-network
    lib-config
)

gtest_discover_tests(BinanceIntegrationTests)
