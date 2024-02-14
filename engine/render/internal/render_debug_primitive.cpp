#include <render/render_debug_primitives.h>
#include <render/render.h>
#include <display/display_handle.h>
#include <ext/glm/glm.hpp>
#include <job/job_helper.h>
#include <render_module/render_module_gpu_memory.h>
#include <render/render_debug_primitives.h>

namespace render
{
	namespace debug_primitives
	{
		struct GPULine
		{
			glm::vec3 a; //4 * 3 bytes
			uint32_t colour_a; //4 bytes
			glm::vec3 b; //4 * 3 bytes
			uint32_t colour_b; //4 bytes
		};

		struct Renderer : public Module
		{
			//Debug primitives associated to this worker thread
			struct DebugPrimitives
			{
				//Vector of segments with debug primitives
				std::vector<GPULine*> segment_vector;

				//Last segment line index
				size_t last_segment_line_index = 0;
			};

			//Thread local storage with the collect debug primitives
			job::ThreadData<DebugPrimitives> m_debug_primitives[2];

			//View projection matrix
			glm::mat4x4 m_view_projection_matrix[2];

			GPUMemoryRenderModule* m_gpu_memory_render_module;
			size_t m_gpu_memory_segment_size;
			display::Device* m_device = nullptr;
			render::System* m_render_system = nullptr;

			display::RootSignatureHandle m_root_signature;
			display::PipelineStateHandle m_pipeline_state;
			display::BufferHandle m_constant_buffer;

			void Render(display::Device* device, render::System* render_system, display::Context* context);
			void DrawLine(const glm::vec3& a, const glm::vec3& b, const uint32_t colour_a, const uint32_t colour_b);


			//Module interface
			static ModuleName GetModuleName()
			{
				return "DebugPrimitives"_sh32;
			}
			void Shutdown(display::Device* device, System* system) override;
			void EndFrame(display::Device* device, System* system);
		};

		Renderer* g_renderer = nullptr;

		class RenderDebugPrimitivesPass : public Pass
		{
		public:
			inline static Renderer* m_renderer = nullptr;

			DECLARE_RENDER_CLASS("RenderDebugPrimitives");

			void Render(RenderContext& render_context) const override;
		};

		void Init(display::Device* device, System* system, GPUMemoryRenderModule* gpu_memory_render_module)
		{
			assert(g_renderer == nullptr);

			g_renderer = RegisterModule<Renderer>(system);

			g_renderer->m_device = device;
			g_renderer->m_render_system = system;
			g_renderer->m_gpu_memory_render_module = gpu_memory_render_module;

			RenderDebugPrimitivesPass::m_renderer = g_renderer;

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
				g_renderer->m_root_signature = display::CreateRootSignature(device, root_signature_desc, "Debug Primitives");
			}

			{
				//Draw primitives shaders
				const char* shader_code =
					"uint data_offset : register(b0); \n\
					struct Camera\n\
					{\n\
						float4x4 view_projection_matrix;\n\
					};\n\
					ConstantBuffer<Camera> camera : register(b1);	\n\
					ByteAddressBuffer dynamic_gpu_memory: register(t0);\n\
					\n\
					struct PSInput\n\
					{\n\
						float4 view_position : SV_POSITION;\n\
						float4 colour : TEXCOORD0;\n\
					};\n\
					\n\
					PSInput vs_line(uint vertex_id : SV_VertexID)\n\
					{\n\
						uint4 line_data = dynamic_gpu_memory.Load4(data_offset + vertex_id * 16);\n\
						PSInput ret;\n\
						ret.view_position = mul(camera.view_projection_matrix, float4(asfloat(line_data.x), asfloat(line_data.y), asfloat(line_data.z), 1.f));\n\
						ret.colour = float4(((line_data.w >> 0) & 0xFF) / 255.f, ((line_data.w >> 8) & 0xFF) / 255.f, ((line_data.w >> 16) & 0xFF) / 255.f, ((line_data.w >> 24) & 0xFF) / 255.f);\n\
						return ret; \n\
					}\n\
					\n\
					float4 ps_line(PSInput input) : SV_TARGET\n\
					{\n\
						return input.colour;\n\
					}";


				//Create pipeline state
				display::PipelineStateDesc pipeline_state_desc;
				pipeline_state_desc.root_signature = g_renderer->m_root_signature;
				pipeline_state_desc.vertex_shader.shader_code = shader_code;
				pipeline_state_desc.vertex_shader.entry_point = "vs_line";
				pipeline_state_desc.vertex_shader.name = "debug primitives line vs";
				pipeline_state_desc.vertex_shader.target = "vs_6_6";
				pipeline_state_desc.pixel_shader.shader_code = shader_code;
				pipeline_state_desc.pixel_shader.entry_point = "ps_line";
				pipeline_state_desc.pixel_shader.name = "debug primitives line ps";
				pipeline_state_desc.pixel_shader.target = "ps_6_6";
				pipeline_state_desc.depth_enable = false;
				pipeline_state_desc.num_render_targets = 1;
				pipeline_state_desc.render_target_format[0] = display::Format::R8G8B8A8_UNORM;
				pipeline_state_desc.antialiasing_lines = true;
				pipeline_state_desc.primitive_topology_type = display::PrimitiveTopologyType::Line;

				g_renderer->m_pipeline_state = display::CreatePipelineState(device, pipeline_state_desc, "Debug Primitives");
			}
			{
				display::BufferDesc constant_buffer_desc = display::BufferDesc::CreateConstantBuffer(display::Access::Dynamic, sizeof(glm::mat4x4));

				g_renderer->m_constant_buffer = display::CreateBuffer(device, constant_buffer_desc, "Debug Primitives Camera");
			}
		}

		void SetViewProjectionMatrix(const glm::mat4x4& view_projection_matrix)
		{
			g_renderer->m_view_projection_matrix[render::GetGameFrameIndex(g_renderer->m_render_system) % 2] = view_projection_matrix;
		}

		void Renderer::Shutdown(display::Device* device, System* system)
		{
			display::DestroyRootSignature(device, m_root_signature);
			display::DestroyPipelineState(device, m_pipeline_state);
			display::DestroyBuffer(device, m_constant_buffer);

			assert(g_renderer);
			g_renderer = nullptr;
		};
		void Renderer::EndFrame(display::Device* device, System* system)
		{
			//Clean the frame
			m_debug_primitives[render::GetRenderFrameIndex(system) % 2].Visit([&](DebugPrimitives& debug_primitives)
				{
					debug_primitives.last_segment_line_index = 0;
					debug_primitives.segment_vector.clear();
				});
		}

		void Renderer::DrawLine(const glm::vec3& a, const glm::vec3& b, const uint32_t colour_a, const uint32_t colour_b)
		{
			//Check if it is inited
			if (m_device == nullptr) return;

			//Check if there is space
			auto& debug_primitives = g_renderer->m_debug_primitives[render::GetGameFrameIndex(m_render_system) % 2].Get();

			if (debug_primitives.segment_vector.empty() || debug_primitives.last_segment_line_index == g_renderer->m_gpu_memory_segment_size / sizeof(GPULine))
			{
				//Needs a new segment
				debug_primitives.segment_vector.push_back(reinterpret_cast<GPULine*>(g_renderer->m_gpu_memory_render_module->AllocDynamicSegmentGPUMemory(g_renderer->m_device, render::GetGameFrameIndex(g_renderer->m_render_system))));
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

		void Renderer::Render(display::Device* device, render::System* render_system, display::Context* context)
		{
			context->SetRootSignature(display::Pipe::Graphics, m_root_signature);
			context->SetPipelineState(m_pipeline_state);


			//Update constant buffer
			display::UpdateResourceBuffer(device, m_constant_buffer, &m_view_projection_matrix[render::GetRenderFrameIndex(m_render_system) % 2], sizeof(glm::mat4x4));

			context->SetConstantBuffer(display::Pipe::Graphics, 1, m_constant_buffer);
			context->SetShaderResource(display::Pipe::Graphics, 2, m_gpu_memory_render_module->GetDynamicGPUMemoryResource());

			//Generate a draw call for each segment filled in each worker thread
			m_debug_primitives[render::GetRenderFrameIndex(render_system) % 2].Visit([&](DebugPrimitives& debug_primitives)
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
			m_renderer->Render(render_context.GetDevice(), render_context.GetRenderSystem(), render_context.GetContext());
		}


		//Debug primitives
		void DrawLine(const glm::vec3& position_a, const glm::vec3& position_b, const Colour& colour)
		{
			g_renderer->DrawLine(position_a, position_b, colour.value, colour.value);
		}

		void DrawStar(const glm::vec3& position, const float size, const Colour& colour)
		{
			DrawLine(position - glm::vec3(1.f, 0.f, 0.f) * size, position + glm::vec3(1.f, 0.f, 0.f) * size, colour);
			DrawLine(position - glm::vec3(0.f, 1.f, 0.f) * size, position + glm::vec3(0.f, 1.f, 0.f) * size, colour);
			DrawLine(position - glm::vec3(0.f, 0.f, 1.f) * size, position + glm::vec3(0.f, 0.f, 1.f) * size, colour);
		}
	}
}