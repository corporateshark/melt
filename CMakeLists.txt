cmake_minimum_required(VERSION 2.8.12)

project(melt)

if(WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 /O2")
else()
  #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O3 -Wall -pedantic")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g -O0 -Wall -pedantic")
endif()

include_directories(${PROJECT_SOURCE_DIR})

add_executable(compile-check melt.c)

add_subdirectory(${PROJECT_SOURCE_DIR}/tests)
