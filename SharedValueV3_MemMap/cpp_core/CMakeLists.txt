cmake_minimum_required(VERSION 3.20)
project(MemMapProducer CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)

include(FetchContent)

# Fetch FlatBuffers to ensure we have the C++ header library
FetchContent_Declare(
  flatbuffers
  GIT_REPOSITORY https://github.com/google/flatbuffers.git
  GIT_TAG        v24.3.25
)
# Disable building flatc from source, we just need the headers
set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FLATBUFFERS_BUILD_FLATC OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(flatbuffers)

add_executable(MemMapProducer main.cpp)

# The flatbuffers generated header is in the current directory
target_include_directories(MemMapProducer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(MemMapProducer PRIVATE flatbuffers)
