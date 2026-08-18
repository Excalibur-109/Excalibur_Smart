cmake_minimum_required(VERSION 3.20)

# 独立的通用前向渲染管线 Demo。
# 场景启动代码复用 PBRDemo 的 Win32/PBR 资源加载逻辑，管线阶段则由
# ForwardRenderPipeline 统一编排；因此可执行文件的视觉效果与 PBRDemo 相同，
# 但 Shadow/Opaque/PostProcess/Overlay/Present 的组织可直接复用到其他场景。
project(RenderPipelineDemo LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "RenderPipelineDemo currently requires Win32 HWND presentation.")
endif()

get_filename_component(RENDER_PIPELINE_REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(PBR_DEMO_SOURCE_DIR "${RENDER_PIPELINE_REPOSITORY_ROOT}/RHI/PBRDemo")

option(
    RENDER_PIPELINE_DEMO_ENABLE_RENDERDOC_CAPTURE
    "Enable the optional --renderdoc-capture debugging command"
    OFF
)

find_package(Vulkan REQUIRED)
if(DEFINED ENV{VULKAN_SDK} AND EXISTS "$ENV{VULKAN_SDK}/Include/vulkan/vulkan.h")
    message(STATUS "RenderPipelineDemo: using VULKAN_SDK headers at $ENV{VULKAN_SDK}/Include")
    set(VULKAN_INCLUDE_DIRS $ENV{VULKAN_SDK}/Include)
endif()
find_program(GLSLANG_VALIDATOR
    NAMES glslangValidator
    HINTS $ENV{VULKAN_SDK}/bin
    PATH_SUFFIXES bin
    DOC "GLSL to SPIR-V compiler from Vulkan SDK"
)
if(NOT GLSLANG_VALIDATOR)
    message(FATAL_ERROR "glslangValidator not found. Install Vulkan SDK or set VULKAN_SDK.")
endif()

# PBRDemo 的 main.cpp 已被参数化为两个独立程序共享：这个 target 通过宏选择
# RenderPipelineDemo 的窗口/应用名，而实际帧构建使用新增的 ForwardRenderPipeline。
add_executable(RenderPipelineDemo WIN32
    ${PBR_DEMO_SOURCE_DIR}/main.cpp
    ${RENDER_PIPELINE_REPOSITORY_ROOT}/RHI/UI/RHIUI.cpp
)
target_compile_definitions(RenderPipelineDemo PRIVATE RHI_RENDER_PIPELINE_DEMO=1)

# 保持后端源码作为静态库链接，Demo 不修改 RHI 的任何既有 target 或实现文件。
add_library(RenderPipelineDemoRHI STATIC
    ${RENDER_PIPELINE_REPOSITORY_ROOT}/RHI/RHIDefinitions.cpp
    ${RENDER_PIPELINE_REPOSITORY_ROOT}/RHI/Device/RHIDevice.cpp
    ${RENDER_PIPELINE_REPOSITORY_ROOT}/RHI/Device/RHIDeviceFactory.cpp
    ${RENDER_PIPELINE_REPOSITORY_ROOT}/RHI/RenderGraph/RHIRenderGraph.cpp
    ${RENDER_PIPELINE_REPOSITORY_ROOT}/RHI/Vulkan/RHIVulkan.cpp
    ${RENDER_PIPELINE_REPOSITORY_ROOT}/RHI/D3D11/RHID3D11.cpp
    ${RENDER_PIPELINE_REPOSITORY_ROOT}/RHI/D3D12/RHID3D12.cpp
)
target_include_directories(RenderPipelineDemoRHI PUBLIC ${RENDER_PIPELINE_REPOSITORY_ROOT})
target_link_libraries(RenderPipelineDemoRHI
    PUBLIC Vulkan::Vulkan
    PRIVATE d3d11 d3d12 dxgi dxguid d3dcompiler
)
target_compile_features(RenderPipelineDemoRHI PUBLIC cxx_std_20)

# 继续使用 PBRDemoConfig.hpp 的路径契约，使同一份源文件在两个 build 目录下都能
# 找到各自的 shaders 与共享 PBR 资源目录，而无需在代码中写绝对路径。
set(PBR_SHADER_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${PBR_SHADER_OUT_DIR})
set(PBR_GENERATED_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PBR_GENERATED_INCLUDE_DIR})
get_filename_component(PBR_ASSET_DIRECTORY "${PBR_DEMO_SOURCE_DIR}" ABSOLUTE)
file(TO_CMAKE_PATH "${PBR_ASSET_DIRECTORY}" PBR_ASSET_DIRECTORY)
configure_file(
    ${PBR_DEMO_SOURCE_DIR}/PBRDemoConfig.hpp.in
    ${PBR_GENERATED_INCLUDE_DIR}/PBRDemoConfig.hpp
    @ONLY
)
target_sources(RenderPipelineDemo PRIVATE ${PBR_GENERATED_INCLUDE_DIR}/PBRDemoConfig.hpp)
target_include_directories(RenderPipelineDemo PRIVATE
    ${PBR_GENERATED_INCLUDE_DIR}
    ${PBR_DEMO_SOURCE_DIR}/third_party/stb
)

function(render_pipeline_compile_shader SRC_FILE)
    get_filename_component(SRC_NAME ${SRC_FILE} NAME)
    set(OUT_FILE ${PBR_SHADER_OUT_DIR}/${SRC_NAME}.spv)
    add_custom_command(
        OUTPUT ${OUT_FILE}
        COMMAND ${GLSLANG_VALIDATOR} -V ${SRC_FILE} -o ${OUT_FILE}
        DEPENDS ${SRC_FILE}
        COMMENT "Compiling ${SRC_NAME} to SPIR-V"
        VERBATIM
    )
endfunction()

render_pipeline_compile_shader(${PBR_DEMO_SOURCE_DIR}/shaders/pbr.vert)
render_pipeline_compile_shader(${PBR_DEMO_SOURCE_DIR}/shaders/pbr.frag)
render_pipeline_compile_shader(${PBR_DEMO_SOURCE_DIR}/shaders/skybox.vert)
render_pipeline_compile_shader(${PBR_DEMO_SOURCE_DIR}/shaders/skybox.frag)
render_pipeline_compile_shader(${PBR_DEMO_SOURCE_DIR}/shaders/shadow.vert)
render_pipeline_compile_shader(${RENDER_PIPELINE_REPOSITORY_ROOT}/RHI/UI/shaders/ui.vert)
render_pipeline_compile_shader(${RENDER_PIPELINE_REPOSITORY_ROOT}/RHI/UI/shaders/ui.frag)

configure_file(${PBR_DEMO_SOURCE_DIR}/shaders/pbr.hlsl ${PBR_SHADER_OUT_DIR}/pbr.hlsl COPYONLY)
configure_file(${RENDER_PIPELINE_REPOSITORY_ROOT}/RHI/UI/shaders/ui.hlsl ${PBR_SHADER_OUT_DIR}/ui.hlsl COPYONLY)
target_sources(RenderPipelineDemo PRIVATE
    ${PBR_SHADER_OUT_DIR}/pbr.vert.spv
    ${PBR_SHADER_OUT_DIR}/pbr.frag.spv
    ${PBR_SHADER_OUT_DIR}/skybox.vert.spv
    ${PBR_SHADER_OUT_DIR}/skybox.frag.spv
    ${PBR_SHADER_OUT_DIR}/shadow.vert.spv
    ${PBR_SHADER_OUT_DIR}/ui.vert.spv
    ${PBR_SHADER_OUT_DIR}/ui.frag.spv
)

target_link_libraries(RenderPipelineDemo PRIVATE RenderPipelineDemoRHI user32)

if(RENDER_PIPELINE_DEMO_ENABLE_RENDERDOC_CAPTURE)
    find_path(RENDERDOC_INCLUDE_DIR
        NAMES renderdoc_app.h
        HINTS "C:/Program Files/RenderDoc"
        DOC "RenderDoc in-application capture API header"
    )
    if(NOT RENDERDOC_INCLUDE_DIR)
        message(FATAL_ERROR "RenderDoc header not found. Set RENDERDOC_INCLUDE_DIR.")
    endif()
    target_include_directories(RenderPipelineDemo PRIVATE ${RENDERDOC_INCLUDE_DIR})
    # main.cpp 共用已有宏，避免复制 RenderDoc 代码路径。
    target_compile_definitions(RenderPipelineDemo PRIVATE PBRDEMO_ENABLE_RENDERDOC_CAPTURE=1)
endif()

if(VULKAN_INCLUDE_DIRS)
    target_include_directories(RenderPipelineDemo PRIVATE ${VULKAN_INCLUDE_DIRS})
endif()

if(MSVC)
    target_compile_options(RenderPipelineDemoRHI PRIVATE /W4 /permissive- /Zc:__cplusplus /utf-8)
    target_compile_options(RenderPipelineDemo PRIVATE /W4 /permissive- /Zc:__cplusplus /utf-8)
    target_compile_definitions(RenderPipelineDemo PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    target_compile_options(RenderPipelineDemoRHI PRIVATE -Wall -Wextra)
    target_compile_options(RenderPipelineDemo PRIVATE -Wall -Wextra)
endif()

set_target_properties(RenderPipelineDemo PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
