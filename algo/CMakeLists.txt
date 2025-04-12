cmake_minimum_required(VERSION 3.10)
project(wsc25)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add optimization flags for different environments
if(APPLE)
    # Development on Mac
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -pthread")
elseif(WIN32)
    # For Windows target
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=tigerlake -mtune=tigerlake -flto -pthread")
endif()

add_executable(wsc25 main.cpp)