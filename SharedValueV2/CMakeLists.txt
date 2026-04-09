# SharedValueV2 CMake
cmake_minimum_required(VERSION 3.20)
project(SharedValueV2 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header only library
add_library(SharedValueV2_Core INTERFACE)
target_include_directories(SharedValueV2_Core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Generate XML Documentation for MSVC
if(MSVC)
    add_compile_options(/doc)
endif()

# Let's add tests
enable_testing()

add_executable(UnitTests tests/UnitTests.cpp)
target_link_libraries(UnitTests PRIVATE SharedValueV2_Core)
if(MSVC)
    set_target_properties(UnitTests PROPERTIES VS_GLOBAL_GenerateXMLDocumentationFiles "true")
endif()

add_executable(StressTest tests/StressTest.cpp)
target_link_libraries(StressTest PRIVATE SharedValueV2_Core)
if(MSVC)
    set_target_properties(StressTest PROPERTIES VS_GLOBAL_GenerateXMLDocumentationFiles "true")
endif()

add_test(NAME CoreUnitTests COMMAND UnitTests)
add_test(NAME ConcurrentStressTest COMMAND StressTest)
