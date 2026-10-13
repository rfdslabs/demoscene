cmake_minimum_required(VERSION 3.20)
project(realism_island_intro LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(realism_island_intro src/main.cpp)
target_link_libraries(realism_island_intro PRIVATE SDL2::SDL2 OpenGL::GL)

if(APPLE)
  target_compile_definitions(realism_island_intro PRIVATE GL_SILENCE_DEPRECATION)
endif()
