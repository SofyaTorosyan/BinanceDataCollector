cmake_minimum_required(VERSION 3.10.0)
project(lib-network)

find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)

add_library(${PROJECT_NAME}
    BinanceWebSocketClient.cpp
    BinanceWebSocketClient.h
    IWebSocketClient.h
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} PUBLIC boost::boost openssl::openssl lib-logging lib-config)
