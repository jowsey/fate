import "lib/PBRUtils";
import "lib/TonemappingUtils";

struct MaterialData
{
    float4 baseColour;
    uint32_t albedoMapIndex;
    uint32_t normalMapIndex;
    uint32_t metallicMapIndex;
    uint32_t roughnessMapIndex;
    uint32_t emissiveMapIndex;
    uint32_t mapFlags; // albedo, normal, metallic, roughness, emissive, -, -, -
    float metallic;
    float roughness;
}

struct LightData
{
    float3 direction;
    float range;
    float3 colour;
    float intensity;
}

struct FrameGlobals
{
    float4x4 view;
    float4x4 projection;
    float3 cameraPosition;
    float _padding; // todo why here specifically?

    LightData light;
};

struct ObjectData
{
    float4x4 model;
    float4x4 inverseTransposeModel;
    MaterialData material;
};

struct VSInput
{
    float4 baseColour;
    float3 position;
    float3 normal;
    float4 tangent;
    float2 uv;
};

struct VSOutput
{
    nointerpolation uint32_t instanceID;

    float4 baseColour;
    float4 position : SV_Position;
    float3 worldNormal;
    float3 worldTangent;
    float3 worldBitangent;
    float2 uv;
    float3 worldPos;
};

struct PushConstants
{
    FrameGlobals *globals;
    ObjectData *objects;
}

Sampler2D textures[];

[shader("vertex")]
VSOutput vertexMain(VSInput input, uniform PushConstants pc, uint instanceIndex: SV_VulkanInstanceID)
{
    ObjectData object = pc.objects[instanceIndex];

    VSOutput output;
    output.instanceID = instanceIndex;
    output.baseColour = input.baseColour;
    output.position = mul(pc.globals->projection, mul(pc.globals->view, mul(object.model, float4(input.position, 1.0))));

    output.worldNormal = normalize(mul(object.inverseTransposeModel, float4(input.normal, 0.0)).xyz);
    output.worldTangent = normalize(mul(object.model, float4(input.tangent.xyz, 0.0)).xyz);
    output.worldBitangent = cross(output.worldNormal, output.worldTangent) * input.tangent.w;

    output.uv = input.uv;
    output.worldPos = mul(object.model, float4(input.position, 1.0)).xyz;

    return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input, uniform PushConstants pc)
{
    MaterialData material = pc.objects[input.instanceID].material;

    bool hasAlbedoMap = (material.mapFlags & (1 << 0)) != 0;
    bool hasNormalMap = (material.mapFlags & (1 << 1)) != 0;
    bool hasMetallicMap = (material.mapFlags & (1 << 2)) != 0;
    bool hasRoughnessMap = (material.mapFlags & (1 << 3)) != 0;
    bool hasEmissiveMap = (material.mapFlags & (1 << 4)) != 0;

    float3x3 TBN = float3x3(normalize(input.worldTangent), normalize(input.worldBitangent), normalize(input.worldNormal));

    float4 albedo = (hasAlbedoMap ? textures[NonUniformResourceIndex(material.albedoMapIndex)].Sample(input.uv) : float4(1.0)) * material.baseColour * input.baseColour;
    float3 normal = (hasNormalMap ? normalize(mul(textures[NonUniformResourceIndex(material.normalMapIndex)].Sample(input.uv).xyz * 2.0 - 1.0, TBN)) : normalize(input.worldNormal));
    float metallic = (hasMetallicMap ? textures[NonUniformResourceIndex(material.metallicMapIndex)].Sample(input.uv).b : 0.0) * material.metallic;
    float roughness = (hasRoughnessMap ? textures[NonUniformResourceIndex(material.roughnessMapIndex)].Sample(input.uv).g : 0.0) * material.roughness;
    float3 emissive = (hasEmissiveMap ? textures[NonUniformResourceIndex(material.emissiveMapIndex)].Sample(input.uv).rgb : float3(0.0));

    // todo additive array
    float3 lightDir = pc.globals->light.direction;
    float3 lightColour = pc.globals->light.colour;
    float lightIntensity = pc.globals->light.intensity;
    float3 radiance = lightColour * lightIntensity;

    float3 N = normalize(normal);
    float3 V = normalize(pc.globals.cameraPosition - input.worldPos);
    float3 L = normalize(-lightDir);
    float3 H = normalize(V + L);

    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);

    float3 F0 = float3(0.04);
    F0 = lerp(F0, albedo.rgb, metallic);

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    float3 numerator = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.0001;
    float3 specular = numerator / denominator;

    float3 kS = F;
    float3 kD = float3(1.0) - kS;
    kD *= 1.0 - metallic;

    float3 Lo = (kD * albedo.rgb / PI + specular) * radiance * NdotL;

    return float4(linearToSRGB(ACESFitted(Lo) + emissive), 1.0);
}
