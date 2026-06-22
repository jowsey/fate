#version 460 core
#extension GL_ARB_bindless_texture : require

struct MaterialData {
    vec4 baseColour;
    sampler2D albedoMap;
    uint mapFlags; // albedo, -, -, -, -, -, -, -
    float metallic;
    float roughness;
};

layout(std430, binding = 1) readonly buffer MaterialBuffer {
    MaterialData materials[];
};

layout(location = 0) flat in uint instanceID;
layout(location = 1) in vec4 vertexColour;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 texCoord;

layout(location = 0) out vec4 fragColor;

void main() {
    MaterialData material = materials[instanceID];

    bool hasAlbedoMap = (material.mapFlags & 0x1u) != 0u;
    fragColor = (hasAlbedoMap ? texture(material.albedoMap, texCoord) : vec4(1.0)) * material.baseColour * vertexColour;
}