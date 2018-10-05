cmake_minimum_required(VERSION 3.1)

project(cow-optimization CXX)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(benchmark REQUIRED)

find_package(CAF COMPONENTS core io REQUIRED)

include_directories(${CAF_INCLUDE_DIRS})

add_executable(main src/main.cpp)

target_link_libraries(main ${CAF_LIBRARIES} benchmark::benchmark_main)

