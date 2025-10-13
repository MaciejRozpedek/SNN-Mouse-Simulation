cmake_minimum_required(VERSION 3.14)

project(SNN_Mouse_Simulation CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(FetchContent)

FetchContent_Declare(
    yamlcpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG master
)
FetchContent_MakeAvailable(yamlcpp)


file(GLOB_RECURSE SOURCE_FILES
    "src/*.cpp"
    "src/*/*.cpp"
)

add_executable(snn_simulator ${SOURCE_FILES})

target_include_directories(snn_simulator PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/exceptions
    ${CMAKE_CURRENT_SOURCE_DIR}/src/simulation
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utility
)

target_link_libraries(snn_simulator PRIVATE yaml-cpp)

if (MSVC)
    # Windows + MSVC
    target_compile_options(snn_simulator PRIVATE 
        $<$<CONFIG:Debug>:/Zi /Od>     # Debug: symboli i brak optymalizacji
        $<$<CONFIG:Release>:/O2>       # Release: optymalizacja
    )
    set_target_properties(snn_simulator PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    )
else()
    # GCC / Clang
    target_compile_options(snn_simulator PRIVATE
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3>
    )
    set_target_properties(snn_simulator PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    )
endif()