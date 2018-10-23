//////////////////////////////////////////////////////////////////////////
// Cute engine - Display layer
//////////////////////////////////////////////////////////////////////////
#ifndef DISPLAY_DESC_H_
#define DISPLAY_DESC_H_

#include "display_enum.h"

#include <cstdint>

namespace display
{
	struct InputElementDesc
	{
		const char* semantic_name;
		uint32_t semantic_index;
		Format format;
		uint32_t input_slot;
		size_t aligned_offset;
		InputType input_type = InputType::Vertex;
		uint32_t instance_step_rate = 0;

		InputElementDesc(const char* _semantic_name, uint32_t _semantic_index, Format _format, uint32_t _input_slot, size_t _aligned_offset) :
			semantic_name(_semantic_name), semantic_index(_semantic_index), format(_format), input_slot(_input_slot), aligned_offset(_aligned_offset)
		{
		}
	};

	struct InputLayoutDesc
	{
		std::vector<InputElementDesc> elements;
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
		RenderTargetBlendDesc render_target_blend[8];
	};

	struct ShaderDesc
	{
		void* data = nullptr;
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
		Format render_target_format[8];

		uint8_t sample_count = 1;
	};
}
#endif

