# RenderPipelineDemo

`RenderPipelineDemo` 是一个基于公开 RHI 和 RenderGraph 接口的通用前向渲染管线示例。
它复用 `PBRDemo` 的 Win32 窗口、metal_18 材质、天空盒、PBR shader 与 UI，所以输出
效果和 PBRDemo 一致；区别在于帧的阶段顺序不再由场景代码直接写入 `RHIFramePacket`，而是
由 `ForwardRenderPipeline` 统一编排。

```text
Scene resource import + uploads
             |
             v
Shadow -> Opaque -> PostProcess (optional) -> Overlay (optional) -> Present
             |
             v
RHIFramePacket -> CompileRHIRenderGraph -> Vulkan / D3D11 / D3D12
```

## 责任边界

- `ForwardRenderPipeline.hpp`：纯 CPU 阶段编排器，不拥有 GPU 资源，也不包含 Vulkan 或 D3D 类型。
- PBR 场景代码：创建 mesh、texture、pipeline、bind set；声明每个 pass 的 reads、attachments 和 draw。
- `RHIRenderGraph`：从资源读写生成依赖、资源状态转换和 transient allocation 计划。
- RHI 后端：把计划变为 Vulkan/D3D11/D3D12 命令并提交。

`Opaque` 与 `Present` 是必需阶段；`Shadow`、`PostProcess`、`Overlay` 可按场景需要省略。
每一阶段至多一个 pass 和一个 workload，且 workload 的 `passName` 必须与对应 pass 一致。

## 构建与运行

```powershell
cmake -S RHI/RenderPipelineDemo -B build/RenderPipelineDemo
cmake --build build/RenderPipelineDemo --config Debug --target RenderPipelineDemo --parallel 4

build\RenderPipelineDemo\Debug\RenderPipelineDemo.exe --api=vulkan
build\RenderPipelineDemo\Debug\RenderPipelineDemo.exe --api=d3d11
build\RenderPipelineDemo\Debug\RenderPipelineDemo.exe --api=d3d12
```

`--frames=N` 可用于自动退出测试。Vulkan 构建需要 Vulkan SDK 中的 `glslangValidator`，它会将
共享的 GLSL shader 编译为 SPIR-V；D3D11/D3D12 在运行时编译共享 HLSL。
