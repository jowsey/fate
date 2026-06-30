Sampler2D textures[];

struct FrameGlobals
{
    float4x4 projection;
    float4x4 view;
    float4 lightPos;
};

struct ObjectData
{
    float4x4 model;
    uint32_t albedoIndex;
};

struct VSInput
{
    float3 position;
    float3 normal;
    float2 uv;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 normal;
    float2 uv;
    float3 Factor;
    float3 LightVec;
    float3 ViewVec;
    uint32_t InstanceIndex;
};

[shader("vertex")]
VSOutput vertexMain(VSInput input, uniform FrameGlobals *globals, uniform ObjectData *objects, uint instanceIndex: SV_VulkanInstanceID)
{
    VSOutput output;
    float4x4 modelMat = objects[instanceIndex].model;

    output.normal = mul((float3x3)mul(globals->view, modelMat), input.normal);
    output.uv = input.uv;
    output.position = mul(globals->projection, mul(globals->view, mul(modelMat, float4(input.position.xyz, 1.0))));
    output.Factor = 1.0f;
    output.InstanceIndex = instanceIndex;

    // Calculate view vectors required for lighting
    float4 fragPos = mul(mul(globals->view, modelMat), float4(input.position.xyz, 1.0));
    output.LightVec = globals->lightPos.xyz - fragPos.xyz;
    output.ViewVec = -fragPos.xyz;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input)
{
    // Phong lighting
    float3 N = normalize(input.normal);
    float3 L = normalize(input.LightVec);
    float3 V = normalize(input.ViewVec);
    float3 R = reflect(-L, N);
    float3 diffuse = max(dot(N, L), 0.0025);
    float3 specular = pow(max(dot(R, V), 0.0), 16.0) * 0.75;

    // Sample from texture
    float3 color = textures[NonUniformResourceIndex(input.InstanceIndex)].Sample(input.uv).rgb * input.Factor;
    return float4(diffuse * color.rgb + specular, 1.0);
}
