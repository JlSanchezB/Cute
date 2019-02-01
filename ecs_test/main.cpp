#include <core/platform.h>
#include <display/display.h>
#include <render/render.h>
#include <render/render_resource.h>
#include <render/render_helper.h>
#include <ecs/entity_component_system.h>
#include <ext/glm/vec2.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif
#include <windows.h>
#include <fstream>
#include <random>
#include <bitset>

struct PositionComponent
{
	glm::vec2 position;
	float angle;

	PositionComponent(float x, float y, float _angle) : position(x, y), angle(_angle)
	{
	}
};

struct VelocityComponent
{
	glm::vec2 lineal_velocity;
	float angle_velocity;

	VelocityComponent(float x, float y, float m) : lineal_velocity(x, y), angle_velocity(m)
	{
	}
};


struct TriangleShapeComponent
{
	float size;

	TriangleShapeComponent(float _size) : size(_size)
	{
	}
};

struct CircleShapeComponent
{
	float size;

	CircleShapeComponent(float _size) : size(_size)
	{
	}
};

struct SquareShapeComponent
{
	float size;

	SquareShapeComponent(float _size) : size(_size)
	{
	}
};

using TriangleEntityType = ecs::EntityType<PositionComponent, VelocityComponent, TriangleShapeComponent>;
using CircleEntityType = ecs::EntityType<PositionComponent, VelocityComponent, CircleShapeComponent>;
using SquareEntityType = ecs::EntityType<PositionComponent, VelocityComponent, SquareShapeComponent>;

using GameComponents = ecs::ComponentList<PositionComponent, VelocityComponent, TriangleShapeComponent, CircleShapeComponent, SquareShapeComponent>;
using GameEntityTypes = ecs::EntityTypeList<TriangleEntityType, CircleEntityType, SquareEntityType>;

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

	render::System* m_render_pass_system = nullptr;

	render::RenderContext* m_render_context = nullptr;

	//Game constant buffer
	display::ConstantBufferHandle m_game_constant_buffer;

	//Last valid descriptor file
	std::vector<uint8_t> m_render_passes_descriptor_buffer;

	//Buffer used for the render passes text editor
	std::array<char, 1024 * 128> m_text_buffer = { 0 };

	//Display imgui edit descriptor file
	bool m_show_edit_descriptor_file = false;

	//Reload render passes file from the text editor
	bool m_render_system_descriptor_load_requested = false;

	//Show errors in imguid modal window
	bool m_show_errors = false;
	std::vector<std::string> m_render_system_errors;
	std::vector<std::string> m_render_system_context_errors;

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

		//Create render pass system
		m_render_pass_system = render::CreateRenderSystem();

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
			ecs::AllocInstance<GameDatabase, CircleEntityType>()
				.Init<PositionComponent>(rand_position_x(gen), rand_position_y(gen), rand_position_angle(gen))
				.Init<VelocityComponent>(rand_lineal_velocity(gen), rand_lineal_velocity(gen), rand_angle_velocity(gen))
				.Init<CircleShapeComponent>(rand_size(gen));
		}

		for (size_t i = 0; i < 1000; ++i)
		{
			ecs::AllocInstance<GameDatabase, TriangleEntityType>()
				.Init<PositionComponent>(rand_position_x(gen), rand_position_y(gen), rand_position_angle(gen))
				.Init<VelocityComponent>(rand_lineal_velocity(gen), rand_lineal_velocity(gen), rand_angle_velocity(gen))
				.Init<TriangleShapeComponent>(rand_size(gen));
		}

		for (size_t i = 0; i < 1000; ++i)
		{
			ecs::AllocInstance<GameDatabase, SquareEntityType>()
				.Init<PositionComponent>(rand_position_x(gen), rand_position_y(gen), rand_position_angle(gen))
				.Init<VelocityComponent>(rand_lineal_velocity(gen), rand_lineal_velocity(gen), rand_angle_velocity(gen))
				.Init<SquareShapeComponent>(rand_size(gen));
		}
		
	}
	void OnDestroy() override
	{
		if (m_render_pass_system)
		{
			if (m_render_context)
			{
				render::DestroyRenderContext(m_render_pass_system, m_render_context);
			}

			render::DestroyRenderSystem(m_render_pass_system, m_device);
		}

		//Destroy handles
		display::DestroyHandle(m_device, m_game_constant_buffer);

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
				position.position += velocity.lineal_velocity * elapsed_time;
				position.angle += velocity.angle_velocity * elapsed_time;
			}, zone_bitset);

			//Tick database
			ecs::Tick<GameDatabase>();
		}

		//RENDER
		{
			display::BeginFrame(m_device);

			//Recreate the descriptor file and context if requested
			if (m_render_system_descriptor_load_requested)
			{
				//Remove the render context
				if (m_render_context)
				{
					render::DestroyRenderContext(m_render_pass_system, m_render_context);
				}

				//Reset errors
				m_render_system_errors.clear();

				//Load render pass sample
				size_t buffer_size = strlen(m_text_buffer.data()) + 1;

				if (!render::LoadPassDescriptorFile(m_render_pass_system, m_device, m_text_buffer.data(), buffer_size, m_render_system_errors))
				{
					core::LogError("Failed to load the new descriptor file, reverting changes");
					m_show_errors = true;
				}


				//Create pass
				render::ResourceMap init_resource_map;
				init_resource_map["GameGlobal"_sh32] = CreateResourceFromHandle<render::ConstantBufferResource>(display::WeakConstantBufferHandle(m_game_constant_buffer));
				init_resource_map["BackBuffer"_sh32] = CreateResourceFromHandle<render::RenderTargetResource>(display::GetBackBuffer(m_device));

				render::RenderContext::PassInfo pass_info;
				pass_info.width = m_width;
				pass_info.height = m_height;

				//Still load it if it fail, as it will use the last valid one
				m_render_context = render::CreateRenderContext(m_render_pass_system, m_device, "Main"_sh32, pass_info, init_resource_map, m_render_system_context_errors);
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

			if (m_render_context)
			{
				//Capture pass
				render::CaptureRenderContext(m_render_pass_system, m_render_context);
				//Execute pass
				render::ExecuteRenderContext(m_render_pass_system, m_render_context);
			}

			display::EndFrame(m_device);
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