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
#include <job/job.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <ext/glm/gtx/vector_angle.hpp>
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

	float& GetNumEventsPerSecond()
	{
		return m_num_events_per_second;
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
	glm::vec2 position;
	float angle;

	PositionComponent(float x, float y, float _angle) : position(x, y), angle(_angle)
	{
	}

	glm::vec2 GetDirection() const { return glm::rotate(glm::vec2(0.f, 1.f), angle); };
};

struct VelocityComponent
{
	glm::vec2 lineal;
	float angular;

	VelocityComponent(float x, float y, float m) : lineal(x, y), angular(m)
	{
	}

	void AddLinealVelocity(const glm::vec2& lineal_velocity)
	{
		lineal += lineal_velocity;
	}

	void AddAngleVelocity(float angle_velocity)
	{
		angular += angle_velocity;
	}
};

//State, write/read component
struct GrassStateComponent
{
	float size;
	bool stop_growing;

	GrassStateComponent(float _size) : size(_size), stop_growing(false)
	{
	}
};

//Read only component
struct GrassComponent
{
	float grow_speed;
	float top_size;

	GrassComponent(float _grow_speed, float _top_size) : grow_speed(_grow_speed), top_size(_top_size)
	{
	}
};

//State, write/read component
struct GazelleStateComponent
{
	float size;

	GazelleStateComponent(float _size) : size(_size)
	{
	}
};

//Read only
struct GazelleComponent
{
	float repro_size;
	float grow_speed;
	float offset_time;
	float size_distance_rate;

	GazelleComponent(float _repro_size, float _grow_speed, float _offset_time, float _size_distance_rate) :
		repro_size(_repro_size), grow_speed(_grow_speed),
		offset_time(_offset_time), size_distance_rate(_size_distance_rate)
	{
	}
};

//State, write/read component
struct  LionStateComponent
{
	float size;
	float attack;

	LionStateComponent(float _size, float _attack) : size(_size), attack(_attack)
	{
	}
};

//Read only
struct LionComponent
{
	float repro_size;
	float grow_speed;
	float size_distance_rate;
	float attack_recharge;

	LionComponent(float _repro_size, float _grow_speed, float _size_distance_rate, float _attack_recharge) :
		repro_size(_repro_size), grow_speed(_grow_speed), size_distance_rate(_size_distance_rate), attack_recharge(_attack_recharge)
	{
	}
};

using GrassEntityType = ecs::EntityType<PositionComponent, GrassStateComponent, GrassComponent>;
using GazelleEntityType = ecs::EntityType<PositionComponent, VelocityComponent, GazelleStateComponent, GazelleComponent>;
using LionEntityType = ecs::EntityType<PositionComponent, VelocityComponent, LionStateComponent, LionComponent>;

using GameComponents = ecs::ComponentList<PositionComponent, VelocityComponent, GrassStateComponent, GazelleStateComponent, LionStateComponent, GrassComponent, GazelleComponent, LionComponent>;
using GameEntityTypes = ecs::EntityTypeList<GrassEntityType, GazelleEntityType, LionEntityType>;

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

	job::System* m_job_system = nullptr;

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

	//Camera info
	float m_camera_position[2] = { 0.f };
	float m_camera_zoom = 1.f;
	float m_camera_move_speed = 1.5f;
	float m_camera_zoom_speed = 2.5f;
	float m_camera_zoom_velocity = 0.f;
	float m_camera_position_velocity[2] = { 0.f };
	float m_camera_friction = 4.f;

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

	//Config world settings
	bool m_show_word_settings = false;

	//World size
	constexpr static float m_world_top = ZoneDescriptor::world_top;
	constexpr static float m_world_bottom = ZoneDescriptor::world_bottom;
	constexpr static float m_world_left = ZoneDescriptor::world_left;
	constexpr static float m_world_right = ZoneDescriptor::world_right;

	//Random generators
	std::random_device m_random_device;
	std::mt19937 m_random_generator;

	//Default parameters
	float m_min_grass_grow_speed = 0.01f;
	float m_max_grass_grow_speed = 0.015f;
	float m_min_grass_top_size = 0.02f;
	float m_max_grass_top_size = 0.0346f;
	float m_grass_creation_rate = 20.f;
	float m_grass_creation_desviation = 0.3f;

	float m_gazelle_init_size = 0.005f;
	float m_gazelle_creation_rate = 3.f;
	float m_gazelle_creation_desviation = 0.3f;
	float m_min_gazelle_grow_speed = 0.002f;
	float m_max_gazelle_grow_speed = 0.006f;
	float m_min_gazelle_top_size = 0.015f;
	float m_max_gazelle_top_size = 0.02f;
	float m_gazelle_food_grow_ratio = 15.f;
	float m_gazelle_food_moving = 0.002f;
	float m_min_gazelle_size_distance_rate = 0.6f;
	float m_max_gazelle_size_distance_rate = 1.5f;
	float m_gazelle_speed = 10.0f;
	float m_gazelle_speed_variation = 5.0f;
	float m_gazelle_looking_for_target_speed = 1.f;
	float m_gazelle_collision_speed = 200.f;
	float m_gazelle_avoiding_lion_speed = 2.0f;
	size_t m_gazelle_num_repro = 3;
	float m_gazelle_looking_max_distance = 0.2f;
	float m_gazelle_lion_avoid_max_distance = 0.1f;

	float m_lion_init_size = 0.01f;
	float m_lion_creation_rate = 0.05f;
	float m_lion_creation_desviation = 0.8f;
	float m_min_lion_eat_factor = 0.2f;
	float m_max_lion_eat_factor = 0.45f;
	float m_min_lion_top_size = 0.025f;
	float m_max_lion_top_size = 0.03f;
	float m_lion_food_moving = 0.002f;
	float m_min_lion_size_distance_rate = 0.6f;
	float m_max_lion_size_distance_rate = 1.5f;
	float m_lion_speed = 0.2f;
	float m_lion_rotation_speed = 2.0f;
	float m_lion_attack_speed = 4.5f;
	float m_lion_attack_rotation_speed = 50.f;
	float m_min_lion_attack_recharge = 1.f;
	float m_max_lion_attack_recharge = 3.f;
	float m_lion_avoid_speed = 3.f;
	size_t m_lion_num_repro = 3;
	float m_lion_target_attack_max_distance = 0.1f;

	float m_friction = 4.0f;
	float m_min_size = 0.001f;
	float m_gazelle_min_distance = 0.002f;

	//Random distributions
	std::uniform_real_distribution<float> m_random_position_x;
	std::uniform_real_distribution<float> m_random_position_y;
	std::uniform_real_distribution<float> m_random_position_angle;

	//Random events
	RandomEventsGenerator m_grass_creation_events;
	RandomEventsGenerator m_gazelle_creation_events;
	RandomEventsGenerator m_lion_creation_events;

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
		m_gazelle_creation_events(m_gazelle_creation_rate, m_gazelle_creation_desviation),
		m_lion_creation_events(m_lion_creation_rate, m_lion_creation_desviation)
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
		render::AddGameResource(m_render_system, "ZoomPosition"_sh32, CreateResourceFromHandle<render::ConstantBufferResource>(display::WeakConstantBufferHandle(m_display_resources.m_zoom_position)));

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

		//Create job system
		job::SystemDesc job_system_desc;
		m_job_system = job::CreateSystem(job_system_desc);
		
	}
	void OnDestroy() override
	{
		if (m_job_system)
		{
			job::DestroySystem(m_job_system);
		}

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
		ecs::Process<GameDatabase, const GrassStateComponent, const PositionComponent>([&](const auto& instance_iterator, const GrassStateComponent& grass, const PositionComponent& position_grass)
		{
			float distance = glm::length(position - position_grass.position);

			if (distance <= grass.size)
			{
				//Inside
				inside = true;
			}
		}, zone_bitset);

		return inside;
	}

	void CreateGazelle(const glm::vec2& position, float size = -1.f)
	{
		//Calculate zone, already based to the bigger size
		float top_size = Random(m_min_gazelle_top_size, m_max_gazelle_top_size);
		float grow_seed = Random(m_min_gazelle_grow_speed, m_max_gazelle_grow_speed);
		float size_distance_rate = Random(m_min_gazelle_size_distance_rate, m_max_gazelle_size_distance_rate);
		float time_offset = Random(0.f, 1.f);
		float init_size = (size < 0.f) ? m_gazelle_init_size : size;
		auto zone = GridZone::GetZone(position.x, position.y, top_size);
		ecs::AllocInstance<GameDatabase, GazelleEntityType>(zone)
			.Init<PositionComponent>(position.x, position.y, 0.f)
			.Init<GazelleStateComponent>(init_size)
			.Init<GazelleComponent>(top_size, grow_seed, time_offset, size_distance_rate)
			.Init<VelocityComponent>(0.f, 0.f, 0.f);
	}

	void CreateLion(const glm::vec2& position, float size = -1.f)
	{
		//Calculate zone, already based to the bigger size
		float top_size = Random(m_min_lion_top_size, m_max_lion_top_size);
		float eat_factor = Random(m_min_lion_eat_factor, m_max_lion_eat_factor);
		float size_distance_rate = Random(m_min_lion_size_distance_rate, m_max_lion_size_distance_rate);
		float attack_recharge = Random(m_min_lion_attack_recharge, m_max_lion_attack_recharge);
		auto zone = GridZone::GetZone(position.x, position.y, top_size);
		ecs::AllocInstance<GameDatabase, LionEntityType>(zone)
			.Init<PositionComponent>(position.x, position.y, Random(0.f, glm::two_pi<float>()))
			.Init<LionStateComponent>((size < 0.f) ? m_lion_init_size : size, 0.f)
			.Init<LionComponent>(top_size, eat_factor, size_distance_rate, attack_recharge)
			.Init<VelocityComponent>(0.f, 0.f, 0.f);
	}

	struct Border
	{
		float left;
		float right;
		float top;
		float bottom;
	};

	bool IsInside(const Border& borders, float x, float y, float size)
	{
		if (x + size < borders.left) return false;
		if (x - size > borders.right) return false;
		if (y + size < borders.top) return false;
		if (y - size > borders.bottom) return false;

		return true;
	}

	void GrassUpdate(float elapsed_time)
	{
		MICROPROFILE_SCOPEI("ECSTest", "GrassGrow", 0xFFFF77FF);

		const auto& zone_bitset = GridZone::All();

		//Grow grass
		ecs::Process<GameDatabase, const GrassComponent, GrassStateComponent, PositionComponent>([&](const auto& instance_iterator, const GrassComponent& grass, GrassStateComponent& grass_state, PositionComponent& position)
		{
			if (!grass_state.stop_growing && (grass_state.size < grass.top_size))
			{
				float new_size = grass_state.size + grass.grow_speed * elapsed_time;
				glm::vec2 grass_position = position.position;

				//Calculate zone bitset based of the range
				const auto& influence_zone_bitset = GridZone::CalculateInfluence(position.position.x, position.position.y, new_size);

				//Check if it collides with another grass O(N*N)
				bool collides = false;
				ecs::Process<GameDatabase, GrassComponent, GrassStateComponent, PositionComponent>([&](const auto& instance_iterator_b, const GrassComponent& grass_b, GrassStateComponent& grass_state_b, const PositionComponent& position_b)
				{
					if (instance_iterator_b != instance_iterator)
					{
						float distance = glm::length(grass_position - position_b.position);
						if (distance <= (new_size + grass_state_b.size))
						{
							collides = true;
						}
					}
				}, influence_zone_bitset);

				if (collides)
				{
					//It is dead, it is not going to grow more, just set the dead space to the current size
					grass_state.stop_growing = true;
				}
				else
				{
					//Grow
					grass_state.size = std::fmin(new_size, grass.top_size);
				}
			}

		}, zone_bitset);
	}

	void GazelleUpdate(double total_time, float elapsed_time)
	{
		MICROPROFILE_SCOPEI("ECSTest", "GazelleUpdate", 0xFFFF77FF);

		const auto& zone_bitset = GridZone::All();

		ecs::Process<GameDatabase, const GazelleComponent, GazelleStateComponent, PositionComponent, VelocityComponent>([&](const auto& instance_iterator, const GazelleComponent& gazelle, GazelleStateComponent& gazelle_state, PositionComponent& position_gazelle, VelocityComponent& velocity)
		{
			glm::vec2 gazelle_position = position_gazelle.position;

			if (gazelle_state.size < gazelle.repro_size)
			{
				//Calculate zone bitset based of the range
				const auto& grass_influence_zone_bitset = GridZone::CalculateInfluence(position_gazelle.position.x, position_gazelle.position.y, m_gazelle_looking_max_distance);

				//Eaten speed
				float eaten = gazelle.grow_speed * elapsed_time;

				//Find the target grass
				glm::vec2 target;
				float max_target_size = 0.f;

				//Check if it eats grass or find grass
				ecs::Process<GameDatabase, const GrassComponent, GrassStateComponent, const PositionComponent>([&](const auto& instance_iterator_b, const GrassComponent& grass, GrassStateComponent& grass_state, const PositionComponent& position_grass)
				{
					glm::vec2 grass_position = position_grass.position;

					float distance = glm::length(grass_position - gazelle_position);

					if (distance < (gazelle_state.size + grass_state.size))
					{
						//Eats all the grass
						if (grass_state.size <= eaten * m_gazelle_food_grow_ratio)
						{
							//Grow 
							gazelle_state.size += grass_state.size / m_gazelle_food_grow_ratio;


							//Kill grass
							grass_state.size = 0.f;
							instance_iterator_b.Dealloc();
						}
						else
						{
							gazelle_state.size += eaten;
							grass_state.size -= eaten * m_gazelle_food_grow_ratio;
						}
					}
					if (distance < m_gazelle_looking_max_distance)
					{
						float target_rate = powf(grass_state.size, gazelle.size_distance_rate) / (distance + 0.0001f);

						if (target_rate > max_target_size)
						{
							//New target
							target = grass_position;
							max_target_size = target_rate;
						}
					}
				}, grass_influence_zone_bitset);

				glm::vec2 target_velocity;
				float gazelle_speed = m_gazelle_speed + m_gazelle_speed_variation * static_cast<float>(cos(total_time + gazelle.offset_time * glm::two_pi<float>()));
				if (max_target_size == 0.f)
				{
					//Didn't find any, just go in the predeterminated direction
					target_velocity = glm::rotate(glm::vec2(elapsed_time * m_gazelle_looking_for_target_speed, 0.f), glm::two_pi<float>() * (static_cast<float>(total_time * 0.05) + gazelle.offset_time));
				}
				else
				{
					target_velocity = (target - gazelle_position) * gazelle_speed * elapsed_time;
				}

				velocity.AddLinealVelocity(target_velocity);

				//Check collision
				const auto& collision_influence_zone_bitset = GridZone::CalculateInfluence(position_gazelle.position.x, position_gazelle.position.y, gazelle_state.size + m_gazelle_min_distance);

				ecs::Process<GameDatabase, const GazelleComponent, GazelleStateComponent, const PositionComponent>([&](const auto& instance_iterator_b, const GazelleComponent& gazelle_b, GazelleStateComponent& gazelle_state_b, const PositionComponent& position_gazelle_b)
				{
					if (instance_iterator != instance_iterator_b)
					{
						glm::vec2 gazelle_position_b = position_gazelle_b.position;

						glm::vec2 difference = (gazelle_position_b - gazelle_position);
						float distance = glm::length(difference);

						float inside = distance - (gazelle_state.size + gazelle_state_b.size + m_gazelle_min_distance);
						if (inside < 0.f)
						{
							glm::vec2 response;
							if (distance < 0.0001f)
							{
								response = glm::rotate(glm::vec2(1.f, 0.f), Random(0.f, glm::two_pi<float>()));
							}
							else
							{
								response = glm::normalize(difference);
							}

							//Collision
							velocity.AddLinealVelocity(response * inside * m_gazelle_collision_speed * elapsed_time);
						}
					}
				}, collision_influence_zone_bitset);


				//Avoid lions
				const auto& lion_influence_zone_bitset = GridZone::CalculateInfluence(position_gazelle.position.x, position_gazelle.position.y, m_gazelle_lion_avoid_max_distance);

				ecs::Process<GameDatabase, const LionComponent, LionStateComponent, const PositionComponent>([&](const auto& instance_iterator_b, const LionComponent& lion, LionStateComponent& lion_state, const PositionComponent& position_lion)
				{
					glm::vec2 lion_position = position_lion.position;
					glm::vec2 lion_direction = position_lion.GetDirection();

					glm::vec2 difference = (gazelle_position - lion_position);
					float distance = glm::length(difference);

					if (distance < m_gazelle_lion_avoid_max_distance)
					{
						glm::vec2 normalize_difference = glm::normalize(difference);
						glm::vec2 normalize_lion_direction = glm::normalize(lion_direction);

						float dot_lion_gazelle = glm::dot(normalize_difference, normalize_lion_direction);

						float distance_factor = (m_gazelle_lion_avoid_max_distance - distance) / m_gazelle_lion_avoid_max_distance;
						//Check lion direction
						float worried_factor_front = glm::clamp(dot_lion_gazelle, 0.f, 1.f) * distance_factor;
						float worried_factor_close = 0.5f * distance_factor;

						//Reaction
						glm::vec2 response_front = glm::normalize(normalize_difference - dot_lion_gazelle * normalize_lion_direction);
						glm::vec2 response = response_front * worried_factor_front + normalize_difference * worried_factor_close;
						response *= m_gazelle_avoiding_lion_speed * elapsed_time;

						//Avoid
						velocity.AddLinealVelocity(response);
					}
				}, lion_influence_zone_bitset);
			}
			else
			{
				//Repro
				instance_iterator.Dealloc();

				for (size_t i = 0; i < m_gazelle_num_repro; ++i)
				{
					glm::vec2 offset = glm::rotate(glm::vec2(0.005f, 0.f), Random(0.f, glm::two_pi<float>()));
					glm::vec2 new_position = gazelle_position + offset;

					CreateGazelle(new_position, gazelle_state.size / static_cast<float>(m_gazelle_num_repro));
				}
			}

		}, zone_bitset);
	};

	void LionUpdate(float elapsed_time)
	{
		MICROPROFILE_SCOPEI("ECSTest", "LionUpdate", 0xFFFF77FF);

		const auto& zone_bitset = GridZone::All();

		ecs::Process<GameDatabase, const LionComponent, LionStateComponent, const PositionComponent, VelocityComponent>([&](const auto& instance_iterator, const LionComponent& lion, LionStateComponent& lion_state, const PositionComponent& position, VelocityComponent& velocity)
		{
			glm::vec2 lion_position = position.position;
			glm::vec2 lion_direction = position.GetDirection();

			velocity.AddLinealVelocity(lion_direction * m_lion_speed * elapsed_time);

			const float border_detection = 0.3f;

			//Avoid sides or other lions
			if (lion_position.x - border_detection < m_world_left)
			{
				//Calculate better direction
				if (glm::dot(lion_direction, glm::vec2(0.f, 1.f)) > 0.f)
					velocity.AddAngleVelocity(-m_lion_rotation_speed * elapsed_time);
				else
					velocity.AddAngleVelocity(m_lion_rotation_speed * elapsed_time);
			}
			else
				if (lion_position.x + border_detection > m_world_right)
				{
					//Calculate better direction
					if (glm::dot(lion_direction, glm::vec2(0.f, 1.f)) > 0.f)
						velocity.AddAngleVelocity(m_lion_rotation_speed * elapsed_time);
					else
						velocity.AddAngleVelocity(-m_lion_rotation_speed * elapsed_time);
				}
				else
					if (lion_position.y - border_detection < m_world_bottom)
					{
						//Calculate better direction
						if (glm::dot(lion_direction, glm::vec2(1.f, 0.f)) > 0.f)
							velocity.AddAngleVelocity(m_lion_rotation_speed * elapsed_time);
						else
							velocity.AddAngleVelocity(-m_lion_rotation_speed * elapsed_time);
					}
					else
						if (lion_position.y + border_detection > m_world_top)
						{
							//Calculate better direction
							if (glm::dot(lion_direction, glm::vec2(1.f, 0.f)) > 0.f)
								velocity.AddAngleVelocity(-m_lion_rotation_speed * elapsed_time);
							else
								velocity.AddAngleVelocity(m_lion_rotation_speed * elapsed_time);
						}

			//Check areas in front of the lion for activating attack or check if there is collision
			glm::vec2 lion_look_at = lion_position + m_lion_target_attack_max_distance * lion_direction;
			const auto& gazelle_influence_zone_bitset = GridZone::CalculateInfluence(lion_look_at.x, lion_look_at.y, m_lion_target_attack_max_distance + lion_state.size);

			//Find the target gazelle
			glm::vec2 target;
			float max_target_size = 0.f;

			ecs::Process<GameDatabase, GazelleStateComponent, const PositionComponent>([&](const auto& instance_iterator_b, GazelleStateComponent& gazelle_state, const PositionComponent& position_gazelle)
			{
				float distance = glm::length(position_gazelle.position - lion_position);

				if (distance < (gazelle_state.size + lion_state.size))
				{
					//Eats the gazelle
					instance_iterator_b.Dealloc();

					lion_state.size += gazelle_state.size * lion.grow_speed;
					gazelle_state.size = 0.f;
				}
				else if (distance < m_lion_target_attack_max_distance)
				{
					float direction_target = std::clamp(8.f * glm::dot(glm::normalize((position_gazelle.position - lion_position)),
						lion_direction), 0.f, 1.f);
					float size_distance_rate = powf(gazelle_state.size, lion.size_distance_rate) / (distance + 0.0001f);

					float target_rate = direction_target * size_distance_rate;

					if (target_rate > max_target_size)
					{
						//New target
						target = position_gazelle.position;
						max_target_size = target_rate;
					}
				}
			}, gazelle_influence_zone_bitset);

			if (max_target_size != 0.f)
			{
				if (lion_state.attack > 0.f)
				{
					//Start an attack
					lion_state.attack = -lion_state.attack;
				}
				else
				{
					//Attacking, orientate to target
					float angle = glm::orientedAngle(glm::normalize(target - lion_position), lion_direction);

					velocity.AddAngleVelocity(-angle * m_lion_attack_rotation_speed * elapsed_time);
					velocity.AddLinealVelocity(lion_direction * m_lion_attack_speed * elapsed_time);

					//Remove attack to the lion
					lion_state.attack += elapsed_time;
				}
			}

			//Recharge attack
			if (lion_state.attack > 0.f)
			{
				lion_state.attack += lion.attack_recharge * elapsed_time;
			}

			if (lion_state.size > lion.repro_size)
			{
				//Repro
				instance_iterator.Dealloc();

				for (size_t i = 0; i < m_lion_num_repro; ++i)
				{
					glm::vec2 offset = glm::rotate(glm::vec2(0.005f, 0.f), Random(0.f, glm::two_pi<float>()));
					glm::vec2 new_position = lion_position + offset;

					CreateLion(new_position, lion_state.size / static_cast<float>(m_lion_num_repro));
				}
			}

		}, zone_bitset);
	}

	void OnTick(double total_time, float elapsed_time) override
	{
		//UPDATE GAME
		{
			MICROPROFILE_SCOPEI("ECSTest", "Update", 0xFFFF77FF);

			GrassUpdate(elapsed_time);

			GazelleUpdate(total_time, elapsed_time);

			LionUpdate(elapsed_time);

			{
				MICROPROFILE_SCOPEI("ECSTest", "EntitiesMove", 0xFFFF77FF);

				const auto& zone_bitset = GridZone::All();

				//Move entities
				ecs::Process<GameDatabase, PositionComponent, VelocityComponent>([&](const auto& instance_iterator, PositionComponent& position, VelocityComponent& velocity)
				{
					position.position += velocity.lineal * elapsed_time;
					position.angle += velocity.angular * elapsed_time;

					if (position.position.x > m_world_right)
					{
						position.position.x = m_world_right;
						velocity.lineal.x = 0.f;
					}
					if (position.position.x < m_world_left)
					{
						position.position.x = m_world_left;
						velocity.lineal.x = 0.f;
					}
					if (position.position.y > m_world_top)
					{
						position.position.y = m_world_top;
						velocity.lineal.y = 0.f;
					}
					if (position.position.y < m_world_bottom)
					{
						position.position.y = m_world_bottom;
						velocity.lineal.y = 0.f;
					}

					float size = 0.f;
					if (instance_iterator.Contain<LionStateComponent>())
					{
						auto& lion_state = instance_iterator.Get<LionStateComponent>();

						//Consume size by lenght moved
						lion_state.size -= m_lion_food_moving * glm::length(velocity.lineal * elapsed_time);

						if (lion_state.size < m_min_size)
						{
							//Dead, moved a lot and didn't eat
							instance_iterator.Dealloc();
						}

						size = lion_state.size;
					}
					else if (instance_iterator.Contain<GazelleStateComponent>())
					{
						auto& gazelle_state = instance_iterator.Get<GazelleStateComponent>();

						//Consume size by lenght moved
						gazelle_state.size -= m_gazelle_food_moving * glm::length(velocity.lineal * elapsed_time);

						if (gazelle_state.size < m_min_size)
						{
							//Dead, moved a lot and didn't eat
							instance_iterator.Dealloc();
						}

						size = gazelle_state.size;
					}

					//Move to the new zone if needed
					auto zone = GridZone::GetZone(position.position.x, position.position.y, size);
					instance_iterator.Move(zone);
					
					//Friction
					float friction = glm::clamp(m_friction * elapsed_time, 0.f, 1.f);
					velocity.lineal -= velocity.lineal * friction;
					velocity.angular -= velocity.angular * friction;
			
					
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
							.Init<GrassComponent>(grow_speed, top_size)
							.Init<GrassStateComponent>(0.f);
					}
				}
			}

			{
				MICROPROFILE_SCOPEI("ECSTest", "NewGazelles", 0xFFFF77FF);
				//Calculate new lions
				const size_t gazelle_count_to_create = m_gazelle_creation_events.Events(m_random_generator, elapsed_time);

				for (size_t i = 0; i < gazelle_count_to_create; ++i)
				{
					glm::vec2 position(m_random_position_x(m_random_generator), m_random_position_y(m_random_generator));

					CreateGazelle(position);
				}
			}

			{
				MICROPROFILE_SCOPEI("ECSTest", "NewLions", 0xFFFF77FF);
				//Calculate new lions
				const size_t lions_count_to_create = m_lion_creation_events.Events(m_random_generator, elapsed_time);

				for (size_t i = 0; i < lions_count_to_create; ++i)
				{
					glm::vec2 position(m_random_position_x(m_random_generator), m_random_position_y(m_random_generator));

					CreateLion(position);
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

			render::BeginPrepareRender(m_render_system);

			render::Frame& render_frame = render::GetGameRenderFrame(m_render_system);

			Border borders;

			//UPDATE CAMERA
			{
				//Reset camera
				for (auto& input_event : GetInputEvents())
				{
					if (input_event.type == platform::EventType::KeyDown && input_event.slot == platform::InputSlot::Escape)
					{
						m_camera_zoom = 1.f;
						m_camera_position[0] = m_camera_position[1] = 0.f;
						m_camera_zoom_velocity = 0.f;
						m_camera_position_velocity[0] = m_camera_position_velocity[1] = 0.f;
					}
				}

				//New camera parameters
				if (GetInputSlotState(platform::InputSlot::PageDown))
				{
					m_camera_zoom_velocity += m_camera_zoom_speed * elapsed_time;
				}
				if (GetInputSlotState(platform::InputSlot::PageUp))
				{
					m_camera_zoom_velocity -= m_camera_zoom_speed * elapsed_time;
				}
				if (GetInputSlotState(platform::InputSlot::Right))
				{
					m_camera_position_velocity[0] += m_camera_move_speed * elapsed_time * m_camera_zoom;
				}
				if (GetInputSlotState(platform::InputSlot::Left))
				{
					m_camera_position_velocity[0] -= m_camera_move_speed * elapsed_time * m_camera_zoom;
				}
				if (GetInputSlotState(platform::InputSlot::Down))
				{
					m_camera_position_velocity[1] -= m_camera_move_speed * elapsed_time * m_camera_zoom;
				}
				if (GetInputSlotState(platform::InputSlot::Up))
				{
					m_camera_position_velocity[1] += m_camera_move_speed * elapsed_time * m_camera_zoom;
				}

				//Adjust aspect ratio
				float max_size = static_cast<float>(std::max(m_width, m_height));
				float aspect_ratio_x = static_cast<float>(m_height) / max_size;
				float aspect_ratio_y = static_cast<float>(m_width) / max_size;

				//Calculate new positions
				m_camera_zoom += m_camera_zoom_velocity * elapsed_time;
				m_camera_position[0] += m_camera_position_velocity[0] * elapsed_time;
				m_camera_position[1] += m_camera_position_velocity[1] * elapsed_time;

				if (m_camera_zoom < 0.1f)
				{
					m_camera_zoom = 0.1f;
				}

				//Friction
				m_camera_zoom_velocity -= m_camera_zoom_velocity * glm::clamp(m_camera_friction * elapsed_time, 0.f, 1.f);
				m_camera_position_velocity[0] -= m_camera_position_velocity[0] * glm::clamp(m_camera_friction * elapsed_time, 0.f, 1.f);
				m_camera_position_velocity[1] -= m_camera_position_velocity[1] * glm::clamp(m_camera_friction * elapsed_time, 0.f, 1.f);

				float position_zoom_buffer[4];
				position_zoom_buffer[0] = aspect_ratio_x * m_camera_zoom;
				position_zoom_buffer[1] = aspect_ratio_y * m_camera_zoom;
				position_zoom_buffer[2] = m_camera_position[0];
				position_zoom_buffer[3] = m_camera_position[1];

				//Update buffer
				auto command_offset = render_frame.GetBeginFrameComamndbuffer().Open();
				render_frame.GetBeginFrameComamndbuffer().UploadResourceBuffer(m_display_resources.m_zoom_position, position_zoom_buffer, sizeof(position_zoom_buffer));
				render_frame.GetBeginFrameComamndbuffer().Close();

				//Calculate culling
				borders.left = m_camera_position[0] - 1.f / (aspect_ratio_x * m_camera_zoom);
				borders.right = m_camera_position[0] + 1.f / (aspect_ratio_x * m_camera_zoom);
				borders.top = m_camera_position[1] - 1.f / (aspect_ratio_y * m_camera_zoom);
				borders.bottom = m_camera_position[1] + 1.f / (aspect_ratio_y * m_camera_zoom);
			}
			
			//Coarse culling
			const auto& zone_bitset = GridZone::CalculateInfluence(borders.left, borders.top, borders.right, borders.bottom);

			render::PassInfo pass_info;
			pass_info.width = m_width;
			pass_info.height = m_height;

			auto& point_of_view = render_frame.AllocPointOfView("Main"_sh32, 0, 0, pass_info, m_init_map_resource_map);
			auto& command_buffer = point_of_view.GetCommandBuffer();


			//Culling and draw per type
			m_instance_buffer.clear();

			ecs::Process<GameDatabase, PositionComponent, GrassStateComponent>([&](const auto& instance_iterator, const PositionComponent& position, const GrassStateComponent& grass)
			{
				if (IsInside(borders, position.position.x, position.position.y, grass.size))
				{
					m_instance_buffer.emplace_back(position.position.x, position.position.y, position.angle, grass.size);
				}
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

			ecs::Process<GameDatabase, PositionComponent, GazelleStateComponent>([&](const auto& instance_iterator, PositionComponent& position, GazelleStateComponent& gazelle)
			{
				//Add to the instance buffer the instance
				if (IsInside(borders, position.position.x, position.position.y, gazelle.size))
				{
					m_instance_buffer.emplace_back(position.position.x, position.position.y, position.angle, gazelle.size);
				}
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
		
			ecs::Process<GameDatabase, PositionComponent, LionStateComponent>([&](const auto& instance_iterator, PositionComponent& position, LionStateComponent& lion)
			{
				//Add to the instance buffer the instance
				if (IsInside(borders, position.position.x, position.position.y, lion.size))
				{
					m_instance_buffer.emplace_back(position.position.x, position.position.y, position.angle, lion.size);
				}
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
			m_show_word_settings = ImGui::MenuItem("Show world settings");
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
			ImGui::Text("Num lion entities (%zu)", ecs::GetNumInstances<GameDatabase, LionEntityType>());

			ecs::DatabaseStats database_stats;
			ecs::GetDatabaseStats<GameDatabase>(database_stats);

			ImGui::Separator();
			ImGui::Text("Num deferred deletions (%zu)", database_stats.num_deferred_deletions);
			ImGui::Text("Num deferred moves (%zu)", database_stats.num_deferred_moves);

			ImGui::End();
		}

		if (m_show_word_settings)
		{
			if (!ImGui::Begin("World settings", &m_show_word_settings))
			{
				ImGui::End();
				return;
			}
			ImGui::SliderFloat("Friction", &m_friction, 0.f, 20.f);
			ImGui::Separator();
			ImGui::SliderFloat("Grass creation rate", &m_grass_creation_events.GetNumEventsPerSecond(), 0.f, 100.f);
			ImGui::SliderFloat("Gazelle creation rate", &m_gazelle_creation_events.GetNumEventsPerSecond(), 0.f, 100.f);
			ImGui::SliderFloat("Lion creation rate", &m_lion_creation_events.GetNumEventsPerSecond(), 0.f, 20.f);
			ImGui::Separator();
			ImGui::SliderFloat2("Grass grow speed", &m_min_grass_grow_speed, 0.f, 0.2f);
			ImGui::SliderFloat2("Grass top size", &m_min_grass_top_size, 0.f, 0.35f);
			ImGui::Separator();
			ImGui::SliderFloat("Gazelle init size", &m_gazelle_init_size, 0.f, 0.35f);
			ImGui::SliderFloat2("Gazelle grow speed", &m_min_gazelle_grow_speed, 0.f, 0.01f);
			ImGui::SliderFloat2("Gazelle top size", &m_min_gazelle_top_size, 0.f, 0.35f);
			ImGui::SliderFloat("Gazelle grow food ratio", &m_gazelle_food_grow_ratio, 0.f, 100.f);
			ImGui::SliderFloat("Gazelle movement waste", &m_gazelle_food_moving, 0.f, 0.04f);
			ImGui::SliderFloat2("Gazelle search size/distance ratio", &m_min_gazelle_size_distance_rate, 0.f, 4.f);
			ImGui::SliderFloat("Gazelle speed", &m_gazelle_speed, 0.f, 100.f);
			ImGui::SliderFloat("Gazelle speed variation", &m_gazelle_speed_variation, 0.f, 50.f);
			ImGui::SliderFloat("Gazelle no target speed", &m_gazelle_looking_for_target_speed, 0.f, 5.f);
			ImGui::SliderFloat("Gazelle colision speed", &m_gazelle_collision_speed, 0.f, 1000.f);
			ImGui::SliderFloat("Gazelle avoiding lion speed", &m_gazelle_avoiding_lion_speed, 0.f, 100.f);
			ImGui::SliderInt("Gazelle number repro", reinterpret_cast<int*>(&m_gazelle_num_repro), 1, 10);
			ImGui::SliderFloat("Gazelle looking max distance", &m_gazelle_looking_max_distance, 0.f, 1.f);
			ImGui::SliderFloat("Gazelle lion avoid max distance", &m_gazelle_lion_avoid_max_distance, 0.f, 1.f);
			ImGui::Separator();
			ImGui::SliderFloat("Lion init size", &m_lion_init_size, 0.f, 0.35f);
			ImGui::SliderFloat2("Lion eat factor", &m_min_lion_eat_factor, 0.f, 1.0f);
			ImGui::SliderFloat2("Lion top size", &m_min_lion_top_size, 0.f, 0.35f);
			ImGui::SliderFloat("Lion movement waste", &m_lion_food_moving, 0.f, 0.01f);
			ImGui::SliderFloat2("Lion search size/distance ratio", &m_min_lion_size_distance_rate, 0.f, 4.f);
			ImGui::SliderFloat("Lion speed", &m_lion_speed, 0.f, 10.f);
			ImGui::SliderFloat("Lion rotation speed", &m_lion_rotation_speed, 0.f, 10.f);
			ImGui::SliderFloat("Lion attack speed", &m_lion_attack_speed, 0.f, 100.f);
			ImGui::SliderFloat("Lion attack rotation speed", &m_lion_attack_rotation_speed, 0.f, 100.f);
			ImGui::SliderFloat("Lion avoid speed", &m_lion_avoid_speed, 0.f, 10.f);
			ImGui::SliderInt("Lion number repro", reinterpret_cast<int*>(&m_lion_num_repro), 1, 10);
			ImGui::SliderFloat("Lion target max distance", &m_lion_target_attack_max_distance, 0.f, 1.f);
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