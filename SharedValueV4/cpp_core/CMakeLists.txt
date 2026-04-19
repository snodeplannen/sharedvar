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

# Generate FlatBuffers header
set(FLATC_EXE "${CMAKE_CURRENT_SOURCE_DIR}/../flatc_tools/flatc.exe")
set(SCHEMA_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../schema/dataset.fbs")

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/dataset_generated.h
    COMMAND ${FLATC_EXE} --cpp -o ${CMAKE_CURRENT_SOURCE_DIR} ${SCHEMA_FILE}
    DEPENDS ${SCHEMA_FILE}
    COMMENT "Regenerating FlatBuffers C++ header..."
)

add_custom_target(GenerateSchema DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/dataset_generated.h)

add_executable(MemMapProducer main.cpp)
add_dependencies(MemMapProducer GenerateSchema)

# The flatbuffers generated header is in the current directory
target_include_directories(MemMapProducer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(MemMapProducer PRIVATE flatbuffers)
