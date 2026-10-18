cmake_minimum_required(VERSION 3.20)
project(HimeraAutonomousAI VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(himera_ai
    src/goap.cpp
    src/hollow.cpp
    src/dstar.cpp
    src/director.cpp
)
target_include_directories(himera_ai PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(MSVC)
    target_compile_options(himera_ai PRIVATE /W4 /permissive-)
else()
    target_compile_options(himera_ai PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(himera_demo examples/demo.cpp)
target_link_libraries(himera_demo PRIVATE himera_ai)

add_executable(himera_benchmark examples/benchmark.cpp)
target_link_libraries(himera_benchmark PRIVATE himera_ai)

include(CTest)
if(BUILD_TESTING)
    add_executable(himera_tests tests/agents_tests.cpp)
    target_link_libraries(himera_tests PRIVATE himera_ai)
    add_test(NAME AgentsUnitTests COMMAND himera_tests)
endif()
