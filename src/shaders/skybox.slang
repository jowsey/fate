import "lib/TonemappingUtils";

static const float3 cubeVertices[36] =
    {
        // +X
        float3(1, -1, -1),
        float3(1, 1, -1),
        float3(1, 1, 1),
        float3(1, -1, -1),
        float3(1, 1, 1),
        float3(1, -1, 1),

        // -X
        float3(-1, -1, 1),
        float3(-1, 1, 1),
        float3(-1, 1, -1),
        float3(-1, -1, 1),
        float3(-1, 1, -1),
        float3(-1, -1, -1),

        // +Y
        float3(-1, 1, -1),
        float3(-1, 1, 1),
        float3(1, 1, 1),
        float3(-1, 1, -1),
        float3(1, 1, 1),
        float3(1, 1, -1),

        // -Y
        float3(-1, -1, 1),
        float3(-1, -1, -1),
        float3(1, -1, -1),
        float3(-1, -1, 1),
        float3(1, -1, -1),
        float3(1, -1, 1),

        // +Z
        float3(-1, -1, 1),
        float3(1, -1, 1),
        float3(1, 1, 1),
        float3(-1, -1, 1),
        float3(1, 1, 1),
        float3(-1, 1, 1),

        // -Z
        float3(1, -1, -1),
        float3(-1, -1, -1),
        float3(-1, 1, -1),
        float3(1, -1, -1),
        float3(-1, 1, -1),
        float3(1, 1, -1),
    };

struct SkyboxGlobals
{
    float4x4 view;
    float4x4 projection;
    uint32_t skyboxIndex;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 direction : TEXCOORD0;
};

SamplerCube textures[];

[shader("vertex")]
VSOutput vertexMain(uniform SkyboxGlobals *globals, uint vertexIndex: SV_VulkanVertexID)
{
    VSOutput output;

    float4x4 viewNoTranslation = globals->view;
    viewNoTranslation[0][3] = 0.0;
    viewNoTranslation[1][3] = 0.0;
    viewNoTranslation[2][3] = 0.0;

    output.position = mul(globals->projection, mul(viewNoTranslation, float4(cubeVertices[vertexIndex], 1.0))).xyww;
    output.direction = normalize(cubeVertices[vertexIndex]);
    return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input, uniform SkyboxGlobals *globals)
{
    input.direction.z *= -1.0; // vulkan coordinate system, todo standardise everywhere

    return float4(linearToSRGB(ACESFitted(textures[globals->skyboxIndex].Sample(input.direction).rgb)), 1.0);
    // return float4(input.direction * 0.5 + 0.5, 1.0);
}
