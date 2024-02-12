#include "render_module_debug_primitives.h"

namespace render
{
	DebugPrimitivesRenderModule::DebugPrimitivesRenderModule(GPUMemoryRenderModule* gpu_memory_render_module)
	{
		m_gpu_memory_render_module = gpu_memory_render_module;
		m_gpu_memory_segment_size = m_gpu_memory_render_module->GetDynamicSegmentSize();
		assert(m_gpu_memory_segment_size % sizeof(GPULine) == 0);
	}

	void DebugPrimitivesRenderModule::Init(display::Device* device, System* system)
	{
		m_device = device;
		m_render_system = system;

		//Register pass
		render::RegisterPassFactory<RenderDebugPrimitivesPass>(system);

		//Create root signature
		{
			//Create compute root signature
			display::RootSignatureDesc root_signature_desc;
			root_signature_desc.num_root_parameters = 3;
			root_signature_desc.root_parameters[0].type = display::RootSignatureParameterType::Constants;
			root_signature_desc.root_parameters[0].root_param.num_constants = 1;
			root_signature_desc.root_parameters[0].root_param.shader_register = 0;
			root_signature_desc.root_parameters[0].visibility = display::ShaderVisibility::Vertex;

			root_signature_desc.root_parameters[1].type = display::RootSignatureParameterType::ConstantBuffer;
			root_signature_desc.root_parameters[1].root_param.shader_register = 1;
			root_signature_desc.root_parameters[1].visibility = display::ShaderVisibility::Vertex;

			root_signature_desc.root_parameters[2].type = display::RootSignatureParameterType::ShaderResource;
			root_signature_desc.root_parameters[2].root_param.shader_register = 0;
			root_signature_desc.root_parameters[2].visibility = display::ShaderVisibility::Vertex;

			root_signature_desc.num_static_samplers = 0;

			//Create the root signature
			m_root_signature = display::CreateRootSignature(device, root_signature_desc, "Debug Primitives");
		}

		{
			//Draw primitives shaders
			const char* shader_code =
				"uint data_offset : register(b0); \
				ConstantBuffer<float4x4> camera : register(b1);	\
				ByteAddressBuffer dynamic_gpu_memory: register(t0);\
				\
				struct PSInput\
				{\
					float4 view_position : SV_POSITION;\
					float4 colour : TEXCOORD0;\
				};\
				\
				PSInput vs_line(uint vertex_id : SV_VertexID)\
				{\
					uint4 line_data = dynamic_gpu_memory.Load4(data_offset + vertex_id * 16);\
					PSInput ret;\
					ret.view_position = mul(camera, float4(asfloat(line_data.x), asfloat(line_data.y), asfloat(line_data.z), 1.f));\
					ret.colour = float4(((line_data.w >> 24) & 0xFF) / 255.f, ((line_data.w >> 16) & 0xFF) / 255.f), ((line_data.w >> 8) & 0xFF) / 255.f, ((line_data.w >> 0) & 0xFF) / 255.f);\
				}\
				\
				float4 ps_box_main(PSInput input) : SV_TARGET\
				{\
					return input.colour;\
				}";
					

			//Create pipeline state
			display::PipelineStateDesc pipeline_state_desc;
			pipeline_state_desc.root_signature = m_root_signature;
			pipeline_state_desc.vertex_shader.shader_code = shader_code;
			pipeline_state_desc.vertex_shader.entry_point = "vs_line";
			pipeline_state_desc.pixel_shader.shader_code = shader_code;
			pipeline_state_desc.pixel_shader.entry_point = "ps_line";
			pipeline_state_desc.depth_enable = false;
			pipeline_state_desc.num_render_targets = 1;
			pipeline_state_desc.render_target_format[0] = display::Format::R8G8B8A8_UNORM;
			pipeline_state_desc.antialiasing_lines = true;
			pipeline_state_desc.primitive_topology_type = display::PrimitiveTopologyType::Line;

			m_pipeline_state = display::CreatePipelineState(device, pipeline_state_desc, "Debug Primitives");
		}
		{
			display::BufferDesc constant_buffer_desc = display::BufferDesc::CreateConstantBuffer(display::Access::Dynamic, sizeof(glm::mat4x4));

			m_constant_buffer = display::CreateBuffer(device, constant_buffer_desc, "Debug Primitives Camera");
		}

	}

	void DebugPrimitivesRenderModule::Shutdown(display::Device* device, System* system)
	{
		display::DestroyRootSignature(device, m_root_signature);
		display::DestroyPipelineState(device, m_pipeline_state);
		display::DestroyBuffer(device, m_constant_buffer);
	}
	void DebugPrimitivesRenderModule::AddLine(const glm::vec3& a, const glm::vec3& b, uint32_t colour)
	{
		AddLine(a, b, colour, colour);
	}
	void DebugPrimitivesRenderModule::AddLine(const glm::vec3& a, const glm::vec3& b, uint32_t colour_a, uint32_t colour_b)
	{
		//Check if there is space
		auto& debug_primitives = m_debug_primitives.Get();

		if (debug_primitives.segment_vector.empty() || debug_primitives.last_segment_line_index == m_gpu_memory_segment_size / sizeof(GPULine))
		{
			//Needs a new segment
			debug_primitives.segment_vector.push_back(reinterpret_cast<GPULine*>(m_gpu_memory_render_module->AllocDynamicSegmentGPUMemory(m_device, render::GetRenderFrameIndex(m_render_system))));
			debug_primitives.last_segment_line_index = 0;
		}

		//Add line (do not read)
		GPULine& line = debug_primitives.segment_vector.back()[debug_primitives.last_segment_line_index];

		line.a = a;
		line.b = b;
		line.colour_a = colour_a;
		line.colour_b = colour_b;

		debug_primitives.last_segment_line_index++;
	}

	void DebugPrimitivesRenderModule::SetViewProjectionMatrix(const glm::mat4x4& view_projection_matrix)
	{
		m_view_projection_matrix[render::GetGameFrameIndex(m_render_system) % 2] = view_projection_matrix;
	}

	void DebugPrimitivesRenderModule::Render(display::Device* device, render::System* render_system, display::Context* context)
	{
		context->SetRootSignature(display::Pipe::Graphics, m_root_signature);
		context->SetPipelineState(m_pipeline_state);

		
		//Update constant buffer
		display::UpdateResourceBuffer(device, m_constant_buffer, &m_view_projection_matrix[render::GetRenderFrameIndex(m_render_system) % 2], sizeof(glm::mat4x4));
		
		context->SetConstantBuffer(display::Pipe::Graphics, 1, m_constant_buffer);
		context->SetShaderResource(display::Pipe::Graphics, 2, m_gpu_memory_render_module->GetDynamicGPUMemoryResource());

		//Generate a draw call for each segment filled in each worker thread
		m_debug_primitives.Visit([&](DebugPrimitives& debug_primitives)
			{
				for (auto& segment : debug_primitives.segment_vector)
				{
					uint32_t num_lines = static_cast<uint32_t>((segment == debug_primitives.segment_vector.back()) ? debug_primitives.last_segment_line_index : m_gpu_memory_segment_size / sizeof(GPULine));

					//Setup the offset
					uint32_t data_offset = static_cast<uint32_t>(m_gpu_memory_render_module->GetDynamicGPUMemoryOffset(device, segment));
					context->SetConstants(display::Pipe::Graphics, 0, &data_offset, 1);

					//Add draw primitive
					display::DrawDesc draw_desc;
					draw_desc.primitive_topology = display::PrimitiveTopology::LineList;
					draw_desc.start_vertex = 0;
					draw_desc.vertex_count = num_lines * 2;
					context->Draw(draw_desc);
				}

				debug_primitives.segment_vector.clear();
				debug_primitives.last_segment_line_index = 0;
			}
		);
	}

	void RenderDebugPrimitivesPass::Render(RenderContext& render_context) const
	{
		if (m_debug_primitives_render_module == nullptr)
		{
			m_debug_primitives_render_module = render::GetModule<DebugPrimitivesRenderModule>(render_context.GetRenderSystem(), "DebugPrimitives"_sh32);
		}

		m_debug_primitives_render_module->Render(render_context.GetDevice(), render_context.GetRenderSystem(), render_context.GetContext());
	}
}