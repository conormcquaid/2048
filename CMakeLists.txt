cmake_minimum_required (VERSION 3.5)

project(2048 VERSION 1.0.1 DESCRIPTION "popular app reimagined as a console game")

set(CMAKE_BUILD_TYPE debug)

#set(CMAKE_C_STANDARD 11)
#set(CMAKE_CXX_COMPILER gcc)
#SET(GCC_COMPILE_FLAGS "-Wall -Wextra")
#SET(GCC_PROF_FLAGS " ")
#add_definitions(${GCC_COMPILE_FLAGS})
#add_definitions(${GCC_PROF_FLAGS})
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_COMPILE_FLAGS} ${GCC_PROF_FLAGS}")

add_executable(2048 thing.c)

