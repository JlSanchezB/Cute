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
		char* name;
		uint32_t semantic_index;
		Format format;
		uint32_t input_slot;
		size_t aligned_offset;
		InputType input_type;
		uint32_t instance_step_rate;
	};

	struct InputLayoutDesc
	{
		InputElementDesc elements;
		size_t size;
	};

	struct RasterizationDesc
	{
		FillMode fill_mode;
		CullMode cull_mode;
		float depth_bias;
		float depth_bias_clamp;
		float slope_depth_bias;
		bool multisample_enable;
		bool convervative_mode;
	};

	struct RenderTargetBlendDesc
	{
		bool blend_enable;
		Blend src_blend;
		Blend dest_blend;
		BlendOp blend_op;
		Blend alpha_src_blend;
		Blend alpha_dest_blend;
		BlendOp alpha_blend_op;
		uint8_t write_mask;
	};

	struct BlendDesc
	{
		bool alpha_to_coverage_enable;
		bool independent_blend_enable;
		RenderTargetBlendDesc render_target_blend[8];
	};

	struct PipelineStateDesc
	{
		WeakRootSignatureHandle root_signature;
		
		WeakShaderHandle vertex_shader;
		WeakShaderHandle pixel_shader;
		WeakShaderHandle domain_shader;
		WeakShaderHandle hull_shader;
		WeakShaderHandle geometry_shader;
		
		InputLayoutDesc input_layout;
		RasterizationDesc rasteritation_state;

		bool depth_enable;
		bool stencil_enable;

		Topology primitive_topology;

		uint8_t num_render_targets;
		Format render_target_format[8];
	};
}
#endif

