cmake_minimum_required(VERSION 3.20)
project(SharedValueV5_CppCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(cpp_producer
    main.cpp
)

target_include_directories(cpp_producer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Link Windows kernel libs
target_link_libraries(cpp_producer PRIVATE kernel32)
