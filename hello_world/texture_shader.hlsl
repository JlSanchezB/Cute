Texture2D texture_test : t0;
SamplerState static_sampler : s0;

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

float4 ps_main(PSInput input) : SV_TARGET
{
	return texture_test.Sample(static_sampler, input.tex);
}
