
#include "constants.hlsl"

ConstantBuffer<ViewDataStruct> ViewData : register(b0);

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

//https://www.shadertoy.com/view/Nlffzj
// 3D Gradient noise from: https://www.shadertoy.com/view/Xsl3Dl
float3 hash( float3 p ) // replace this by something better
{
	p = float3( dot(p,float3(127.1,311.7, 74.7)),
			  dot(p,float3(269.5,183.3,246.1)),
			  dot(p,float3(113.5,271.9,124.6)));

	return -1.0 + 2.0*frac(sin(p)*43758.5453123);
}

float noise( in float3 p )
{
    float3 i = floor( p );
    float3 f = frac( p );
	
	float3 u = f*f*(3.0-2.0*f);

    return saturate(lerp( lerp( lerp( dot( hash( i + float3(0.0,0.0,0.0) ), f - float3(0.0,0.0,0.0) ), 
                          dot( hash( i + float3(1.0,0.0,0.0) ), f - float3(1.0,0.0,0.0) ), u.x),
                     lerp( dot( hash( i + float3(0.0,1.0,0.0) ), f - float3(0.0,1.0,0.0) ), 
                          dot( hash( i + float3(1.0,1.0,0.0) ), f - float3(1.0,1.0,0.0) ), u.x), u.y),
                lerp( lerp( dot( hash( i + float3(0.0,0.0,1.0) ), f - float3(0.0,0.0,1.0) ), 
                          dot( hash( i + float3(1.0,0.0,1.0) ), f - float3(1.0,0.0,1.0) ), u.x),
                     lerp( dot( hash( i + float3(0.0,1.0,1.0) ), f - float3(0.0,1.0,1.0) ), 
                          dot( hash( i + float3(1.0,1.0,1.0) ), f - float3(1.0,1.0,1.0) ), u.x), u.y), u.z ));
}

// from Unity's black body Shader Graph node
float3 Unity_Blackbody_float(float Temperature)
{
    float3 color = float3(255.0, 255.0, 255.0);
    color.x = 56100000. * pow(Temperature,(-3.0 / 2.0)) + 148.0;
    color.y = 100.04 * log(Temperature) - 623.6;
    if (Temperature > 6500.0) color.y = 35200000.0 * pow(Temperature,(-3.0 / 2.0)) + 184.0;
    color.z = 194.18 * log(Temperature) - 1448.6;
    color = clamp(color, 0.0, 255.0)/255.0;
    if (Temperature < 1000.0) color *= Temperature/1000.0;
    return color;
}

float4 ps_main(PSInput input) : SV_TARGET
{
    //Calculate view vector
    float2 viewport_position = 2.f * input.tex - 1.f;
    float4 screen_position = float4(viewport_position.x, -viewport_position.y, 1.f, 1.f);

    float4 world_position = mul(ViewData.view_projection_matrix_inv, screen_position);
    world_position = world_position / world_position.w;

    float3 view_vector = normalize(world_position.xyz - ViewData.camera_position.xyz);

    // Stars computation:
    float3 stars_direction = view_vector * 1.2f; // could be view vector for example
	float stars_threshold = 10.0f; // modifies the number of stars that are visible
	float stars_exposure = 1000.0f; // modifies the overall strength of the stars
	float stars = pow(clamp(noise(stars_direction * 200.0f), 0.0f, 1.0f), stars_threshold) * stars_exposure;
	stars *= lerp(0.3, 1.6, noise(stars_direction * 100.0f + float3(ViewData.time.xxx))); // time based flickering

     // star color by randomized temperature
    float stars_temperature = noise(stars_direction * 150.0) * 0.5 + 0.5;
    float3 stars_color = Unity_Blackbody_float(lerp(1500.0, 65000.0, pow(stars_temperature,2.0)));

    return float4(stars * stars_color, 1.f);
}