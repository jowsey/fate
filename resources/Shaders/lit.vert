#version 460 core

layout(std430, binding = 0) readonly buffer TransformBuffer {
    mat4 modelMatrices[];
};

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;

layout(location = 0) uniform mat4 uVP;

layout(location = 0) out vec3 customCol;

void main() {
    gl_Position = uVP * modelMatrices[gl_DrawID] * vec4(aPosition, 1.0);
    customCol = aColor;
}