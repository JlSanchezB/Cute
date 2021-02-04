//////////////////////////////////////////////////////////////////////////
// Cute engine - Conversion from cute to DX12
//////////////////////////////////////////////////////////////////////////
#ifndef DISPLAY_CONVERT_H_
#define DISPLAY_CONVERT_H_

#include <display/display_enum.h>
#include <d3d12.h>

namespace display
{
	template<typename TARGET_TYPE, typename SOURCE_TYPE>
	TARGET_TYPE Convert(SOURCE_TYPE type);

	inline DXGI_FORMAT Convert(Format format)
	{
		switch (format)
		{
		default:
		case Format::UNKNOWN: return DXGI_FORMAT_UNKNOWN;
		case Format::R32G32_FLOAT: return DXGI_FORMAT_R32G32_FLOAT;
		case Format::R32G32B32_FLOAT: return DXGI_FORMAT_R32G32B32_FLOAT;
		case Format::R32G32B32A32_FLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case Format::R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
		case Format::R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		case Format::R32_UINT: return DXGI_FORMAT_R32_UINT;
		case Format::R16_UINT: return DXGI_FORMAT_R16_UINT;
		case Format::D32_FLOAT: return DXGI_FORMAT_D32_FLOAT;
		}
	}

	inline D3D12_FILL_MODE Convert(FillMode fill_mode)
	{
		switch (fill_mode)
		{
		default:
		case FillMode::Solid: return D3D12_FILL_MODE_SOLID;
		case FillMode::Wireframe: return D3D12_FILL_MODE_WIREFRAME;
		}
	}

	inline D3D12_CULL_MODE Convert(CullMode cull_mode)
	{
		switch (cull_mode)
		{
		default:
		case CullMode::None: return D3D12_CULL_MODE_NONE;
		case CullMode::Front: return D3D12_CULL_MODE_FRONT;
		case CullMode::Back: return D3D12_CULL_MODE_BACK;
		}
	}

	inline D3D12_BLEND Convert(Blend blend)
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

	inline D3D12_BLEND_OP Convert(BlendOp blend_op)
	{
		switch (blend_op)
		{
		default:
		case BlendOp::Add: return D3D12_BLEND_OP_ADD;
		case BlendOp::Substract: return D3D12_BLEND_OP_SUBTRACT;
		}
	}

	inline D3D12_COMPARISON_FUNC Convert(ComparationFunction comparation_function)
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

	inline D3D12_PRIMITIVE_TOPOLOGY_TYPE Convert(Topology topology)
	{
		switch (topology)
		{
		default:
		case Topology::Triangle: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		}
	}

	inline D3D_PRIMITIVE_TOPOLOGY Convert(PrimitiveTopology primitive_topology)
	{
		switch (primitive_topology)
		{
		default:
		case PrimitiveTopology::TriangleList: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		}
	}

	inline D3D12_INPUT_CLASSIFICATION Convert(InputType input_type)
	{
		switch (input_type)
		{
		default:
		case InputType::Vertex: return D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		case InputType::Instance: return D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
		}
	}

	inline D3D12_ROOT_PARAMETER_TYPE Convert(RootSignatureParameterType root_signature_parameter)
	{
		switch (root_signature_parameter)
		{
		default:
		case RootSignatureParameterType::ConstantBuffer: return D3D12_ROOT_PARAMETER_TYPE_CBV;
		case RootSignatureParameterType::ShaderResource: return D3D12_ROOT_PARAMETER_TYPE_SRV;
		case RootSignatureParameterType::UnorderedAccessBuffer: return D3D12_ROOT_PARAMETER_TYPE_UAV;
		case RootSignatureParameterType::DescriptorTable: return D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		}
	}

	inline D3D12_DESCRIPTOR_RANGE_TYPE Convert(DescriptorTableParameterType descriptor_table_parameter_type)
	{
		switch (descriptor_table_parameter_type)
		{
		default:
		case DescriptorTableParameterType::ConstantBuffer: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		case DescriptorTableParameterType::ShaderResource: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		case DescriptorTableParameterType::UnorderedAccessBuffer: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		case DescriptorTableParameterType::Sampler: return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
		}
	}

	inline D3D12_SHADER_VISIBILITY Convert(ShaderVisibility shader_visibility)
	{
		switch (shader_visibility)
		{
		default:
		case ShaderVisibility::All: return D3D12_SHADER_VISIBILITY_ALL;
		case ShaderVisibility::Vertex: return D3D12_SHADER_VISIBILITY_VERTEX;
		case ShaderVisibility::Hull: return D3D12_SHADER_VISIBILITY_HULL;
		case ShaderVisibility::Domain: return D3D12_SHADER_VISIBILITY_DOMAIN;
		case ShaderVisibility::Geometry: return D3D12_SHADER_VISIBILITY_GEOMETRY;
		case ShaderVisibility::Pixel: return D3D12_SHADER_VISIBILITY_PIXEL;
		}
	}

	inline D3D12_FILTER Convert(Filter filter)
	{
		switch (filter)
		{
		default:
		case Filter::Point: return D3D12_FILTER_MIN_MAG_MIP_POINT;
		case Filter::Linear: return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		case Filter::Anisotropic: return D3D12_FILTER_ANISOTROPIC;
		}
	}

	inline D3D12_TEXTURE_ADDRESS_MODE Convert(TextureAddressMode texture_address_mode)
	{
		switch (texture_address_mode)
		{
		default:
		case TextureAddressMode::Wrap: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		case TextureAddressMode::Mirror: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		case TextureAddressMode::Clamp: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		}
	}


	inline D3D12_STATIC_SAMPLER_DESC Convert(StaticSamplerDesc static_sample_desc)
	{
		D3D12_STATIC_SAMPLER_DESC ret;
		ret.Filter = Convert(static_sample_desc.filter);
		ret.AddressU = Convert(static_sample_desc.address_u);
		ret.AddressV = Convert(static_sample_desc.address_v);
		ret.AddressW = Convert(static_sample_desc.address_w);
		ret.MipLODBias = static_sample_desc.mip_lod_bias;
		ret.MaxAnisotropy = static_cast<UINT>(static_sample_desc.max_anisotropy);
		ret.MinLOD = static_sample_desc.min_lod;
		ret.MaxLOD = static_sample_desc.max_lod;
		ret.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		ret.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		ret.ShaderRegister = static_cast<UINT>(static_sample_desc.shader_register);
		ret.RegisterSpace = 0;
		ret.ShaderVisibility = Convert(static_sample_desc.visibility);

		return ret;
	}

	inline D3D12_SAMPLER_DESC Convert(SamplerDesc sample_desc)
	{
		D3D12_SAMPLER_DESC ret;
		ret.Filter = Convert(sample_desc.filter);
		ret.AddressU = Convert(sample_desc.address_u);
		ret.AddressV = Convert(sample_desc.address_v);
		ret.AddressW = Convert(sample_desc.address_w);
		ret.MipLODBias = sample_desc.mip_lod_bias;
		ret.MaxAnisotropy = static_cast<UINT>(sample_desc.max_anisotropy);
		ret.MinLOD = sample_desc.min_lod;
		ret.MaxLOD = sample_desc.max_lod;
		ret.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		ret.BorderColor[0] = ret.BorderColor[1] = ret.BorderColor[2] = ret.BorderColor[3] = 0;

		return ret;
	}

	template<>
	inline D3D12_RESOURCE_DIMENSION Convert<D3D12_RESOURCE_DIMENSION, ShaderResourceType>(ShaderResourceType shader_resource_type)
	{
		switch (shader_resource_type)
		{
		default:
		case ShaderResourceType::Buffer: return D3D12_RESOURCE_DIMENSION_BUFFER;
		case ShaderResourceType::Texture2D: return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		}
	}

	template<>
	inline D3D12_SRV_DIMENSION Convert<D3D12_SRV_DIMENSION, ShaderResourceType>(ShaderResourceType shader_resource_type)
	{
		switch (shader_resource_type)
		{
		default:
		case ShaderResourceType::Buffer: return D3D12_SRV_DIMENSION_BUFFER;
		case ShaderResourceType::Texture2D: return D3D12_SRV_DIMENSION_TEXTURE2D;
		}
	}

	inline D3D12_RESOURCE_STATES Convert(ResourceState resource_state)
	{
		switch (resource_state)
		{
		default:
		case ResourceState::RenderTarget: return D3D12_RESOURCE_STATE_RENDER_TARGET;
		case ResourceState::PixelShaderResource: return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		case ResourceState::NonPixelShaderResource: return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		}
	}

	inline D3D12_RESOURCE_BARRIER_TYPE Convert(ResourceBarrierType resource_barrier_type)
	{
		switch (resource_barrier_type)
		{
		default:
		case ResourceBarrierType::Transition: return D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		case ResourceBarrierType::UnorderAccess: return D3D12_RESOURCE_BARRIER_TYPE_UAV;
		}
	}

	inline D3D12_RESOURCE_STATES Convert(TranstitionState transition_state)
	{
		switch (transition_state)
		{
		default:
		case TranstitionState::Common: return D3D12_RESOURCE_STATE_COMMON;
		case TranstitionState::UnorderedAccess: return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		case TranstitionState::VertexAndConstantBuffer: return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		case TranstitionState::RenderTarget: return D3D12_RESOURCE_STATE_RENDER_TARGET;
		case TranstitionState::PixelShaderResource: return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		case TranstitionState::NonPixelShaderResource: return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		case TranstitionState::AllShaderResource: return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		case TranstitionState::Depth: return D3D12_RESOURCE_STATE_DEPTH_WRITE;
		case TranstitionState::DepthRead: return D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		}
	}
}
#endif //DISPLAY_CONVERT_H_
