#include <render/render_command_buffer.h>

namespace render
{
	enum class Commands : uint8_t
	{
		Close,
		SetPipelineState,
		SetVertexBuffers,
		SetIndexBuffer,
		SetConstants,
		SetConstantBuffer,
		SetUnorderedAccessBuffer,
		SetShaderResource,
		SetDescriptorTable,
		SetSamplerDescriptorTable,
		Draw,
		DrawIndexed,
		DrawIndexedInstanced,
		ExecuteCompute,
		Custom
	};

}