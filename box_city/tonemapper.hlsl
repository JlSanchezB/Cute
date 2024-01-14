
#include "constants.hlsl"

Texture2D light_scene : t0;
SamplerState light_scene_sampler : s0;
Texture2D light_scene_4 : t1;

CONTROL_VARIABLE(bool, PostProcess, BloomEnable, true)

struct PSInput
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD0;
};

PSInput vs_main(float4 position : POSITION, float2 tex : TEXCOORD0)
{
    PSInput result;

    result.position = position;
    result.tex = tex;

    return result;
}

float3 tonemap_uncharted2(in float3 x)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;

    return pow(((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F, 1.f / 2.2f);
}

float3 tonemap_filmic(in float3 color)
{
    color = max(0, color - 0.004f);
    color = (color * (6.2f * color + 0.5f)) / (color * (6.2f * color + 1.7f)+ 0.06f);

    // result has 1/2.2 baked in
    return pow(color, 1.f / 2.2f);
}

// Reinhard Tonemapper
float3 tonemap_reinhard(in float3 color)
{
   color = color/(1+color);
   float3 ret = pow(color, 1.f / 2.2f); // gamma
   return ret;
}

float3 tonemap_srgb(in float3 color)
{
   float3 ret = pow(color, 1.f / 2.2f); // gamma
   return ret;
}

//Aces tonemap

// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
static const float3x3 ACESInputMat =
{
    {0.59719, 0.35458, 0.04823},
    {0.07600, 0.90834, 0.01566},
    {0.02840, 0.13383, 0.83777}
};

// ODT_SAT => XYZ => D60_2_D65 => sRGB
static const float3x3 ACESOutputMat =
{
    { 1.60475, -0.53108, -0.07367},
    {-0.10208,  1.10813, -0.00605},
    {-0.00327, -0.07276,  1.07602}
};

float3 RRTAndODTFit(float3 v)
{
    float3 a = v * (v + 0.0245786f) - 0.000090537f;
    float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

float3 ACESFitted(float3 color)
{
    color = mul(ACESInputMat, color);

    // Apply RRT and ODT
    color = RRTAndODTFit(color);

    color = mul(ACESOutputMat, color);

    // Clamp to [0, 1]
    color = saturate(color);

    return color;
}

float3 aces_tonemap(in float3 color)
{
	return pow(ACESFitted(color), 1.f / 2.2f);
}

//	<https://www.shadertoy.com/view/4dS3Wd>
//	By Morgan McGuire @morgan3d, http://graphicscodex.com
//
float hash(float n) { return frac(sin(n) * 1e4); }
float hash(float2 p) { return frac(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x)))); }

float noise(float x) {
	float i = floor(x);
	float f = frac(x);
	float u = f * f * (3.0 - 2.0 * f);
	return lerp(hash(i), hash(i + 1.0), u);
}

float noise(float2 x) {
	float2 i = floor(x);
	float2 f = frac(x);

	// Four corners in 2D of a tile
	float a = hash(i);
	float b = hash(i + float2(1.0, 0.0));
	float c = hash(i + float2(0.0, 1.0));
	float d = hash(i + float2(1.0, 1.0));

	// Simple 2D lerp using smoothstep envelope between the values.
	// return float3(lerp(lerp(a, b, smoothstep(0.0, 1.0, f.x)),
	//			lerp(c, d, smoothstep(0.0, 1.0, f.x)),
	//			smoothstep(0.0, 1.0, f.y)));

	// Same code, with the clamps in smoothstep and common subexpressions
	// optimized away.
	float2 u = f * f * (3.0 - 2.0 * f);
	return lerp(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

float4 ps_main(PSInput input) : SV_TARGET
{
    float3 colour = light_scene.Sample(light_scene_sampler, input.tex).rgb * exposure;
    
    //Add Bloom
    float3 bloom = light_scene_4.Sample(light_scene_sampler, input.tex).rgb * bloom_intensity * exposure;

    if (!BloomEnable) bloom = float3(0.f, 0.f, 0.f);

    float3 result = colour + bloom + noise(input.tex * 1000.f) / 256.f;

    //Tonemapper
    result = tonemap_reinhard(result);

    return float4(result, 1.f);
}