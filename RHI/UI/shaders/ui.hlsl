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
    // Direct3D maps NDC +Y to the top of a positive-height viewport.
    output.position = float4(input.position.x * 2.0F - 1.0F, 1.0F - input.position.y * 2.0F, 0.0F, 1.0F);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

float4 UIPS(UIVertexOutput input) : SV_TARGET0 {
    return uiTexture.Sample(uiSampler, input.uv) * input.color;
}
