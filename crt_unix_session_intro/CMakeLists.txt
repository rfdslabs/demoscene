cmake_minimum_required(VERSION 3.20)
project(crt_unix_session_intro LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)

add_executable(crt_unix_session_intro src/main.cpp)
target_link_libraries(crt_unix_session_intro PRIVATE SDL2::SDL2)
