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
	display::VertexBufferHandle m_quad_vertex_buffer;
	display::IndexBufferHandle m_quad_index_buffer;
	display::RootSignatureHandle m_root_signature;
	display::PipelineStateHandle m_grass_pipeline_state;
	display::PipelineStateHandle m_gazelle_pipeline_state;
	display::PipelineStateHandle m_lion_pipeline_state;

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
					float2 coords : TEXCOORD0; \
				};\
				\
				PSInput main_vs(float2 position : POSITION, float4 instance_data : TEXCOORD)\
				{\
					PSInput result; \
					result.position.xy = position.xy * instance_data.w + instance_data.xy; \
					result.position.zw = float2(0.f, 1.f); \
					result.coords.xy = position.xy; \
					return result;\
				}\
				float4 main_ps(PSInput input) : SV_TARGET\
				{\
					float alpha = smoothstep(1.f, 0.95f, length(input.coords.xy));\
					return float4(0.f, alpha, 0.f, alpha);\
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

		//Blend
		pipeline_state_desc.blend_desc.render_target_blend[0].blend_enable = true;
		pipeline_state_desc.blend_desc.render_target_blend[0].src_blend = display::Blend::SrcAlpha;
		pipeline_state_desc.blend_desc.render_target_blend[0].dest_blend = display::Blend::InvSrcAlpha;

		//Create
		m_grass_pipeline_state = display::CreatePipelineState(device, pipeline_state_desc, "Grass");

		//Cow
		//Create pipeline state
		const char* gazelle_shader_code =
			"	struct PSInput\
				{\
					float4 position : SV_POSITION;\
					float2 coords : TEXCOORD0; \
				};\
				\
				PSInput main_vs(float2 position : POSITION, float4 instance_data : TEXCOORD)\
				{\
					PSInput result; \
					result.position.xy = position.xy * instance_data.w + instance_data.xy; \
					result.position.zw = float2(0.f, 1.f); \
					result.coords.xy = position.xy; \
					return result;\
				}\
				float4 main_ps(PSInput input) : SV_TARGET\
				{\
					float alpha = smoothstep(1.f, 0.95f, length(input.coords.xy));\
					return float4(alpha, alpha, alpha, alpha);\
				}";


		compile_shader_desc.code = gazelle_shader_code;
		compile_shader_desc.entry_point = "main_vs";
		compile_shader_desc.target = "vs_5_0";
		display::CompileShader(device, compile_shader_desc, vertex_shader);

		compile_shader_desc.code = gazelle_shader_code;
		compile_shader_desc.entry_point = "main_ps";
		compile_shader_desc.target = "ps_5_0";
		display::CompileShader(device, compile_shader_desc, pixel_shader);

		//Add shaders
		pipeline_state_desc.pixel_shader.data = reinterpret_cast<void*>(pixel_shader.data());
		pipeline_state_desc.pixel_shader.size = pixel_shader.size();

		pipeline_state_desc.vertex_shader.data = reinterpret_cast<void*>(vertex_shader.data());
		pipeline_state_desc.vertex_shader.size = vertex_shader.size();

		//Create
		m_gazelle_pipeline_state = display::CreatePipelineState(device, pipeline_state_desc, "Gazelle");

		//Lion
		//Create pipeline state
		const char* lion_shader_code =
			"	struct PSInput\
				{\
					float4 position : SV_POSITION;\
					float2 coords : TEXCOORD0; \
				};\
				\
				PSInput main_vs(float2 position : POSITION, float4 instance_data : TEXCOORD)\
				{\
					PSInput result; \
					result.position.xy = position.xy * instance_data.w + instance_data.xy; \
					result.position.zw = float2(0.f, 1.f); \
					result.coords.xy = position.xy; \
					return result;\
				}\
				float4 main_ps(PSInput input) : SV_TARGET\
				{\
					float alpha = smoothstep(1.f, 0.95f, length(input.coords.xy));\
					return float4(alpha, alpha, 0.f, alpha);\
				}";


		compile_shader_desc.code = lion_shader_code;
		compile_shader_desc.entry_point = "main_vs";
		compile_shader_desc.target = "vs_5_0";
		display::CompileShader(device, compile_shader_desc, vertex_shader);

		compile_shader_desc.code = lion_shader_code;
		compile_shader_desc.entry_point = "main_ps";
		compile_shader_desc.target = "ps_5_0";
		display::CompileShader(device, compile_shader_desc, pixel_shader);

		//Add shaders
		pipeline_state_desc.pixel_shader.data = reinterpret_cast<void*>(pixel_shader.data());
		pipeline_state_desc.pixel_shader.size = pixel_shader.size();

		pipeline_state_desc.vertex_shader.data = reinterpret_cast<void*>(vertex_shader.data());
		pipeline_state_desc.vertex_shader.size = vertex_shader.size();

		//Create
		m_lion_pipeline_state = display::CreatePipelineState(device, pipeline_state_desc, "Lion");

		//Quad Vertex buffer
		{
			struct VertexData
			{
				float position[2];
			};

			VertexData vertex_data[4] =
			{
				{1.f, 1.f},
				{-1.f, 1.f},
				{1.f, -1.f},
				{-1.f, -1.f}
			};

			display::VertexBufferDesc vertex_buffer_desc;
			vertex_buffer_desc.init_data = vertex_data;
			vertex_buffer_desc.size = sizeof(vertex_data);
			vertex_buffer_desc.stride = sizeof(VertexData);

			m_quad_vertex_buffer = display::CreateVertexBuffer(device, vertex_buffer_desc, "quad_vertex_buffer");
		}

		//Quad Index buffer
		{
			uint16_t index_buffer_data[6] = { 0, 2, 1, 1, 2, 3 };
			display::IndexBufferDesc index_buffer_desc;
			index_buffer_desc.init_data = index_buffer_data;
			index_buffer_desc.size = sizeof(index_buffer_data);

			m_quad_index_buffer = display::CreateIndexBuffer(device, index_buffer_desc, "quad_index_buffer");
		}

	}

	void Unload(display::Device* device)
	{
		display::DestroyHandle(device, m_quad_vertex_buffer);
		display::DestroyHandle(device, m_quad_index_buffer);
		display::DestroyHandle(device, m_grass_pipeline_state);
		display::DestroyHandle(device, m_gazelle_pipeline_state);
		display::DestroyHandle(device, m_lion_pipeline_state);
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
	float grow_speed;
	float dead_size;

	GrassComponent(float _size, float _grow_speed, float _dead_size) : size(_size), grow_speed(_grow_speed), dead_size(_dead_size)
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

	//Game constant buffer
	display::ConstantBufferHandle m_game_constant_buffer;

	//Instances instance buffer
	display::VertexBufferHandle m_instances_vertex_buffer;

	//Command buffer
	display::CommandListHandle m_render_command_list;

	//Display resources
	DisplayResource m_display_resources;

	//Main point of view resources
	render::ResourceMap m_init_map_resource_map;

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

	//World size
	constexpr static float m_world_top = 1.f;
	constexpr static float m_world_bottom = -1.f;
	constexpr static float m_world_left = -1.f;
	constexpr static float m_world_right = 1.f;

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
		m_render_system = render::CreateRenderSystem(m_device);

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
		database_desc.num_max_entities_zone = 1024 * 128;
		ecs::CreateDatabase<GameDatabase>(database_desc);

		//Allocate random instances
		std::random_device rd;  //Will be used to obtain a seed for the random number engine
		std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
		std::uniform_real_distribution<float> rand_position_x(m_world_left, m_world_right);
		std::uniform_real_distribution<float> rand_position_y(m_world_bottom, m_world_top);
		std::uniform_real_distribution<float> rand_position_angle(0.f, 2.f * 3.1415926f);
		std::uniform_real_distribution<float> rand_lineal_velocity(-0.05f, 0.05f);
		std::uniform_real_distribution<float> rand_angle_velocity(-0.01f, 0.01f);
		std::uniform_real_distribution<float> rand_size(0.005f, 0.01f);
		std::uniform_real_distribution<float> rand_grass_grow_speed(0.0001f, 0.001f);
		std::uniform_real_distribution<float> rand_grass_dead_size(0.005f, 0.05f);

		for (size_t i = 0; i < 2000; ++i)
		{
			ecs::AllocInstance<GameDatabase, GazelleEntityType>()
				.Init<PositionComponent>(rand_position_x(gen), rand_position_y(gen), rand_position_angle(gen))
				.Init<VelocityComponent>(rand_lineal_velocity(gen), rand_lineal_velocity(gen), rand_angle_velocity(gen))
				.Init<GazelleComponent>(rand_size(gen));
		}

		for (size_t i = 0; i < 2000; ++i)
		{
			ecs::AllocInstance<GameDatabase, GrassEntityType>()
				.Init<PositionComponent>(rand_position_x(gen), rand_position_y(gen), 0.f)
				.Init<VelocityComponent>(0.f, 0.f, 0.f)
				.Init<GrassComponent>(0.f, rand_grass_grow_speed(gen), rand_grass_dead_size(gen));
		}

		for (size_t i = 0; i < 2000; ++i)
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

			//Grow grass
			ecs::Process<GameDatabase, GrassComponent>([&](const auto& instance_iterator, GrassComponent& grass)
			{
				grass.size += grass.grow_speed * elapsed_time;
				if (grass.size > grass.dead_size)
				{
					//Kill it
					instance_iterator.Dealloc();
				}
			}, zone_bitset);

			//Move entities
			ecs::Process<GameDatabase, PositionComponent, VelocityComponent>([&](const auto& instance_iterator, PositionComponent& position, VelocityComponent& velocity)
			{
				position.position_angle += velocity.lineal_angle_velocity * elapsed_time;
				if (position.position_angle.x > m_world_right)
				{
					position.position_angle.x = m_world_right;
					velocity.lineal_angle_velocity.x = -velocity.lineal_angle_velocity.x;
				}
				if (position.position_angle.x < m_world_left)
				{
					position.position_angle.x = m_world_left;
					velocity.lineal_angle_velocity.x = -velocity.lineal_angle_velocity.x;
				}
				if (position.position_angle.y > m_world_top)
				{
					position.position_angle.y = m_world_top;
					velocity.lineal_angle_velocity.y = -velocity.lineal_angle_velocity.y;
				}
				if (position.position_angle.y < m_world_bottom)
				{
					position.position_angle.y = m_world_bottom;
					velocity.lineal_angle_velocity.y = -velocity.lineal_angle_velocity.y;
				}
			}, zone_bitset);

		}

		//Recreate the descriptor file and context if requested
		if (m_render_system_descriptor_load_requested)
		{
			//Reset errors
			m_render_system_errors.clear();

			//Load render pass sample
			size_t buffer_size = strlen(m_text_buffer.data()) + 1;

			if (!render::LoadPassDescriptorFile(m_render_system, m_device, m_text_buffer.data(), buffer_size, m_render_system_errors))
			{
				core::LogError("Failed to load the new descriptor file, reverting changes");
				m_show_errors = true;
			}

			m_render_system_descriptor_load_requested = false;
		}

		//PREPARE RENDERING
		{
			std::bitset<1> zone_bitset(1);

			render::BeginPrepareRender(m_render_system);

			render::Frame& render_frame = render::GetGameRenderFrame(m_render_system);

			render::PassInfo pass_info;
			pass_info.width = m_width;
			pass_info.height = m_height;

			auto& point_of_view = render_frame.AllocPointOfView("Main"_sh32, 0, 0, pass_info, m_init_map_resource_map);
			auto& command_buffer = point_of_view.GetCommandBuffer();

			//Culling and draw per type
			m_instance_buffer.clear();

			ecs::Process<GameDatabase, PositionComponent, GrassComponent>([&](const auto& instance_iterator, PositionComponent& position, GrassComponent& grass)
			{
				m_instance_buffer.emplace_back(position.position_angle.x, position.position_angle.y, position.position_angle.z, grass.size);
			}, zone_bitset);

			{
				//Add a render item for rendering the grass
				auto commands_offset = command_buffer.Open();
				command_buffer.SetVertexBuffers(0, 1, &m_display_resources.m_quad_vertex_buffer);
				command_buffer.SetVertexBuffers(1, 1, &m_instances_vertex_buffer);
				command_buffer.SetIndexBuffer(m_display_resources.m_quad_index_buffer);
				command_buffer.SetPipelineState(m_display_resources.m_grass_pipeline_state);
				display::DrawIndexedInstancedDesc draw_desc;
				draw_desc.index_count = 6;
				draw_desc.instance_count = static_cast<uint32_t>(m_instance_buffer.size());
				command_buffer.DrawIndexedInstanced(draw_desc);
				command_buffer.Close();

				point_of_view.PushRenderItem(m_solid_render_priority, 0, commands_offset);
			}

			size_t instance_offset = m_instance_buffer.size();

			ecs::Process<GameDatabase, PositionComponent, GazelleComponent>([&](const auto& instance_iterator, PositionComponent& position, GazelleComponent& gazelle)
			{
				//Add to the instance buffer the instance
				m_instance_buffer.emplace_back(position.position_angle.x, position.position_angle.y, position.position_angle.z, gazelle.size);
			}, zone_bitset);

			{
				//Add a render item for rendering the gazelle
				auto commands_offset = command_buffer.Open();
				command_buffer.SetVertexBuffers(0, 1, &m_display_resources.m_quad_vertex_buffer);
				command_buffer.SetVertexBuffers(1, 1, &m_instances_vertex_buffer);
				command_buffer.SetIndexBuffer(m_display_resources.m_quad_index_buffer);
				command_buffer.SetPipelineState(m_display_resources.m_gazelle_pipeline_state);
				display::DrawIndexedInstancedDesc draw_desc;
				draw_desc.index_count = 6;
				draw_desc.instance_count = static_cast<uint32_t>(m_instance_buffer.size() - instance_offset);
				draw_desc.start_instance = static_cast<uint32_t>(instance_offset);
				command_buffer.DrawIndexedInstanced(draw_desc);
				command_buffer.Close();

				point_of_view.PushRenderItem(m_solid_render_priority, 1, commands_offset);
			}

			instance_offset = m_instance_buffer.size();
		
			ecs::Process<GameDatabase, PositionComponent, TigerComponent>([&](const auto& instance_iterator, PositionComponent& position, TigerComponent& tiger)
			{
				m_instance_buffer.emplace_back(position.position_angle.x, position.position_angle.y, position.position_angle.z, tiger.size);
			}, zone_bitset);

			{
				//Add a render item for rendering the lion
				auto commands_offset = command_buffer.Open();
				command_buffer.SetVertexBuffers(0, 1, &m_display_resources.m_quad_vertex_buffer);
				command_buffer.SetVertexBuffers(1, 1, &m_instances_vertex_buffer);
				command_buffer.SetIndexBuffer(m_display_resources.m_quad_index_buffer);
				command_buffer.SetPipelineState(m_display_resources.m_lion_pipeline_state);
				display::DrawIndexedInstancedDesc draw_desc;
				draw_desc.index_count = 6;
				draw_desc.instance_count = static_cast<uint32_t>(m_instance_buffer.size() - instance_offset);
				draw_desc.start_instance = static_cast<uint32_t>(instance_offset);
				command_buffer.DrawIndexedInstanced(draw_desc);
				command_buffer.Close();

				point_of_view.PushRenderItem(m_solid_render_priority, 2, commands_offset);
			}


			//Send the buffer for updating the vertex constant
			auto command_offset = render_frame.GetBeginFrameComamndbuffer().Open();
			render_frame.GetBeginFrameComamndbuffer().UploadResourceBuffer(m_instances_vertex_buffer, &m_instance_buffer[0], m_instance_buffer.size() * sizeof(glm::vec4));
			render_frame.GetBeginFrameComamndbuffer().Close();

			//Render
			render::EndPrepareRenderAndSubmit(m_render_system);
		}

		//Tick database
		ecs::Tick<GameDatabase>();
	}

	void OnSizeChange(uint32_t width, uint32_t height, bool minimized) override
	{
		m_width = width;
		m_height = height;


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