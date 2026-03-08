cmake_minimum_required(VERSION 3.10.0)
project(lib-app)

set(APP_NAME BinanceDataCollector)

add_executable(${APP_NAME} 
    App.cpp
    App.h
    main.cpp
)

target_link_libraries(${APP_NAME} PUBLIC lib-config lib-logging lib-network lib-serialization lib-market)
