cmake_minimum_required(VERSION 3.10.0)
project(BinanceDataCollector VERSION 0.1.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/Coverage.cmake)

add_subdirectory(src)
add_subdirectory(tests)

if(ENABLE_COVERAGE)
    add_coverage_target()
endif()

