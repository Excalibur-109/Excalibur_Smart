#version 450

// binding 0 对纯色控件绑定 1x1 白纹理，对 Image 控件绑定实际材质纹理。
// 统一成 combined sampler 后，UI draw 不需要为 Panel 和 Image 使用两套 PSO。
layout(set = 0, binding = 0) uniform sampler2D uiTexture;

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 outColor;

void main() {
    // 白纹理的采样值为 (1,1,1,1)，所以 Panel 的外观由 vColor 完全决定；
    // 图片则通过纹理颜色和顶点色相乘获得统一的 tint/alpha 行为。
    outColor = texture(uiTexture, vUV) * vColor;
}
