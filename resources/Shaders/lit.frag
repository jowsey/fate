#version 460 core
#extension GL_ARB_bindless_texture : require

struct MaterialData {
    sampler2D albedoMap;
    float metallic;
    float roughness;
};

layout(std430, binding = 1) readonly buffer MaterialBuffer {
    MaterialData materials[];
};

layout(location = 0) flat in uint drawID;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

layout(location = 0) out vec4 fragColor;

void main() {
    fragColor = texture(materials[drawID].albedoMap, texCoord);
}