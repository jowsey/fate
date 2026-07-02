module TonemappingUtils;

// ACES fit by Stephen Hill (@self_shadow), as shared on https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
static const float3x3 ACESInputMat =
    {
        { 0.59719, 0.35458, 0.04823 },
        { 0.07600, 0.90834, 0.01566 },
        { 0.02840, 0.13383, 0.83777 }
    };

// ODT_SAT => XYZ => D60_2_D65 => sRGB
static const float3x3 ACESOutputMat =
    {
        { 1.60475, -0.53108, -0.07367 },
        { -0.10208, 1.10813, -0.00605 },
        { -0.00327, -0.07276, 1.07602 }
    };

static float3 RRTAndODTFit(float3 v)
{
    float3 a = v * (v + 0.0245786f) - 0.000090537f;
    float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

public static float3 ACESFitted(float3 color)
{
    color = mul(ACESInputMat, color);
    color = RRTAndODTFit(color);
    color = mul(ACESOutputMat, color);
    return saturate(color);
}

public static float3 linearToSRGB(float3 colour)
{
    return lerp(colour * 12.92, 1.055 * pow(colour, 1.0 / 2.4) - 0.055, step(0.0031308, colour));
}

public static float3 sRGBToLinear(float3 colour)
{
    return lerp(colour / 12.92, pow((colour + 0.055) / 1.055, 2.4), step(0.04045, colour));
}
