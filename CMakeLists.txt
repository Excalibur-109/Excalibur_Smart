cmake_minimum_required(VERSION 3.20)

project(
    Excalibur
    VERSION 0.0.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(Math INTERFACE)
target_sources(Math INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Math.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Math/Scalar.hpp
)
target_include_directories(Math INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(Math INTERFACE cxx_std_20)