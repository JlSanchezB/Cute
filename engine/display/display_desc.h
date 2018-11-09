//////////////////////////////////////////////////////////////////////////
// Cute engine - Display layer
//////////////////////////////////////////////////////////////////////////
#ifndef DISPLAY_DESC_H_
#define DISPLAY_DESC_H_

#include "display_enum.h"

#include <cstdint>
#include <array>
#include <limits>

namespace display
{
	static const size_t kMaxNumRenderTargets = 8;
	static const size_t kMaxNumInputLayoutElements = 32;
	static const size_t kMaxNumRootParameters = 32;
	static const size_t kMaxNumStaticSamplers = 32;

	struct SamplerDesc
	{
		Filter filter = Filter::Point;
		TextureAddressMode address_u = TextureAddressMode::Clamp;
		TextureAddressMode address_v = TextureAddressMode::Clamp;
		TextureAddressMode address_w = TextureAddressMode::Clamp;
		float mip_lod_bias = 0.f;
		uint32_t max_anisotropy = 0;
		float min_lod = 0.f;
		float max_lod = std::numeric_limits<float>::max();
	};

	struct StaticSamplerDesc : SamplerDesc
	{
		size_t shader_register;
		ShaderVisibility visibility;
	};

	struct RootSignatureBaseParameter
	{
		size_t shader_register;
	};

	struct RootSignatureTableRange
	{
		DescriptorTableParameterType type;
		size_t base_shader_register;
		size_t size;
	};

	struct RootSignatureTable
	{
		static const size_t kNumMaxRanges = 8;
		size_t num_ranges;
		std::array<RootSignatureTableRange, kNumMaxRanges> range;
	};

	struct RootSignatureParameter
	{
		RootSignatureParameterType type;
		ShaderVisibility visibility;
		union
		{
			RootSignatureBaseParameter root_param;
			RootSignatureTable table;
		};
	};

	struct RootSignatureDesc
	{
		size_t num_root_parameters = 0;
		std::array<RootSignatureParameter, kMaxNumRootParameters> root_parameters;
		size_t num_static_samplers = 0;
		std::array< StaticSamplerDesc, kMaxNumStaticSamplers> static_samplers;
	};

	struct InputElementDesc
	{
		const char* semantic_name;
		uint32_t semantic_index;
		Format format;
		uint32_t input_slot;
		size_t aligned_offset;
		InputType input_type = InputType::Vertex;
		uint32_t instance_step_rate = 0;
		
		InputElementDesc()
		{
		}
		InputElementDesc(const char* _semantic_name, uint32_t _semantic_index, Format _format, uint32_t _input_slot, size_t _aligned_offset) :
			semantic_name(_semantic_name), semantic_index(_semantic_index), format(_format), input_slot(_input_slot), aligned_offset(_aligned_offset)
		{
		}
	};

	struct InputLayoutDesc
	{
		std::array<InputElementDesc, kMaxNumInputLayoutElements> elements;
		size_t num_elements = 0;
	};

	struct RasterizationDesc
	{
		FillMode fill_mode = FillMode::Solid;
		CullMode cull_mode = CullMode::Front;
		int32_t depth_bias = 0;
		float depth_bias_clamp = 0.f;
		float slope_depth_bias = 0.f;
		bool depth_clip_enable = true;
		bool multisample_enable = false;
		uint32_t forced_sample_count = 1;
		bool convervative_mode = false;
	};

	struct RenderTargetBlendDesc
	{
		bool blend_enable = false;
		Blend src_blend = Blend::Zero;
		Blend dest_blend = Blend::One;
		BlendOp blend_op = BlendOp::Add;
		Blend alpha_src_blend = Blend::Zero;
		Blend alpha_dest_blend = Blend::One;
		BlendOp alpha_blend_op = BlendOp::Add;
		uint8_t write_mask = 0b1111;
	};

	struct BlendDesc
	{
		bool alpha_to_coverage_enable = false;
		bool independent_blend_enable = false;
		std::array <RenderTargetBlendDesc, kMaxNumRenderTargets> render_target_blend;
	};

	struct ShaderDesc
	{
		const void* data = nullptr;
		size_t size = 0;
	};

	struct PipelineStateDesc
	{
		WeakRootSignatureHandle root_signature;
		
		ShaderDesc vertex_shader;
		ShaderDesc pixel_shader;
		
		InputLayoutDesc input_layout;
		RasterizationDesc rasteritation_state;
		BlendDesc blend_desc;

		bool depth_enable = false;
		bool stencil_enable = false;

		Topology primitive_topology;

		uint8_t num_render_targets = 0;
		std::array<Format, kMaxNumRenderTargets> render_target_format;

		uint8_t sample_count = 1;
	};

	struct Viewport
	{
		float top_left_x = 0.f;
		float top_left_y = 0.f;
		float width;
		float height;
		float min_depth = 0.f;
		float max_depth = 1.f;

		Viewport(float _width, float _height) : width(_width), height(_height)
		{
		}
	};

	struct Rect
	{
		size_t left;
		size_t top;
		size_t right;
		size_t bottom;
		
		Rect()
		{
		}

		Rect(size_t _left, size_t _top, size_t _right, size_t _bottom) :
			left(_left), top(_top), right(_right), bottom(_bottom)
		{
		}
	};

	struct VertexBufferDesc
	{
		size_t size = 0;
		const void* init_data = nullptr;
		size_t stride = 0;
	};

	struct IndexBufferDesc
	{
		size_t size = 0;
		const void* init_data = nullptr;
		Format format = Format::R16_UINT;
	};

	struct ConstantBufferDesc
	{
		Access access = Access::Static;
		size_t size = 0;
		const void* init_data = nullptr;
	};

	struct UnorderedAccessBufferDesc
	{
		size_t element_size = 0;
		size_t element_count = 0;
	};

	struct ShaderResourceDesc
	{
		Access access = Access::Static;
		Format format = Format::R8G8B8A8_UNORM;
		ShaderResourceType type = ShaderResourceType::Texture2D;
		size_t width = 0;
		size_t heigth = 0;
		size_t pitch = 0;
		size_t slice_pitch = 0;
		size_t mips = 0;
		size_t size = 0;
		const void* init_data = nullptr;
	};

	struct RenderTargetDesc
	{
		Format format;
		RenderTargetType render_target_type = RenderTargetType::Texture2D;
		size_t width = 0;
		size_t heigth = 0;
	};

	struct DepthBufferDesc
	{
		size_t width = 0;
		size_t heigth = 0;
	};

	struct DescriptorTableDesc
	{
		Access access = Access::Static;
		static const size_t kNumMaxDescriptors = 32;
		RootSignatureParameterType type; //Can not be a descriptor table

		std::array<WeakConstantBufferHandle, kNumMaxDescriptors> constant_buffer_table;
		std::array<WeakUnorderedAccessBufferHandle, kNumMaxDescriptors> unordered_access_buffer_table;
		std::array<WeakShaderResourceHandle, kNumMaxDescriptors> shader_resource_table;

		size_t num_descriptors = 0;
	};
}
#endif

