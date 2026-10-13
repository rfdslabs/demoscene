cmake_minimum_required(VERSION 3.20)
project(mac_demo_intro LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(mac_demo src/main.cpp)
target_link_libraries(mac_demo PRIVATE SDL2::SDL2 OpenGL::GL)

if(APPLE)
  target_compile_definitions(mac_demo PRIVATE GL_SILENCE_DEPRECATION)
endif()
