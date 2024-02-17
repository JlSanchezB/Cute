#include <render/render_debug_primitives.h>
#include <render/render.h>
#include <display/display_handle.h>
#include <ext/glm/glm.hpp>
#include <job/job_helper.h>
#include <render/render_debug_primitives.h>
#include <core/platform.h>

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

			GPULine(const glm::vec3& _a, uint32_t _colour_a, const glm::vec3& _b, uint32_t _colour_b) :
				a(_a), colour_a(_colour_a), b(_b), colour_b(_colour_b) {};
		};

		enum class FrameSlot
		{
			Game,
			Render
		};

		struct Renderer
		{
			struct DebugPrimitivesFrame
			{
				std::vector<GPULine> update_debug_primitives;
				std::vector<GPULine> render_debug_primitives;
			};

			//Thread local storage with the collect debug primitives
			job::ThreadData<DebugPrimitivesFrame> m_debug_primitives[2];

			//View projection matrix
			glm::mat4x4 m_view_projection_matrix[2];

			size_t m_gpu_memory_segment_size;
			display::Device* m_device = nullptr;
			render::System* m_render_system = nullptr;

			display::RootSignatureHandle m_root_signature;
			display::PipelineStateHandle m_pipeline_state;
			display::BufferHandle m_constant_buffer;

			display::BufferHandle m_line_buffer;
			uint32_t m_line_buffer_size = 4 * 1024;

			//current frame slot
			FrameSlot m_frame_slot = FrameSlot::Game;

			void Render(display::Device* device, render::System* render_system, display::Context* context);
			void DrawLine(const glm::vec3& a, const glm::vec3& b, const uint32_t colour_a, const uint32_t colour_b);


			void Init(display::Device* device, System* system);
			void Shutdown();
			void ResetGameFrame()
			{
				m_frame_slot = FrameSlot::Game;

				//Clear debug primitives from update
				m_debug_primitives[render::GetGameFrameIndex(m_render_system) % 2].Visit([&](DebugPrimitivesFrame& debug_primitives)
					{
						debug_primitives.update_debug_primitives.clear();
					});
			}
			void ResetRenderFrame()
			{
				m_frame_slot = FrameSlot::Render;

				//Clear debug primitives from render
				m_debug_primitives[render::GetGameFrameIndex(m_render_system) % 2].Visit([&](DebugPrimitivesFrame& debug_primitives)
					{
						debug_primitives.render_debug_primitives.clear();
					});
			}

		};

		Renderer* g_renderer = nullptr;

		//Platform module to comunicate with the platform
		struct DebugPrimitivesModule : platform::Module
		{
			DebugPrimitivesModule()
			{
				//During construction register as a module
				platform::RegisterModule(this);
			}

			//Callbacks
			virtual void OnInit(display::Device* device, render::System* render_system)
			{
				if (device && render_system)
				{

					//It can be inited
					assert(g_renderer == nullptr);
					g_renderer = new Renderer;

					g_renderer->Init(device, render_system);
					return;
				}

				core::LogInfo("DrawPrimitives renderer can not start with the current configuration");
			};
			virtual void OnDestroy() override
			{
				if (g_renderer)
				{
					g_renderer->Shutdown();

					delete g_renderer;
					g_renderer = nullptr;
				}
			};
				
			virtual void OnResetFrame()
			{
				if (g_renderer)
				{
					g_renderer->ResetGameFrame();
				}
			};
			virtual void OnRender(double total_time, float elapsed_time)
			{
				if (g_renderer)
				{
					g_renderer->ResetRenderFrame();
				}
			};
		};

		//It will register the platform module and all the callbacks
		DebugPrimitivesModule g_platform_module;

		void Renderer::Init(display::Device* device, render::System* system)
		{
			m_device = device;
			m_render_system = system;

			//Create root signature
			{
				//Create compute root signature
				display::RootSignatureDesc root_signature_desc;
				root_signature_desc.num_root_parameters = 2;

				root_signature_desc.root_parameters[0].type = display::RootSignatureParameterType::ConstantBuffer;
				root_signature_desc.root_parameters[0].root_param.shader_register = 1;
				root_signature_desc.root_parameters[0].visibility = display::ShaderVisibility::Vertex;

				root_signature_desc.root_parameters[1].type = display::RootSignatureParameterType::ShaderResource;
				root_signature_desc.root_parameters[1].root_param.shader_register = 0;
				root_signature_desc.root_parameters[1].visibility = display::ShaderVisibility::Vertex;

				root_signature_desc.num_static_samplers = 0;

				//Create the root signature
				m_root_signature = display::CreateRootSignature(device, root_signature_desc, "Debug Primitives");
			}

			{
				//Draw primitives shaders
				const char* shader_code = "\
					struct Camera\n\
					{\n\
						float4x4 view_projection_matrix;\n\
					};\n\
					ConstantBuffer<Camera> camera : register(b1);	\n\
					struct GPULine {float3 a; uint colour_a; float3 b; uint colour_b;};\n\
					StructuredBuffer<GPULine> line_buffer: register(t0);\n\
					\n\
					struct PSInput\n\
					{\n\
						float4 view_position : SV_POSITION;\n\
						float4 colour : TEXCOORD0;\n\
					};\n\
					\n\
					PSInput vs_line(uint vertex_id : SV_VertexID)\n\
					{\n\
						GPULine debug_line = line_buffer[vertex_id / 2];\n\
						float3 position = (vertex_id % 2 == 0) ? debug_line.a : debug_line.b;\n\
						uint colour = (vertex_id % 2 == 0) ? debug_line.colour_a : debug_line.colour_b;\n\
						PSInput ret;\n\
						ret.view_position = mul(camera.view_projection_matrix, float4(position, 1.f));\n\
						ret.colour = float4(((colour >> 0) & 0xFF) / 255.f, ((colour >> 8) & 0xFF) / 255.f, ((colour >> 16) & 0xFF) / 255.f, ((colour >> 24) & 0xFF) / 255.f);\n\
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

				m_pipeline_state = display::CreatePipelineState(device, pipeline_state_desc, "Debug Primitives");
			}
			{
				display::BufferDesc constant_buffer_desc = display::BufferDesc::CreateConstantBuffer(display::Access::Dynamic, sizeof(glm::mat4x4));

				m_constant_buffer = display::CreateBuffer(device, constant_buffer_desc, "Debug Primitives Camera");
			}
		}

		void Renderer::Shutdown()
		{
			display::DestroyRootSignature(m_device, m_root_signature);
			display::DestroyPipelineState(m_device, m_pipeline_state);
			display::DestroyBuffer(m_device, m_constant_buffer);
			if (m_line_buffer.IsValid())
			{
				display::DestroyBuffer(m_device, m_line_buffer);
			}
		};

		void Renderer::DrawLine(const glm::vec3& a, const glm::vec3& b, const uint32_t colour_a, const uint32_t colour_b)
		{
			//Check if it is inited
			if (m_device == nullptr) return;

			//Check if there is space
			auto& debug_primitives_frame = g_renderer->m_debug_primitives[render::GetGameFrameIndex(m_render_system) % 2].Get();

			if (m_frame_slot == FrameSlot::Game)
			{
				debug_primitives_frame.update_debug_primitives.emplace_back(a, colour_a, b, colour_b);
			}
			else
			{
				debug_primitives_frame.render_debug_primitives.emplace_back(a, colour_a, b, colour_b);
			}
		}

		void Renderer::Render(display::Device* device, render::System* render_system, display::Context* context)
		{
			uint32_t num_lines = 0;
			//Calculate the size of the render
			auto& debug_primitives_frame = g_renderer->m_debug_primitives[render::GetRenderFrameIndex(m_render_system) % 2];

			debug_primitives_frame.Visit([&](DebugPrimitivesFrame& debug_primitives)
				{
					num_lines += static_cast<uint32_t>(debug_primitives.update_debug_primitives.size());
					num_lines += static_cast<uint32_t>(debug_primitives.render_debug_primitives.size());
				});

			if (num_lines == 0)
			{
				//Nothing to do
				return;
			}

			//Check if I need to resize my buffer
			if (m_line_buffer_size < (num_lines * 2) || !m_line_buffer.IsValid())
			{
				//Resize to twice the number of lines
				display::DestroyBuffer(m_device, m_line_buffer);
				m_line_buffer = display::CreateBuffer(m_device, display::BufferDesc::CreateStructuredBuffer(display::Access::Dynamic, (num_lines * 2), sizeof(GPULine)), "Debug Primitives Line Buffer");

				m_line_buffer_size = (num_lines * 2);
			}

			//Update the buffer with the lines
			GPULine* dest_buffer = reinterpret_cast<GPULine*>(display::GetResourceMemoryBuffer(m_device, m_line_buffer));

			uint32_t upload_lines = 0;
			debug_primitives_frame.Visit([&](DebugPrimitivesFrame& debug_primitives)
				{
					if (debug_primitives.update_debug_primitives.size() > 0)
					{
						memcpy(dest_buffer + upload_lines, debug_primitives.update_debug_primitives.data(), debug_primitives.update_debug_primitives.size() * sizeof(GPULine));
						upload_lines += static_cast<uint32_t>(debug_primitives.update_debug_primitives.size());
					}
					if (debug_primitives.render_debug_primitives.size() > 0)
					{
						memcpy(dest_buffer + upload_lines, debug_primitives.render_debug_primitives.data(), debug_primitives.render_debug_primitives.size() * sizeof(GPULine));
						upload_lines += static_cast<uint32_t>(debug_primitives.update_debug_primitives.size());
					}
				});
			assert(upload_lines == num_lines);

			context->SetRootSignature(display::Pipe::Graphics, m_root_signature);
			context->SetPipelineState(m_pipeline_state);

			//Update constant buffer
			display::UpdateResourceBuffer(device, m_constant_buffer, &m_view_projection_matrix[render::GetRenderFrameIndex(m_render_system) % 2], sizeof(glm::mat4x4));

			context->SetConstantBuffer(display::Pipe::Graphics, 0, m_constant_buffer);
			context->SetShaderResource(display::Pipe::Graphics, 1, m_line_buffer);

			//Add draw primitive
			display::DrawDesc draw_desc;
			draw_desc.primitive_topology = display::PrimitiveTopology::LineList;
			draw_desc.start_vertex = 0;
			draw_desc.vertex_count = num_lines * 2;
			context->Draw(draw_desc);
		}

		//Capture the view projection matrix
		void SetViewProjectionMatrix(const glm::mat4x4& view_projection_matrix)
		{
			if (g_renderer)
			{
				g_renderer->m_view_projection_matrix[render::GetGameFrameIndex(g_renderer->m_render_system) % 2] = view_projection_matrix;
			}
		}

		//Debug primitives
		void DrawLine(const glm::vec3& position_a, const glm::vec3& position_b, const Colour& colour)
		{
			if (g_renderer)
			{
				g_renderer->DrawLine(position_a, position_b, colour.value, colour.value);
			}
		}

		void DrawStar(const glm::vec3& position, const float size, const Colour& colour)
		{
			if (g_renderer)
			{
				DrawLine(position - glm::vec3(1.f, 0.f, 0.f) * size, position + glm::vec3(1.f, 0.f, 0.f) * size, colour);
				DrawLine(position - glm::vec3(0.f, 1.f, 0.f) * size, position + glm::vec3(0.f, 1.f, 0.f) * size, colour);
				DrawLine(position - glm::vec3(0.f, 0.f, 1.f) * size, position + glm::vec3(0.f, 0.f, 1.f) * size, colour);
			}
		}
	}

	void RenderDebugPrimitivesPass::Render(RenderContext& render_context) const
	{
		if (debug_primitives::g_renderer)
		{
			debug_primitives::g_renderer->Render(render_context.GetDevice(), render_context.GetRenderSystem(), render_context.GetContext());
		}
	}
}