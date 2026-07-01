struct MaterialData
{
    float4 baseColour;
    uint32_t albedoMapIndex;
    uint32_t mapFlags;
    float metallic;
    float roughness;
}

struct FrameGlobals
{
    float4x4 projection;
    float4x4 view;
    float4 lightPos;
};

struct ObjectData
{
    float4x4 model;
    MaterialData material;
};

struct VSInput
{
    float4 baseColour;
    float3 position;
    float3 normal;
    float2 uv;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 normal;
    float2 uv;
    float3 lightVec;
    float3 viewVec;
    uint32_t albedoIndex;
};

Sampler2D textures[];

[shader("vertex")]
VSOutput vertexMain(VSInput input, uniform FrameGlobals *globals, uniform ObjectData *objects, uint instanceIndex: SV_VulkanInstanceID)
{
    VSOutput output;
    float4x4 modelMat = objects[instanceIndex].model;

    output.normal = mul((float3x3)mul(globals->view, modelMat), input.normal);
    output.uv = input.uv;
    output.position = mul(globals->projection, mul(globals->view, mul(modelMat, float4(input.position.xyz, 1.0))));
    output.albedoIndex = objects[instanceIndex].material.albedoMapIndex;

    // Calculate view vectors required for lighting
    float4 fragPos = mul(mul(globals->view, modelMat), float4(input.position.xyz, 1.0));
    output.lightVec = globals->lightPos.xyz - fragPos.xyz;
    output.viewVec = -fragPos.xyz;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input)
{
    // Phong lighting
    float3 N = normalize(input.normal);
    float3 L = normalize(input.lightVec);
    float3 V = normalize(input.viewVec);
    float3 R = reflect(-L, N);
    float3 diffuse = max(dot(N, L), 0.0025);
    float3 specular = pow(max(dot(R, V), 0.0), 16.0) * 0.75;

    // Sample from texture
    float3 color = textures[NonUniformResourceIndex(input.albedoIndex)].Sample(input.uv).rgb;
    return float4(diffuse * color.rgb + specular, 1.0);
}
