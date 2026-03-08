cmake_minimum_required(VERSION 3.10.0)
project(lib-market)

find_package(nlohmann_json REQUIRED)

add_library(${PROJECT_NAME}
    IAggregator.h
    IMonitoringService.h
    MonitoringService.cpp
    MonitoringService.h
    TradeAggregator.cpp
    TradeAggregator.h
    TradeEvent.h
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} PUBLIC lib-serialization lib-network lib-logging nlohmann_json::nlohmann_json)
