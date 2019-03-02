#define NOMINMAX
#include <core/platform.h>
#include <display/display.h>
#include <render/render.h>
#include <render/render_resource.h>
#include <render/render_helper.h>
#include <ecs/entity_component_system.h>
#include <ecs/zone_bitmask_helper.h>
#include <ext/glm/vec4.hpp>
#include <ext/glm/vec2.hpp>
#include <ext/glm/gtc/constants.hpp>
#include <core/profile.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <ext/glm/gtx/rotate_vector.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif
#include <windows.h>
#include <fstream>
#include <random>
#include <bitset>
#include <algorithm>

#include "resources.h"

class RandomEventsGenerator
{
	std::normal_distribution<float> m_distribution;
	float m_num_events_per_second;
	float m_event_timer;
public:
	RandomEventsGenerator(float num_events_per_second, float desviation)
		: m_distribution(1.f, desviation), m_num_events_per_second(num_events_per_second), m_event_timer(0.f)
	{
	}

	//Calculate number of events for this elapsed time
	template<typename RANDON_GENERATOR>
	size_t Events(RANDON_GENERATOR& generator, float elapsed_time)
	{
		m_event_timer += m_num_events_per_second * m_distribution(generator) * elapsed_time;

		if (m_event_timer >= 1.f)
		{
			const float floor_event_timer = floor(m_event_timer);
			const size_t num_events = static_cast<size_t>(floor_event_timer);
			m_event_timer = m_event_timer - floor_event_timer;
			return num_events;
		}
		else
		{
			return 0;
		}
	}
};

struct ZoneDescriptor
{
	static constexpr uint16_t side_count = 16;
	static constexpr float world_top = 1.f;
	static constexpr float world_bottom = -1.f;
	static constexpr float world_left = -1.f;
	static constexpr float world_right = 1.f;
	static constexpr float object_zero_zone_max_size = 0.035f;
};

using GridZone = ecs::GridOneLevel<ZoneDescriptor>;

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
	float top_size;

	GrassComponent(float _size, float _grow_speed, float _top_size) : size(_size), grow_speed(_grow_speed), top_size(_top_size)
	{
	}
};


struct GazelleComponent
{
	float size;
	float repro_size;
	float grow_speed;

	GazelleComponent(float _size, float _repro_size, float _grow_speed) : size(_size), repro_size(_repro_size), grow_speed(_grow_speed)
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

using GrassEntityType = ecs::EntityType<PositionComponent, GrassComponent>;
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

	//Show ecs debug info
	bool m_show_ecs_stats = false;

	//World size
	constexpr static float m_world_top = 1.f;
	constexpr static float m_world_bottom = -1.f;
	constexpr static float m_world_left = -1.f;
	constexpr static float m_world_right = 1.f;
	constexpr static float m_world_max_size = 0.025f;

	//Random generators
	std::random_device m_random_device;
	std::mt19937 m_random_generator;

	//Default parameters
	float m_min_grass_grow_speed = 0.001f;
	float m_max_grass_grow_speed = 0.01f;
	float m_min_grass_top_size = 0.01f;
	float m_max_grass_top_size = 0.03f;
	float m_grass_creation_rate = 20.f;
	float m_grass_creation_desviation = 1.f;
	float m_gazelle_init_size = 0.005f;
	float m_gazelle_creation_rate = 20.f;
	float m_gazelle_creation_desviation = 1.f;
	float m_min_gazelle_grow_speed = 0.001f;
	float m_max_gazelle_grow_speed = 0.002f;
	float m_min_gazelle_top_size = 0.02f;
	float m_max_gazelle_top_size = 0.03f;

	//Random distributions
	std::uniform_real_distribution<float> m_random_position_x;
	std::uniform_real_distribution<float> m_random_position_y;
	std::uniform_real_distribution<float> m_random_position_angle;

	//Random events
	RandomEventsGenerator m_grass_creation_events;
	RandomEventsGenerator m_gazelle_creation_events;


	float Random(float min, float max)
	{
		float random_01 = static_cast<float>(m_random_generator() - m_random_generator.min()) / static_cast<float>(m_random_generator.max() - m_random_generator.min());
		return min + random_01 * (max - min);
	}

	ECSGame() : m_random_generator(m_random_device()),
		m_random_position_x(m_world_left, m_world_right),
		m_random_position_y(m_world_bottom, m_world_top),
		m_random_position_angle(0.f, glm::two_pi<float>()),
		m_grass_creation_events(m_grass_creation_rate, m_grass_creation_desviation),
		m_gazelle_creation_events(m_gazelle_creation_rate, m_gazelle_creation_desviation)
	{
	}

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
		database_desc.num_zones = GridZone::zone_count;
		ecs::CreateDatabase<GameDatabase>(database_desc);
		
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

	//Check if there is a grass in the current position
	bool FreeGrassPosition(const glm::vec2& position)
	{
		const auto& zone_bitset = GridZone::All();

		bool inside = false;
		ecs::Process<GameDatabase, GrassComponent, PositionComponent>([&](const auto& instance_iterator, GrassComponent& grass, const PositionComponent& position_grass)
		{
			float distance = glm::length(position - glm::vec2(position_grass.position_angle.x, position_grass.position_angle.y));

			if (distance <= grass.size)
			{
				//Inside
				inside = true;
			}
		}, zone_bitset);

		return inside;
	}

	void OnTick(double total_time, float elapsed_time) override
	{
		//UPDATE GAME
		{
			MICROPROFILE_SCOPEI("ECSTest", "Update", 0xFFFF77FF);
			const auto& zone_bitset = GridZone::All();

			{
				MICROPROFILE_SCOPEI("ECSTest", "GrassGrow", 0xFFFF77FF);
				//Grow grass
				ecs::Process<GameDatabase, GrassComponent, PositionComponent>([&](const auto& instance_iterator, GrassComponent& grass, const PositionComponent& position)
				{
					if (grass.size < grass.top_size)
					{
						float new_size = grass.size + grass.grow_speed * elapsed_time;
						glm::vec2 grass_position(position.position_angle.x, position.position_angle.y);

						//Calculate zone bitset based of the range
						const auto& influence_zone_bitset = GridZone::CalculateInfluence(position.position_angle.x, position.position_angle.y, new_size);

						//Check if it collides with another grass O(N*N)
						bool collides = false;
						ecs::Process<GameDatabase, GrassComponent, PositionComponent>([&](const auto& instance_iterator_b, GrassComponent& grass_b, const PositionComponent& position_b)
						{
							if (instance_iterator_b != instance_iterator)
							{
								float distance = glm::length(grass_position - glm::vec2(position_b.position_angle.x, position_b.position_angle.y));
								if (distance <= (new_size + grass_b.size))
								{
									collides = true;
								}
							}
						}, influence_zone_bitset);

						if (collides)
						{
							//It is dead, it is not going to grow more, just set the dead space to the current size
							grass.top_size = grass.size;
						}
						else
						{
							//Grow
							grass.size = std::fmin(new_size, grass.top_size);
						}
					}

				}, zone_bitset);
			}

			{
				MICROPROFILE_SCOPEI("ECSTest", "GazelleUpdate", 0xFFFF77FF);
				
				ecs::Process<GameDatabase, GazelleComponent, PositionComponent, VelocityComponent>([&](const auto& instance_iterator, GazelleComponent& gazelle, const PositionComponent& position, VelocityComponent& velocity)
				{
					if (gazelle.size < gazelle.repro_size)
					{
						//Calculate zone bitset based of the range
						const auto& influence_zone_bitset = GridZone::CalculateInfluence(position.position_angle.x, position.position_angle.y, 0.2f);

						//Eaten speed
						float eaten = gazelle.grow_speed * elapsed_time;
						//Random value in case nothing is found
						glm::vec2 target(m_random_position_x(m_random_generator), m_random_position_y(m_random_generator));
						float max_target_size = 0.f;

						glm::vec2 gazelle_position(position.position_angle.x, position.position_angle.y);

						//Check if it eats grass or find grass
						ecs::Process<GameDatabase, GrassComponent, PositionComponent>([&](const auto& instance_iterator_b, GrassComponent& grass, const PositionComponent& position_b)
						{
							glm::vec2 grass_position (position_b.position_angle.x, position_b.position_angle.y);

							float distance = glm::length(grass_position - gazelle_position);

							if (distance < (gazelle.size + grass.size))
							{
								//Eats
								grass.size -= eaten;
								if (grass.size <= 0.f)
								{
									//Kill grass
									//instance_iterator_b.Dealloc();
								}
								gazelle.size += eaten;
							}

							if (grass.size / (distance + 0.0001f) > max_target_size)
							{
								//New target
								target = grass_position;
								max_target_size = grass.size / (distance + 0.0001f);
							}
						}, influence_zone_bitset);

						glm::vec2 target_velocity = (target - gazelle_position) * 0.1f;
						velocity.lineal_angle_velocity.x += target_velocity.x;
						velocity.lineal_angle_velocity.y += target_velocity.y;
					}
					else
					{
						/*
						//Repro
						instance_iterator.Dealloc();

						for (size_t i = 0; i < 3; ++i)
						{
							//Calculate zone, already based to the bigger size
							float top_size = Random(m_min_gazelle_top_size, m_max_gazelle_top_size);
							float grow_seed = Random(m_min_gazelle_grow_speed, m_max_gazelle_grow_speed);
							auto zone = GridZone::GetZone(position.position_angle.x, position.position_angle.y, top_size);
							ecs::AllocInstance<GameDatabase, GazelleEntityType>(zone)
								.Init<PositionComponent>(position.position_angle.x, position.position_angle.y, 0.f)
								.Init<GazelleComponent>(m_gazelle_init_size, grow_seed, top_size)
								.Init<VelocityComponent>(0.f, 0.f, 0.f);
						}
						*/
					}

				}, zone_bitset);
			}

			{
				MICROPROFILE_SCOPEI("ECSTest", "EntitiesMove", 0xFFFF77FF);
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

					if (instance_iterator.Contain<GazelleComponent>())
					{
						//auto zone = GridZone::GetZone(position.position_angle.x, position.position_angle.y, instance_iterator.Get<GazelleComponent>().size);
						//instance_iterator.Move(zone);
					}

					//Friction
					velocity.lineal_angle_velocity.x *= 0.5f;
					velocity.lineal_angle_velocity.y *= 0.5f;
					
				}, zone_bitset);

			}

			{
				MICROPROFILE_SCOPEI("ECSTest", "NewGrass", 0xFFFF77FF);
				//Calculate new grass
				const size_t grass_count_to_create = m_grass_creation_events.Events(m_random_generator, elapsed_time);

				for (size_t i = 0; i < grass_count_to_create; ++i)
				{
					glm::vec2 position(m_random_position_x(m_random_generator), m_random_position_y(m_random_generator));

					if (!FreeGrassPosition(position))
					{
						//Calculate zone, already based to the bigger size
						float top_size = Random(m_min_grass_top_size, m_max_grass_top_size);
						float grow_speed = Random(m_min_grass_grow_speed, m_max_grass_grow_speed);
						auto zone = GridZone::GetZone(position.x, position.y, top_size);
						ecs::AllocInstance<GameDatabase, GrassEntityType>(zone)
							.Init<PositionComponent>(position.x, position.y, 0.f)
							.Init<GrassComponent>(0.f, grow_speed, top_size);
					}
				}
			}

			{
				MICROPROFILE_SCOPEI("ECSTest", "NewGazelles", 0xFFFF77FF);
				//Calculate new grass
				const size_t gazelle_count_to_create = m_gazelle_creation_events.Events(m_random_generator, elapsed_time);

				for (size_t i = 0; i < gazelle_count_to_create; ++i)
				{
					glm::vec2 position(m_random_position_x(m_random_generator), m_random_position_y(m_random_generator));

					//Calculate zone, already based to the bigger size
					float top_size = Random(m_min_gazelle_top_size, m_max_gazelle_top_size);
					float grow_seed = Random(m_min_gazelle_grow_speed, m_max_gazelle_grow_speed);
					auto zone = GridZone::GetZone(position.x, position.y, top_size);
					ecs::AllocInstance<GameDatabase, GazelleEntityType>(zone)
						.Init<PositionComponent>(position.x, position.y, 0.f)
						.Init<GazelleComponent>(m_gazelle_init_size, top_size, grow_seed)
						.Init<VelocityComponent>(0.f, 0.f, 0.f);
				}
			}
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
			MICROPROFILE_SCOPEI("ECSTest", "PrepareRendering", 0xFFFF77FF);
			const auto& zone_bitset = GridZone::All();

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

			if (m_instance_buffer.size() > 0)
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

			if ((m_instance_buffer.size() - instance_offset) > 0)
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

			if ((m_instance_buffer.size() - instance_offset) > 0)
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

			if (m_instance_buffer.size() > 0)
			{
				//Send the buffer for updating the vertex constant
				auto command_offset = render_frame.GetBeginFrameComamndbuffer().Open();
				render_frame.GetBeginFrameComamndbuffer().UploadResourceBuffer(m_instances_vertex_buffer, &m_instance_buffer[0], m_instance_buffer.size() * sizeof(glm::vec4));
				render_frame.GetBeginFrameComamndbuffer().Close();
			}

			//Render
			render::EndPrepareRenderAndSubmit(m_render_system);
		}

		{
			MICROPROFILE_SCOPEI("ECSTest", "DatabaseTick", 0xFFFF77FF);
			//Tick database
			ecs::Tick<GameDatabase>();
		}
	}

	void OnSizeChange(uint32_t width, uint32_t height, bool minimized) override
	{
		m_width = width;
		m_height = height;


	}

	void OnAddImguiMenu() override
	{
		//Add menu for modifying the render system descriptor file
		if (ImGui::BeginMenu("ECS"))
		{
			m_show_edit_descriptor_file = ImGui::MenuItem("Edit descriptor file");
			m_show_ecs_stats = ImGui::MenuItem("Show ECS stats");
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

		if (m_show_ecs_stats)
		{
			if (!ImGui::Begin("Show ECS stats", &m_show_ecs_stats))
			{
				ImGui::End();
				return;
			}
			ImGui::Text("Num grass entities (%zu)", ecs::GetNumInstances<GameDatabase, GrassEntityType>());
			ImGui::Text("Num gazelle entities (%zu)", ecs::GetNumInstances<GameDatabase, GazelleEntityType>());
			ImGui::Text("Num tiger entities (%zu)", ecs::GetNumInstances<GameDatabase, TigerEntityType>());

			ecs::DatabaseStats database_stats;
			ecs::GetDatabaseStats<GameDatabase>(database_stats);

			ImGui::Separator();
			ImGui::Text("Num deferred deletions (%zu)", database_stats.num_deferred_deletions);
			ImGui::Text("Num deferred moves (%zu)", database_stats.num_deferred_moves);

			ImGui::End();
		}
	}
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	void* param = reinterpret_cast<void*>(&hInstance);

	ECSGame ecs_game;

	return platform::Run("Entity Component System Test", param, ECSGame::kInitWidth, ECSGame::kInitHeight, &ecs_game);
}