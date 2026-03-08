cmake_minimum_required(VERSION 3.10.0)

add_subdirectory(config)
add_subdirectory(market)
add_subdirectory(network)
add_subdirectory(serialization)

add_executable(BinanceDataCollector main.cpp)


