struct ConstantBufferTest
{
	float4 position;
	float4 color;
	float4 size;
};

struct PSInput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

ConstantBufferTest constant_buffer_test : c0;

PSInput vs_main(float4 position : POSITION)
{
	PSInput result;

	result.position.xy = position.xy * constant_buffer_test.size.xx + constant_buffer_test.position.xy;
	result.position.zw = position.zw;
	result.color = constant_buffer_test.color;

	return result;
}

float4 ps_main(PSInput input) : SV_TARGET
{
	return input.color;
}
