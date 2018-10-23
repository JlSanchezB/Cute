//////////////////////////////////////////////////////////////////////////
// Cute engine - Conversion from cute to DX12
//////////////////////////////////////////////////////////////////////////
#ifndef DISPLAY_CONVERT_H_
#define DISPLAY_CONVERT_H_

#include <display/display_enum.h>
#include <d3d12.h>

namespace display
{
	DXGI_FORMAT Convert(Format format)
	{
		switch (format)
		{
		default:
		case Format::R32G32B32_FLOAT: return DXGI_FORMAT_R32G32B32_FLOAT;
		case Format::R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
		case Format::R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		}
	}

	D3D12_FILL_MODE Convert(FillMode fill_mode)
	{
		switch (fill_mode)
		{
		default:
		case FillMode::Solid: return D3D12_FILL_MODE_SOLID;
		case FillMode::Wireframe: return D3D12_FILL_MODE_WIREFRAME;
		}
	}

	D3D12_CULL_MODE Convert(CullMode cull_mode)
	{
		switch (cull_mode)
		{
		default:
		case CullMode::None: return D3D12_CULL_MODE_NONE;
		case CullMode::Front: return D3D12_CULL_MODE_FRONT;
		case CullMode::Back: return D3D12_CULL_MODE_BACK;
		}
	}

	D3D12_BLEND Convert(Blend blend)
	{
		switch (blend)
		{
		default:
		case Blend::Zero: return D3D12_BLEND_ZERO;
		case Blend::One: return D3D12_BLEND_ONE;
		case Blend::SrcAlpha: return D3D12_BLEND_SRC_ALPHA;
		case Blend::InvSrcAlpha: return D3D12_BLEND_INV_SRC_ALPHA;
		}
	}

	D3D12_BLEND_OP Convert(BlendOp blend_op)
	{
		switch (blend_op)
		{
		default:
		case BlendOp::Add: return D3D12_BLEND_OP_ADD;
		case BlendOp::Substract: return D3D12_BLEND_OP_SUBTRACT;
		}
	}

	D3D12_COMPARISON_FUNC Convert(ComparationFunction comparation_function)
	{
		switch (comparation_function)
		{
		default:
		case ComparationFunction::Never: return D3D12_COMPARISON_FUNC_NEVER;
		case ComparationFunction::Less: return D3D12_COMPARISON_FUNC_LESS;
		case ComparationFunction::Equal: return D3D12_COMPARISON_FUNC_EQUAL;
		case ComparationFunction::Less_Equal: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
		case ComparationFunction::Greater: return D3D12_COMPARISON_FUNC_GREATER;
		case ComparationFunction::NotEqual: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
		case ComparationFunction::Greater_Equal: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		case ComparationFunction::Always: return D3D12_COMPARISON_FUNC_ALWAYS;
		}
	}

	D3D12_PRIMITIVE_TOPOLOGY_TYPE Convert(Topology topology)
	{
		switch (topology)
		{
		default:
		case Topology::Triangle: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		}
	}

	D3D12_INPUT_CLASSIFICATION Convert(InputType input_type)
	{
		switch (input_type)
		{
		default:
		case InputType::Vertex: return D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		case InputType::Instance: return D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
		}
	}
}
#endif //DISPLAY_CONVERT_H_
