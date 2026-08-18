#version 450

// UI 顶点由 CPU 直接写成客户区归一化坐标：[0,0] 是左上角，[1,1] 是右下角。
// 这里不使用 uniform 矩阵，窗口尺寸变化只会改变 CPU 归一化时的分母。
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;

void main() {
    // Vulkan 正高度 viewport 中 NDC 的 -Y 位于顶部，因此归一化 y 直接映射到 -1..1。
    // D3D 的 HLSL 版本使用相反的 y 公式；两者最终都把像素 (0,0) 放到左上角。
    gl_Position = vec4(inPosition.x * 2.0 - 1.0, inPosition.y * 2.0 - 1.0, 0.0, 1.0);
    vUV = inUV;
    vColor = inColor;
}
