#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;

void main() {
    // Vulkan's positive-height viewport maps NDC -Y to the client-area top.
    gl_Position = vec4(inPosition.x * 2.0 - 1.0, inPosition.y * 2.0 - 1.0, 0.0, 1.0);
    vUV = inUV;
    vColor = inColor;
}
