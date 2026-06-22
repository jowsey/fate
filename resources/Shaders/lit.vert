#version 460 core

layout(std430, binding = 0) readonly buffer TransformBuffer {
    mat4 modelMatrices[];
};

layout(location = 0) uniform mat4 uVP;

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

layout(location = 0) flat out uint drawID;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec2 vTexCoord;

void main() {
    gl_Position = uVP * modelMatrices[gl_DrawID] * vec4(aPosition, 1.0);

    drawID = gl_DrawID;
    vNormal = mat3(transpose(inverse(modelMatrices[gl_DrawID]))) * aNormal;
    vTexCoord = aTexCoord.xy;
}