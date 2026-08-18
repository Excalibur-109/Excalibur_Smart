// D3D11/D3D12 共享这一份 HLSL。RHI 在运行时分别以 vs_5_0/ps_5_0 或
// vs_5_1/ps_5_1 编译不同入口点，资源寄存器布局与 Vulkan set=0,binding=0 对齐。
Texture2D<float4> uiTexture : register(t0);
SamplerState uiSampler : register(s0);

struct UIVertexInput {
    float2 position : POSITION0;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
};

struct UIVertexOutput {
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
};

UIVertexOutput UIVS(UIVertexInput input) {
    UIVertexOutput output;
    // D3D 正高度 viewport 的 NDC +Y 在顶部，所以这里需要翻转归一化 y。
    output.position = float4(input.position.x * 2.0F - 1.0F, 1.0F - input.position.y * 2.0F, 0.0F, 1.0F);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

float4 UIPS(UIVertexOutput input) : SV_TARGET0 {
    // 与 GLSL fragment shader 保持完全相同的采样乘顶点色语义。
    return uiTexture.Sample(uiSampler, input.uv) * input.color;
}
