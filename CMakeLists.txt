cmake_minimum_required(VERSION 3.20)

project(
    Excalibur
    VERSION 0.0.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(Math INTERFACE)
target_sources(Math INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Math.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Math/Math.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Math/Color.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Math/Curves.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Math/Definitions.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Math/Functions.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Math/Matrix.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Math/Quaternion.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Math/Random.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Math/Rendering.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Math/Scalar.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Math/Vector.hpp
)
target_include_directories(Math INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(Math INTERFACE cxx_std_20)
target_compile_options(Math INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:/permissive->
    $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

# ---- Vulkan (RHIVulkan.hpp 与 RHI 公共头文件依赖 vulkan/vulkan.h) ----
enable_testing()
add_executable(MathTests test_math.cpp)
target_link_libraries(MathTests PRIVATE Math)
add_test(NAME MathTests COMMAND MathTests)

find_package(Vulkan REQUIRED)

# ---- RHI ----
add_library(RHI STATIC
    RHI/RHIDefinitions.cpp
    RHI/RHIDefinitions.hpp
    RHI/Device/RHIDevice.cpp
    RHI/Device/RHIDevice.hpp
    RHI/Device/RHIDeviceDesc.hpp
    RHI/Device/RHIDeviceFactory.cpp
    RHI/Device/RHIDeviceFactory.hpp
    RHI/RenderGraph/RHIRenderGraph.cpp
    RHI/RenderGraph/RHIRenderGraph.hpp
    RHI/Vulkan/RHIVulkan.hpp
)
target_include_directories(RHI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(RHI PUBLIC Math Vulkan::Vulkan)
target_compile_features(RHI PUBLIC cxx_std_20)

# 让从仓库根目录打开的 CMake Tools 也能发现 PBRDemo。此前该 Demo 只能作为
# `cmake -S RHI/PBRDemo` 的独立工程配置，VS Code 的 configuration provider 因而
# 无法把生成的 PBRDemoConfig.hpp、stb 与后端依赖传给 main.cpp，造成大量假红线。
# 子目录保留自己的 PBRDemoRHI target，不改变上面的通用 RHI 静态库或后端实现。
option(EXCALIBUR_BUILD_PBR_DEMO "Expose PBRDemo to the root CMake project" ON)
if(WIN32 AND EXCALIBUR_BUILD_PBR_DEMO)
    add_subdirectory(RHI/PBRDemo)
endif()
