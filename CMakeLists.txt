cmake_minimum_required(VERSION 3.29)
project(MpscChannel)

set(CMAKE_CXX_STANDARD 20)

add_library(MpscChannel STATIC
        src/MPSCQueue.cpp
        include/MPSCQueue.h
        src/Handles.cpp
        include/Handles.h)

target_include_directories(MpscChannel PUBLIC include)

add_executable(MspcChannel_test library.cpp
        src/MPSCQueue.cpp
        include/MPSCQueue.h
        src/Handles.cpp
        include/Handles.h)

target_include_directories(MspcChannel_test PUBLIC include)
