struct ConstantBufferTest
{
	float4 position;
	float4 color;
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

	result.position = position + constant_buffer_test.position;
	result.color = constant_buffer_test.color;

	return result;
}

float4 ps_main(PSInput input) : SV_TARGET
{
	return input.color;
}
