#version 460 core

layout(std430, binding = 0) readonly buffer TransformBuffer {
    mat4 modelMatrices[];
};

layout(location = 0) uniform mat4 uVP;

layout(location = 0) in vec4 aBaseColour;
layout(location = 1) in vec3 aPosition;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec2 aTexCoord;

layout(location = 0) flat out uint instanceID;
layout(location = 1) out vec4 vBaseColour;
layout(location = 2) out vec3 vNormal;
layout(location = 3) out vec2 vTexCoord;

void main() {
    instanceID = gl_BaseInstance + gl_InstanceID;

    mat4 modelMatrix = modelMatrices[instanceID];
    gl_Position = uVP * modelMatrix * vec4(aPosition, 1.0);

    vBaseColour = aBaseColour;
    vNormal = mat3(transpose(inverse(modelMatrix))) * aNormal;
    vTexCoord = aTexCoord.xy;
}