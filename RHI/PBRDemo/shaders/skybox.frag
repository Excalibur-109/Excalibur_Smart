#version 450

layout(set = 0, binding = 0) uniform PBRUBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 lightDir;
    vec4 lightColor;
    vec4 cameraPos;
    vec4 baseColor;
    vec4 materialParams;
    mat4 lightViewProjection;
    vec4 shadowParameters;
} ubo;

layout(set = 0, binding = 2) uniform samplerCube skyboxTexture;

layout(location = 0) in vec3 vDirection;
layout(location = 0) out vec4 outColor;

vec3 rotateSkyDirectionYExceptBottom(vec3 direction, float angle) {
    // Cubemap's -Y face is selected when the negative Y component is the dominant axis.
    // Do not rotate those directions, so the skybox bottom remains anchored.
    if (direction.y < 0.0 &&
        -direction.y >= abs(direction.x) &&
        -direction.y >= abs(direction.z)) {
        return direction;
    }

    float sine = sin(angle);
    float cosine = cos(angle);
    return vec3(
        cosine * direction.x - sine * direction.z,
        direction.y,
        sine * direction.x + cosine * direction.z);
}

void main() {
    vec3 color = texture(
        skyboxTexture,
        rotateSkyDirectionYExceptBottom(normalize(vDirection), ubo.materialParams.w)).rgb;
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = max(mix(vec3(luminance), color, 1.18) * 1.18, vec3(0.0));
    outColor = vec4(color, 1.0);
}
