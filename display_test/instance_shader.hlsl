
struct PSInput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

PSInput vs_main(float4 position : POSITION, float4 instance_data : TEXCOORD)
{
	PSInput result;

	result.position.xy = position.xy * instance_data.z + instance_data.xy;
	result.position.zw = position.zw;
	result.color = instance_data.wwww;

	return result;
}

float4 ps_main(PSInput input) : SV_TARGET
{
	return input.color;
}
