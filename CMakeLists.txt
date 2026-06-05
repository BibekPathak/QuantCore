cmake_minimum_required(VERSION 3.16)
project(hft VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "-O3 -march=native -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native")

include_directories(include)

find_package(Threads REQUIRED)

set(SOURCES
    src/main.cpp
    src/order_book.cpp
    src/matching_engine.cpp
    src/market_data_feed.cpp
    src/strategy.cpp
    src/backtester.cpp
    src/metrics.cpp
)

add_executable(hft ${SOURCES})

target_link_libraries(hft PRIVATE Threads::Threads)

enable_testing()
add_subdirectory(tests)
