cmake_minimum_required(VERSION 3.20)

# -----------------------------------------------------------------------------
# PBR Demo: 一个 plane + 一个 UV sphere 的简化 PBR 渲染示例
#   - 窗口 surface 由 Win32/Vulkan 平台层创建，所有资源和绘制都经过 RHI + RenderGraph
#   - shader 在 build 时用 glslangValidator 编译为 SPIR-V
# -----------------------------------------------------------------------------
project(PBRDemo LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "PBRDemo currently requires Win32 HWND presentation.")
endif()

get_filename_component(PBR_REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

option(
    PBRDEMO_ENABLE_RENDERDOC_CAPTURE
    "Enable the optional --renderdoc-capture PBR debugging command"
    OFF
)

# -----------------------------------------------------------------------------
# 依赖
# -----------------------------------------------------------------------------
find_package(Vulkan REQUIRED)
# find_package 可能从 vcpkg 找 lib 但 SDK 头在 VULKAN_SDK；显式补 include 路径
if(DEFINED ENV{VULKAN_SDK} AND EXISTS "$ENV{VULKAN_SDK}/Include/vulkan/vulkan.h")
    message(STATUS "PBRDemo: using VULKAN_SDK headers at $ENV{VULKAN_SDK}/Include")
    set(VULKAN_INCLUDE_DIRS $ENV{VULKAN_SDK}/Include)
endif()
find_program(GLSLANG_VALIDATOR
    NAMES glslangValidator
    HINTS $ENV{VULKAN_SDK}/bin
    PATH_SUFFIXES bin
    DOC   "GLSL → SPIR-V compiler from Vulkan SDK"
)
if(NOT GLSLANG_VALIDATOR)
    message(FATAL_ERROR "glslangValidator not found. Install Vulkan SDK and ensure it's on PATH or set VULKAN_SDK env var.")
endif()

# -----------------------------------------------------------------------------
# 可执行文件
# -----------------------------------------------------------------------------
add_executable(PBRDemo WIN32
    main.cpp
    ${PBR_REPOSITORY_ROOT}/RHI/UI/RHIUI.cpp
)

# PBRDemo 可以单独用 `cmake -S RHI/PBRDemo ...` 构建。为了不改动根目录 RHI
# target，本示例在自己的 CMake 中组合需要的后端实现。
add_library(PBRDemoRHI STATIC
    ${PBR_REPOSITORY_ROOT}/RHI/RHIDefinitions.cpp
    ${PBR_REPOSITORY_ROOT}/RHI/Device/RHIDevice.cpp
    ${PBR_REPOSITORY_ROOT}/RHI/Device/RHIDeviceFactory.cpp
    ${PBR_REPOSITORY_ROOT}/RHI/RenderGraph/RHIRenderGraph.cpp
    ${PBR_REPOSITORY_ROOT}/RHI/Vulkan/RHIVulkan.cpp
    ${PBR_REPOSITORY_ROOT}/RHI/D3D11/RHID3D11.cpp
    ${PBR_REPOSITORY_ROOT}/RHI/D3D12/RHID3D12.cpp
)
target_include_directories(PBRDemoRHI PUBLIC ${PBR_REPOSITORY_ROOT})
target_link_libraries(PBRDemoRHI
    PUBLIC Vulkan::Vulkan
    PRIVATE d3d11 d3d12 dxgi dxguid d3dcompiler
)
target_compile_features(PBRDemoRHI PUBLIC cxx_std_20)

# shader 输出目录：<build>/PBRDemo/shaders/
set(PBR_SHADER_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${PBR_SHADER_OUT_DIR})

# 生成源码可见的配置头。相比只用 target_compile_definitions 注入宏，这种方式让
# 编译器和 IntelliSense 看到同一个 constexpr 定义，并支持“转到定义”。
set(PBR_GENERATED_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PBR_GENERATED_INCLUDE_DIR})
get_filename_component(
    PBR_ASSET_DIRECTORY
    "${CMAKE_CURRENT_SOURCE_DIR}"
    ABSOLUTE
)
file(TO_CMAKE_PATH "${PBR_ASSET_DIRECTORY}" PBR_ASSET_DIRECTORY)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/PBRDemoConfig.hpp.in
    ${PBR_GENERATED_INCLUDE_DIR}/PBRDemoConfig.hpp
    @ONLY
)
set_source_files_properties(
    ${PBR_GENERATED_INCLUDE_DIR}/PBRDemoConfig.hpp
    PROPERTIES
        GENERATED TRUE
        HEADER_FILE_ONLY TRUE
)
target_sources(PBRDemo PRIVATE
    ${PBR_GENERATED_INCLUDE_DIR}/PBRDemoConfig.hpp
)
target_include_directories(PBRDemo PRIVATE
    ${PBR_GENERATED_INCLUDE_DIR}
)

# shader 编译：.vert / .frag → .spv
function(rhi_compile_shader SRC_FILE)
    get_filename_component(SRC_NAME ${SRC_FILE} NAME)
    set(OUT_FILE ${PBR_SHADER_OUT_DIR}/${SRC_NAME}.spv)
    add_custom_command(
        OUTPUT  ${OUT_FILE}
        COMMAND ${GLSLANG_VALIDATOR} -V ${SRC_FILE} -o ${OUT_FILE}
        DEPENDS ${SRC_FILE}
        COMMENT "Compiling ${SRC_NAME} → SPIR-V"
        VERBATIM)
endfunction()

rhi_compile_shader(${CMAKE_CURRENT_SOURCE_DIR}/shaders/pbr.vert)
rhi_compile_shader(${CMAKE_CURRENT_SOURCE_DIR}/shaders/pbr.frag)
rhi_compile_shader(${CMAKE_CURRENT_SOURCE_DIR}/shaders/skybox.vert)
rhi_compile_shader(${CMAKE_CURRENT_SOURCE_DIR}/shaders/skybox.frag)
# Shadow Pass 是 depth-only，所以只需要编译 shadow.vert，不存在 shadow.frag。
# 顶点输出的 gl_Position 会由固定功能光栅器转换成 Shadow Map 深度。
rhi_compile_shader(${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow.vert)
rhi_compile_shader(${PBR_REPOSITORY_ROOT}/RHI/UI/shaders/ui.vert)
rhi_compile_shader(${PBR_REPOSITORY_ROOT}/RHI/UI/shaders/ui.frag)

# D3D11/D3D12 在运行时通过 D3DCompileFromFile 编译同一份 HLSL。复制到统一输出目录，
# 让三个后端都只依赖 PBRDemoConfig.hpp 提供的 SHADER_DIRECTORY。
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/pbr.hlsl
    ${PBR_SHADER_OUT_DIR}/pbr.hlsl
    COPYONLY
)
configure_file(
    ${PBR_REPOSITORY_ROOT}/RHI/UI/shaders/ui.hlsl
    ${PBR_SHADER_OUT_DIR}/ui.hlsl
    COPYONLY
)

# 把 .spv 列成 PBRDemo 的源文件 → CMake 自动建立编译顺序
target_sources(PBRDemo PRIVATE
    ${PBR_SHADER_OUT_DIR}/pbr.vert.spv
    ${PBR_SHADER_OUT_DIR}/pbr.frag.spv
    ${PBR_SHADER_OUT_DIR}/skybox.vert.spv
    ${PBR_SHADER_OUT_DIR}/skybox.frag.spv
    ${PBR_SHADER_OUT_DIR}/shadow.vert.spv
    ${PBR_SHADER_OUT_DIR}/ui.vert.spv
    ${PBR_SHADER_OUT_DIR}/ui.frag.spv
)

# -----------------------------------------------------------------------------
# 链接
# -----------------------------------------------------------------------------
target_link_libraries(PBRDemo
    PRIVATE
        PBRDemoRHI
        user32
)

target_include_directories(PBRDemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
)

if(PBRDEMO_ENABLE_RENDERDOC_CAPTURE)
    find_path(RENDERDOC_INCLUDE_DIR
        NAMES renderdoc_app.h
        HINTS "C:/Program Files/RenderDoc"
        DOC "RenderDoc in-application capture API header"
    )
    if(NOT RENDERDOC_INCLUDE_DIR)
        message(FATAL_ERROR
            "PBRDEMO_ENABLE_RENDERDOC_CAPTURE requires renderdoc_app.h. "
            "Install RenderDoc or set RENDERDOC_INCLUDE_DIR.")
    endif()
    target_include_directories(PBRDemo PRIVATE ${RENDERDOC_INCLUDE_DIR})
    target_compile_definitions(PBRDemo PRIVATE PBRDEMO_ENABLE_RENDERDOC_CAPTURE=1)
endif()

if(VULKAN_INCLUDE_DIRS)
    target_include_directories(PBRDemo PRIVATE ${VULKAN_INCLUDE_DIRS})
endif()

if(MSVC)
    target_compile_options(PBRDemoRHI PRIVATE /W4 /permissive- /Zc:__cplusplus /utf-8)
    target_compile_options(PBRDemo PRIVATE
        /W4 /permissive- /Zc:__cplusplus /utf-8
    )
    target_compile_definitions(PBRDemo PRIVATE
        _CRT_SECURE_NO_WARNINGS NOMINMAX
    )
else()
    target_compile_options(PBRDemoRHI PRIVATE -Wall -Wextra)
    target_compile_options(PBRDemo PRIVATE
        -Wall -Wextra
    )
endif()

set_target_properties(PBRDemo PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
