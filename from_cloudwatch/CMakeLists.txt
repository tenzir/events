cmake_minimum_required(VERSION 3.15)
project(from_cloudwatch CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find AWS SDK
find_package(AWSSDK REQUIRED COMPONENTS logs)

# Find CURL with HTTP/2 support for Live Tail streaming
find_package(CURL REQUIRED)

# Main executable (uses polling fallback)
add_executable(from_cloudwatch
    src/from_cloudwatch.cpp
)

target_include_directories(from_cloudwatch PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${AWSSDK_INCLUDE_DIRS}
)

target_link_libraries(from_cloudwatch PRIVATE
    ${AWSSDK_LINK_LIBRARIES}
)

# Live Tail streaming executable (uses HTTP/2 with libcurl)
add_executable(from_cloudwatch_livetail
    src/from_cloudwatch.cpp
    src/live_tail_stream.cpp
)

target_include_directories(from_cloudwatch_livetail PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${AWSSDK_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
)

target_link_libraries(from_cloudwatch_livetail PRIVATE
    ${AWSSDK_LINK_LIBRARIES}
    CURL::libcurl
)

target_compile_definitions(from_cloudwatch_livetail PRIVATE
    USE_LIVE_TAIL_STREAMING
)

# Install targets
install(TARGETS from_cloudwatch from_cloudwatch_livetail
    RUNTIME DESTINATION bin
)
