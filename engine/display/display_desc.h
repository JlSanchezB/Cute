//////////////////////////////////////////////////////////////////////////
// Cute engine - Display layer
//////////////////////////////////////////////////////////////////////////
#ifndef DISPLAY_DESC_H_
#define DISPLAY_DESC_H_

#include "display_enum.h"

#include <cstdint>
#include <array>
#include <limits>
#include <variant>

namespace display
{
	constexpr size_t kMaxNumRenderTargets = 8;
	constexpr size_t kMaxNumInputLayoutElements = 32;
	constexpr size_t kMaxNumRootParameters = 32;
	constexpr size_t kMaxNumStaticSamplers = 32;

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
		uint16_t shader_register;
		ShaderVisibility visibility;
	};

	struct RootSignatureBaseParameter
	{
		uint16_t shader_register;
		uint16_t num_constants; //Only used in case it is a 32bits constant
	};

	struct RootSignatureTableRange
	{
		DescriptorTableParameterType type;
		uint16_t base_shader_register;
		uint16_t size;
	};

	struct RootSignatureTable
	{
		static constexpr size_t kNumMaxRanges = 8;
		uint16_t num_ranges;
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
		uint8_t num_root_parameters = 0;
		std::array<RootSignatureParameter, kMaxNumRootParameters> root_parameters;
		uint8_t num_static_samplers = 0;
		std::array< StaticSamplerDesc, kMaxNumStaticSamplers> static_samplers;
	};

	struct InputElementDesc
	{
		const char* semantic_name;
		uint16_t semantic_index;
		Format format;
		uint16_t input_slot;
		uint32_t aligned_offset = 0;
		InputType input_type = InputType::Vertex;
		uint32_t instance_step_rate = 0;
		
		InputElementDesc()
		{
		}
		InputElementDesc(const char* _semantic_name, uint16_t _semantic_index, Format _format, uint16_t _input_slot, uint32_t _aligned_offset, InputType _input_type = InputType::Vertex) :
			semantic_name(_semantic_name), semantic_index(_semantic_index), format(_format), input_slot(_input_slot), aligned_offset(_aligned_offset), input_type(_input_type)
		{
			instance_step_rate = (input_type == InputType::Vertex) ? 0 : 1;
		}
	};

	struct InputLayoutDesc
	{
		std::array<InputElementDesc, kMaxNumInputLayoutElements> elements;
		uint16_t num_elements = 0;
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
		uint32_t forced_sample_count = 0;
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
		bool depth_write = false;
		bool stencil_enable = false;
		ComparationFunction depth_func = ComparationFunction::Less_Equal;

		Topology primitive_topology;

		uint8_t num_render_targets = 0;
		std::array<Format, kMaxNumRenderTargets> render_target_format;
		Format depth_stencil_format;
		uint8_t sample_count = 1;
	};

	struct ComputePipelineStateDesc
	{
		WeakRootSignatureHandle root_signature;

		ShaderDesc compute_shader;
	};

	struct Viewport
	{
		float top_left_x = 0.f;
		float top_left_y = 0.f;
		float width;
		float height;
		float min_depth = 0.f;
		float max_depth = 1.f;
		
		Viewport()
		{
		}
		Viewport(float _width, float _height) : width(_width), height(_height)
		{
		}
	};

	struct Rect
	{
		uint32_t left;
		uint32_t top;
		uint32_t right;
		uint32_t bottom;
		
		Rect()
		{
		}

		Rect(uint32_t _left, uint32_t _top, uint32_t _right, uint32_t _bottom) :
			left(_left), top(_top), right(_right), bottom(_bottom)
		{
		}
	};

	struct VertexBufferDesc
	{
		Access access = Access::Static;
		size_t size = 0;
		const void* init_data = nullptr;
		uint32_t stride = 0;
	};

	struct IndexBufferDesc
	{
		Access access = Access::Static;
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
		uint32_t element_size = 0;
		uint32_t element_count = 0;
	};

	struct ShaderResourceDesc
	{
		Access access = Access::Static;
		Format format = Format::R8G8B8A8_UNORM;
		ShaderResourceType type = ShaderResourceType::Texture2D;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t pitch = 0;
		uint32_t slice_pitch = 0;
		uint16_t mips = 0;
		size_t size = 0;
		uint32_t num_elements = 0; //Used for buffers
		uint32_t structure_stride = 0; //Used for buffers
		const void* init_data = nullptr;
	};

	struct RenderTargetDesc
	{
		Format format;
		RenderTargetType render_target_type = RenderTargetType::Texture2D;
		uint32_t width = 0;
		uint32_t height = 0;
	};

	struct DepthBufferDesc
	{
		uint32_t width = 0;
		uint32_t height = 0;
	};

	struct WeakUnorderedAccessBufferHandleAsShaderResource : WeakUnorderedAccessBufferHandle
	{
		WeakUnorderedAccessBufferHandleAsShaderResource(WeakUnorderedAccessBufferHandle& in) : WeakUnorderedAccessBufferHandle(in)
		{
		}
	};

	struct DescriptorTableDesc
	{
		struct NullDescriptor {}; //Used to reserve a slot, but not setting a handle
		using Descritor = std::variant<NullDescriptor, WeakConstantBufferHandle, WeakUnorderedAccessBufferHandle, WeakUnorderedAccessBufferHandleAsShaderResource, WeakShaderResourceHandle, WeakRenderTargetHandle>;
		Access access = Access::Static; //With static, only static handles are supported
		static constexpr size_t kNumMaxDescriptors = 32;

		std::array<Descritor, kNumMaxDescriptors> descriptors;
		size_t num_descriptors = 0;

		void AddDescriptor(const Descritor& descriptor)
		{
			descriptors[num_descriptors++] = descriptor;
		}
	};

	struct SamplerDescriptorTableDesc
	{
		static constexpr size_t kNumMaxDescriptors = 32;
		std::array<SamplerDesc, kNumMaxDescriptors> descriptors;
		size_t num_descriptors = 0;

		void AddDescriptor(const SamplerDesc& sampler)
		{
			descriptors[num_descriptors] = sampler;
			num_descriptors++;
		}
	};

	struct DrawDesc
	{
		uint32_t start_vertex = 0;
		uint32_t vertex_count = 0;
		PrimitiveTopology primitive_topology = PrimitiveTopology::TriangleList;
	};

	struct DrawIndexedDesc
	{
		uint32_t start_index = 0;
		uint32_t index_count = 0;
		uint32_t base_vertex = 0;
		PrimitiveTopology primitive_topology = PrimitiveTopology::TriangleList;
	};

	struct DrawIndexedInstancedDesc
	{
		uint32_t instance_count = 0;
		uint32_t start_instance = 0;
		uint32_t start_index = 0;
		uint32_t index_count = 0;
		uint32_t base_vertex = 0;
		PrimitiveTopology primitive_topology = PrimitiveTopology::TriangleList;
	};

	struct CompileShaderDesc
	{
		const char* code = nullptr;
		const char* entry_point = nullptr;
		const char* target = nullptr;
		const char* name = nullptr;
		std::vector < std::pair<const char*, const char*>> defines;
	};

	struct ExecuteComputeDesc
	{
		uint32_t group_count_x = 1;
		uint32_t group_count_y = 1;
		uint32_t group_count_z = 1;
	};

	struct SetShaderResourceAsVertexBufferDesc
	{
		uint32_t stride = 0;
		uint32_t size = 0;
	};

}
#endif

