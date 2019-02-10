#include <core/platform.h>
#include <display/display.h>
#include <render/render.h>
#include <render/render_resource.h>
#include <render/render_helper.h>
#include <ecs/entity_component_system.h>
#include <ext/glm/vec4.hpp>
#include <ext/glm/vec2.hpp>
#include <ext/glm/gtc/constants.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif
#include <windows.h>
#include <fstream>
#include <random>
#include <bitset>

struct DisplayResource
{
	static constexpr uint32_t kNumCircleVertex = 16;
	static constexpr uint32_t kNumCircleIndex = 3 * (kNumCircleVertex - 2);

	display::VertexBufferHandle m_circle_vertex_buffer;
	display::IndexBufferHandle m_circle_index_buffer;
	display::RootSignatureHandle m_root_signature;
	display::PipelineStateHandle m_pipeline_state;

	void Load(display::Device* device)
	{
		//Create root signature
		display::RootSignatureDesc root_signature_desc;
		m_root_signature = display::CreateRootSignature(device, root_signature_desc, "Root Signature");

		//Create pipeline state
		const char* shader_code =
			"	struct PSInput\
				{\
					float4 position : SV_POSITION;\
				};\
				\
				PSInput main_vs(float2 position : POSITION, float4 instance_data : TEXCOORD)\
				{\
					PSInput result; \
					result.position.xy = position.xy * instance_data.z + instance_data.xy; \
					result.position.zw = float2(0.f, 1.f); \
					return result;\
				}\
				float4 main_ps(PSInput input) : SV_TARGET\
				{\
					return float4(1.f, 0.f, 0.f, 1.f);\
				}";

		std::vector<char> vertex_shader;
		std::vector<char> pixel_shader;

		display::CompileShaderDesc compile_shader_desc;
		compile_shader_desc.code = shader_code;
		compile_shader_desc.entry_point = "main_vs";
		compile_shader_desc.target = "vs_5_0";
		display::CompileShader(device, compile_shader_desc, vertex_shader);

		compile_shader_desc.code = shader_code;
		compile_shader_desc.entry_point = "main_ps";
		compile_shader_desc.target = "ps_5_0";
		display::CompileShader(device, compile_shader_desc, pixel_shader);


		//Create pipeline state
		display::PipelineStateDesc pipeline_state_desc;
		pipeline_state_desc.root_signature = m_root_signature;

		//Add input layouts
		pipeline_state_desc.input_layout.elements[0] = display::InputElementDesc("POSITION", 0, display::Format::R32G32_FLOAT, 0, 0);
		pipeline_state_desc.input_layout.elements[1] = display::InputElementDesc("TEXCOORD", 0, display::Format::R32G32B32A32_FLOAT, 1, 0, display::InputType::Instance);
		pipeline_state_desc.input_layout.num_elements = 2;


		//Add shaders
		pipeline_state_desc.pixel_shader.data = reinterpret_cast<void*>(pixel_shader.data());
		pipeline_state_desc.pixel_shader.size = pixel_shader.size();

		pipeline_state_desc.vertex_shader.data = reinterpret_cast<void*>(vertex_shader.data());
		pipeline_state_desc.vertex_shader.size = vertex_shader.size();

		//Add render targets
		pipeline_state_desc.num_render_targets = 1;
		pipeline_state_desc.render_target_format[0] = display::Format::R8G8B8A8_UNORM;

		//Create
		m_pipeline_state = display::CreatePipelineState(device, pipeline_state_desc, "Circle");



		//Create circle vertex buffer and index buffer
		glm::vec2 vertex_data[kNumCircleVertex];
		for (size_t i = 0; i < kNumCircleVertex; ++i)
		{
			const float angle = glm::two_pi<float>() * (float(i) / kNumCircleVertex);
			vertex_data[i].x = cosf(angle);
			vertex_data[i].y = cosf(angle);
		}

		display::VertexBufferDesc vertex_buffer_desc;
		vertex_buffer_desc.init_data = vertex_data;
		vertex_buffer_desc.size = sizeof(vertex_data);
		vertex_buffer_desc.stride = sizeof(glm::vec2);

		m_circle_vertex_buffer = display::CreateVertexBuffer(device, vertex_buffer_desc, "Circle");

		uint16_t index_buffer_data[kNumCircleIndex];
		for (uint16_t i = 0; i < static_cast<uint16_t>(kNumCircleVertex - 2); ++i)
		{
			index_buffer_data[i * 3] = 0;
			index_buffer_data[i * 3 + 1] = i + 1;
			index_buffer_data[i * 3 + 2] = i + 2;
		}

		display::IndexBufferDesc index_buffer_desc;
		index_buffer_desc.init_data = index_buffer_data;
		index_buffer_desc.size = sizeof(index_buffer_data);

		m_circle_index_buffer = display::CreateIndexBuffer(device, index_buffer_desc, "Circle");
	}

	void Unload(display::Device* device)
	{
		display::DestroyHandle(device, m_circle_vertex_buffer);
		display::DestroyHandle(device, m_circle_index_buffer);
		display::DestroyHandle(device, m_pipeline_state);
		display::DestroyHandle(device, m_root_signature);
	}
};


struct PositionComponent
{
	glm::vec4 position_angle;

	PositionComponent(float x, float y, float angle) : position_angle(x, y, angle, 0.f)
	{
	}
};

struct VelocityComponent
{
	glm::vec4 lineal_angle_velocity;

	VelocityComponent(float x, float y, float m) : lineal_angle_velocity(x, y, m, 0.f)
	{
	}
};


struct GrassComponent
{
	float size;

	GrassComponent(float _size) : size(_size)
	{
	}
};

struct GazelleComponent
{
	float size;

	GazelleComponent(float _size) : size(_size)
	{
	}
};

struct TigerComponent
{
	float size;

	TigerComponent(float _size) : size(_size)
	{
	}
};

using GrassEntityType = ecs::EntityType<PositionComponent, VelocityComponent, GrassComponent>;
using GazelleEntityType = ecs::EntityType<PositionComponent, VelocityComponent, GazelleComponent>;
using TigerEntityType = ecs::EntityType<PositionComponent, VelocityComponent, TigerComponent>;

using GameComponents = ecs::ComponentList<PositionComponent, VelocityComponent, GrassComponent, GazelleComponent, TigerComponent>;
using GameEntityTypes = ecs::EntityTypeList<GrassEntityType, GazelleEntityType, TigerEntityType>;

using GameDatabase = ecs::DatabaseDeclaration<GameComponents, GameEntityTypes>;
using Instance = ecs::Instance<GameDatabase>;

namespace
{
	std::vector<uint8_t> ReadFileToBuffer(const char* file)
	{
		std::ifstream root_signature_file(file, std::ios::binary | std::ios::ate);
		if (root_signature_file.good())
		{
			std::streamsize size = root_signature_file.tellg();
			root_signature_file.seekg(0, std::ios::beg);

			std::vector<uint8_t> buffer(size);
			root_signature_file.read(reinterpret_cast<char*>(buffer.data()), size);

			return buffer;
		}
		else
		{
			return std::vector<uint8_t>(0);
		}
	}
}

class ECSGame : public platform::Game
{
public:
	constexpr static uint32_t kInitWidth = 500;
	constexpr static uint32_t kInitHeight = 500;

	uint32_t m_width;
	uint32_t m_height;

	display::Device* m_device;

	render::System* m_render_system = nullptr;

	render::RenderContext* m_render_context = nullptr;

	//Game constant buffer
	display::ConstantBufferHandle m_game_constant_buffer;

	//Instances instance buffer
	display::VertexBufferHandle m_instances_vertex_buffer;

	//Command buffer
	display::CommandListHandle m_render_command_list;

	//Display resources
	DisplayResource m_display_resources;

	//Last valid descriptor file
	std::vector<uint8_t> m_render_passes_descriptor_buffer;

	//Buffer used for the render passes text editor
	std::array<char, 1024 * 128> m_text_buffer = { 0 };

	//Display imgui edit descriptor file
	bool m_show_edit_descriptor_file = false;

	//Reload render passes file from the text editor
	bool m_render_system_descriptor_load_requested = false;

	//Solid render priority
	render::Priority m_solid_render_priority;

	//Show errors in imguid modal window
	bool m_show_errors = false;
	std::vector<std::string> m_render_system_errors;
	std::vector<std::string> m_render_system_context_errors;

	//Instance buffer
	std::vector<glm::vec4> m_instance_buffer;

	void OnInit() override
	{
		display::DeviceInitParams device_init_params;

		device_init_params.debug = true;
		device_init_params.width = kInitWidth;
		device_init_params.height = kInitHeight;
		device_init_params.tearing = true;
		device_init_params.vsync = false;
		device_init_params.num_frames = 3;

		m_device = display::CreateDevice(device_init_params);

		if (m_device == nullptr)
		{
			throw std::runtime_error::exception("Error creating the display device");
		}

		SetDevice(m_device);

		//Create constant buffer with the time
		display::ConstantBufferDesc constant_buffer_desc;
		constant_buffer_desc.access = display::Access::Dynamic;
		constant_buffer_desc.size = 16;
		m_game_constant_buffer = display::CreateConstantBuffer(m_device, constant_buffer_desc, "GameConstantBuffer");

		//Create instances vertex buffer
		display::VertexBufferDesc instance_vertex_buffer_desc;
		instance_vertex_buffer_desc.access = display::Access::Dynamic;
		instance_vertex_buffer_desc.stride = 16; //4 floats
		instance_vertex_buffer_desc.size = 1024 * 1024;
		m_instances_vertex_buffer = display::CreateVertexBuffer(m_device, instance_vertex_buffer_desc, "InstanceVertexBuffer");

		m_display_resources.Load(m_device);

		//Create a command list for rendering the render frame
		m_render_command_list = display::CreateCommandList(m_device, "BeginFrameCommandList");

		//Create render pass system
		m_render_system = render::CreateRenderSystem();

		//Add game resources
		render::AddGameResource(m_render_system, "GameGlobal"_sh32, CreateResourceFromHandle<render::ConstantBufferResource>(display::WeakConstantBufferHandle(m_game_constant_buffer)));
		render::AddGameResource(m_render_system, "BackBuffer"_sh32, CreateResourceFromHandle<render::RenderTargetResource>(display::GetBackBuffer(m_device)));
		render::AddGameResource(m_render_system, "GameRootSignature"_sh32, CreateResourceFromHandle<render::RootSignatureResource>(display::WeakRootSignatureHandle(m_display_resources.m_root_signature)));


		//Get render priorities
		m_solid_render_priority = render::GetRenderItemPriority(m_render_system, "Solid"_sh32);

		//Read file
		m_render_passes_descriptor_buffer = ReadFileToBuffer("ecs_render_passes.xml");

		if (m_render_passes_descriptor_buffer.size() > 0)
		{
			//Copy to the text editor buffer
			memcpy(m_text_buffer.data(), m_render_passes_descriptor_buffer.data(), m_render_passes_descriptor_buffer.size());
		}

		m_render_system_descriptor_load_requested = true;

		//Create ecs database
		ecs::DatabaseDesc database_desc;
		ecs::CreateDatabase<GameDatabase>(database_desc);

		//Allocate random instances
		std::random_device rd;  //Will be used to obtain a seed for the random number engine
		std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
		std::uniform_real_distribution<float> rand_position_x(-1.f, 1.0f);
		std::uniform_real_distribution<float> rand_position_y(-1.f, 1.0f);
		std::uniform_real_distribution<float> rand_position_angle(0.f, 2.f * 3.1415926f);
		std::uniform_real_distribution<float> rand_lineal_velocity(-0.01f, 0.01f);
		std::uniform_real_distribution<float> rand_angle_velocity(-0.01f, 0.01f);
		std::uniform_real_distribution<float> rand_size(0.01f, 0.02f);

		for (size_t i = 0; i < 1000; ++i)
		{
			ecs::AllocInstance<GameDatabase, GazelleEntityType>()
				.Init<PositionComponent>(rand_position_x(gen), rand_position_y(gen), rand_position_angle(gen))
				.Init<VelocityComponent>(rand_lineal_velocity(gen), rand_lineal_velocity(gen), rand_angle_velocity(gen))
				.Init<GazelleComponent>(rand_size(gen));
		}

		for (size_t i = 0; i < 1000; ++i)
		{
			ecs::AllocInstance<GameDatabase, GrassEntityType>()
				.Init<PositionComponent>(rand_position_x(gen), rand_position_y(gen), rand_position_angle(gen))
				.Init<VelocityComponent>(rand_lineal_velocity(gen), rand_lineal_velocity(gen), rand_angle_velocity(gen))
				.Init<GrassComponent>(rand_size(gen));
		}

		for (size_t i = 0; i < 1000; ++i)
		{
			ecs::AllocInstance<GameDatabase, TigerEntityType>()
				.Init<PositionComponent>(rand_position_x(gen), rand_position_y(gen), rand_position_angle(gen))
				.Init<VelocityComponent>(rand_lineal_velocity(gen), rand_lineal_velocity(gen), rand_angle_velocity(gen))
				.Init<TigerComponent>(rand_size(gen));
		}
		
	}
	void OnDestroy() override
	{
		if (m_render_system)
		{
			if (m_render_context)
			{
				render::DestroyRenderContext(m_render_system, m_render_context);
			}

			render::DestroyRenderSystem(m_render_system, m_device);
		}

		//Destroy handles
		display::DestroyHandle(m_device, m_game_constant_buffer);
		display::DestroyHandle(m_device, m_instances_vertex_buffer);
		display::DestroyHandle(m_device, m_render_command_list);
		m_display_resources.Unload(m_device);

		//Destroy device
		display::DestroyDevice(m_device);
	}
	void OnTick(double total_time, float elapsed_time) override
	{
		//UPDATE GAME
		{
			std::bitset<1> zone_bitset(1);

			//Move entities
			ecs::Process<GameDatabase, PositionComponent, VelocityComponent>([&](const auto& instance_iterator, PositionComponent& position, VelocityComponent& velocity)
			{
				position.position_angle += velocity.lineal_angle_velocity * elapsed_time;
			}, zone_bitset);

		}

		//PREPARE RENDERING
		{
			std::bitset<1> zone_bitset(1);

			render::BeginPrepareRender(m_render_system);

			render::Frame& render_frame = render::GetGameRenderFrame(m_render_system);

			auto& point_of_view = render_frame.AllocPointOfView("Main"_sh32, 0);
			auto& command_buffer = point_of_view.GetCommandBuffer();

			//Culling and draw per type
			m_instance_buffer.clear();

			ecs::Process<GameDatabase, PositionComponent, GrassComponent>([&](const auto& instance_iterator, PositionComponent& position, GrassComponent& grass)
			{
				m_instance_buffer.emplace_back(position.position_angle.x, position.position_angle.y, position.position_angle.z, grass.size);
			}, zone_bitset);

			//Add a render item for rendering the grass
			auto commands_offset = command_buffer.Open();
			command_buffer.SetVertexBuffers(0, 1, &m_display_resources.m_circle_vertex_buffer);
			command_buffer.SetVertexBuffers(1, 1, &m_instances_vertex_buffer);
			command_buffer.SetIndexBuffer(m_display_resources.m_circle_index_buffer);
			command_buffer.SetPipelineState(m_display_resources.m_pipeline_state);
			display::DrawIndexedInstancedDesc draw_desc;
			draw_desc.index_count = DisplayResource::kNumCircleIndex;
			draw_desc.instance_count = static_cast<uint32_t>(m_instance_buffer.size());
			command_buffer.DrawIndexedInstanced(draw_desc);
			command_buffer.Close();

			point_of_view.PushRenderItem(m_solid_render_priority, 0, commands_offset);

			ecs::Process<GameDatabase, PositionComponent, GazelleComponent>([&](const auto& instance_iterator, PositionComponent& position, GazelleComponent& gazelle)
			{
				//Add to the instance buffer the instance
				m_instance_buffer.emplace_back(position.position_angle.x, position.position_angle.y, position.position_angle.z, gazelle.size);
			}, zone_bitset);
			//Add a render item for rendering the gazelles

		
			ecs::Process<GameDatabase, PositionComponent, TigerComponent>([&](const auto& instance_iterator, PositionComponent& position, TigerComponent& tiger)
			{
				m_instance_buffer.emplace_back(position.position_angle.x, position.position_angle.y, position.position_angle.z, tiger.size);
			}, zone_bitset);
			//Add a render item for rendering the tigers


			//Send the buffer for updating the vertex constant
			auto command_offset = render_frame.GetBeginFrameComamndbuffer().Open();
			render_frame.GetBeginFrameComamndbuffer().UploadResourceBuffer(m_instances_vertex_buffer, &m_instance_buffer[0], m_instance_buffer.size() * sizeof(glm::vec4));
			render_frame.GetBeginFrameComamndbuffer().Close();

			render::EndPrepareRenderAndSubmit(m_render_system);
		}

		//Tick database
		ecs::Tick<GameDatabase>();

		//RENDER
		{
			display::BeginFrame(m_device);

			//Recreate the descriptor file and context if requested
			if (m_render_system_descriptor_load_requested)
			{
				//Remove the render context
				if (m_render_context)
				{
					render::DestroyRenderContext(m_render_system, m_render_context);
				}

				//Reset errors
				m_render_system_errors.clear();

				//Load render pass sample
				size_t buffer_size = strlen(m_text_buffer.data()) + 1;

				if (!render::LoadPassDescriptorFile(m_render_system, m_device, m_text_buffer.data(), buffer_size, m_render_system_errors))
				{
					core::LogError("Failed to load the new descriptor file, reverting changes");
					m_show_errors = true;
				}


				//Create pass
				render::RenderContext::PassInfo pass_info;
				pass_info.width = m_width;
				pass_info.height = m_height;

				render::ResourceMap init_resource_map;

				//Still load it if it fail, as it will use the last valid one
				m_render_context = render::CreateRenderContext(m_render_system, m_device, "Main"_sh32, pass_info, init_resource_map, m_render_system_context_errors);
				if (m_render_context == nullptr)
				{
					m_show_errors = true;
				}

				m_render_system_descriptor_load_requested = false;
			}

			//Update time
			struct GameConstantBuffer
			{
				float time[4];
			};
			GameConstantBuffer game_constant_buffer = { { static_cast<float>(total_time), elapsed_time, 0.f, 0.f } };

			//Update game constant buffer
			display::UpdateResourceBuffer(m_device, m_game_constant_buffer, &game_constant_buffer, sizeof(game_constant_buffer));

			render::Frame& render_frame = render::GetGameRenderFrame(m_render_system);

			//Execute begin commands in the render_frame
			auto render_context = display::OpenCommandList(m_device, m_render_command_list);

			render::CommandBuffer::CommandOffset command_offset = 0;
			while (command_offset != render::CommandBuffer::InvalidCommandOffset)
			{
				command_offset = render_frame.GetBeginFrameComamndbuffer().Execute(*render_context, command_offset);
			}
			
			display::CloseCommandList(m_device, render_context);
			display::ExecuteCommandList(m_device, m_render_command_list);

			if (m_render_context)
			{
				//Capture pass
				render::CaptureRenderContext(m_render_system, m_render_context);
				//Execute pass
				render::ExecuteRenderContext(m_render_system, m_render_context);
			}

			display::EndFrame(m_device);

			render_frame.Reset();
		}
	}

	void OnSizeChange(uint32_t width, uint32_t height, bool minimized) override
	{
		m_width = width;
		m_height = height;

		if (m_render_context)
		{
			render::RenderContext::PassInfo pass_info = m_render_context->GetPassInfo();
			pass_info.width = m_width;
			pass_info.height = m_height;
			m_render_context->UpdatePassInfo(pass_info);
		}
	}

	void OnAddImguiMenu() override
	{
		//Add menu for modifying the render system descriptor file
		if (ImGui::BeginMenu("RenderSystem"))
		{
			m_show_edit_descriptor_file = ImGui::MenuItem("Edit descriptor file");
			ImGui::EndMenu();
		}
	}

	void OnImguiRender() override
	{
		if (m_show_edit_descriptor_file)
		{
			if (!ImGui::Begin("Render System Descriptor File", &m_show_edit_descriptor_file))
			{
				ImGui::End();
				return;
			}

			ImGui::InputTextMultiline("file", m_text_buffer.data(), m_text_buffer.size(), ImVec2(-1.0f, ImGui::GetTextLineHeight() * 32), ImGuiInputTextFlags_AllowTabInput);
			if (ImGui::Button("Reset"))
			{
				memcpy(m_text_buffer.data(), m_render_passes_descriptor_buffer.data(), m_render_passes_descriptor_buffer.size());
			}
			if (ImGui::Button("Load"))
			{
				//Request a load from the text buffer 
				m_render_system_descriptor_load_requested = true;
			}

			ImGui::End();
		}

		if (m_show_errors)
		{
			//Show modal window with the errors
			ImGui::OpenPopup("Errors loading the render pass descriptors");
			if (ImGui::BeginPopupModal("Errors loading the render pass descriptors", NULL, ImGuiWindowFlags_AlwaysAutoResize))
			{
				for (auto& error : m_render_system_errors)
				{
					ImGui::Text(error.c_str());
				}
				for (auto& error : m_render_system_context_errors)
				{
					ImGui::Text(error.c_str());
				}
				ImGui::Separator();

				if (ImGui::Button("OK", ImVec2(120, 0)))
				{
					ImGui::CloseCurrentPopup();
					m_show_errors = false;
				}
				ImGui::EndPopup();
			}
		}
	}
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	void* param = reinterpret_cast<void*>(&hInstance);

	ECSGame ecs_game;

	return platform::Run("Entity Component System Test", param, ECSGame::kInitWidth, ECSGame::kInitHeight, &ecs_game);
}