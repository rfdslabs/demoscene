cmake_minimum_required(VERSION 3.20)
project(razor_rfds_intro LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(razor_rfds_intro src/main.cpp)
target_link_libraries(razor_rfds_intro PRIVATE SDL2::SDL2 OpenGL::GL)

if(APPLE)
  target_compile_definitions(razor_rfds_intro PRIVATE GL_SILENCE_DEPRECATION)
endif()
