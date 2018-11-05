#include <core/platform.h>
#include <display/display.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif
#include <windows.h>
#include <fstream>
#include "dds.h"
namespace
{
	std::vector<char> ReadFileToBuffer(const char* file)
	{
		std::ifstream root_signature_file(file, std::ios::binary | std::ios::ate);
		if (root_signature_file.good())
		{
			std::streamsize size = root_signature_file.tellg();
			root_signature_file.seekg(0, std::ios::beg);

			std::vector<char> buffer(size);
			root_signature_file.read(buffer.data(), size);

			return buffer;
		}
		else
		{
			return std::vector<char>(0);
		}
	}
}

class HelloWorldGame : public platform::Game
{
public:
	const static size_t kInitWidth = 500;
	const static size_t kInitHeight = 500;

	display::Device* m_device;

	display::CommandListHandle m_command_list;

	display::RootSignatureHandle m_root_signature;
	display::PipelineStateHandle m_pipeline_state;
	display::VertexBufferHandle m_vertex_buffer;

	display::ConstantBufferHandle m_constant_buffer;

	display::UnorderedAccessBufferHandle m_unordered_access_buffer;

	display::ShaderResourceHandle m_texture;


	void OnInit() override
	{
		display::DeviceInitParams device_init_params;

		device_init_params.debug = true;
		device_init_params.width = kInitWidth;
		device_init_params.height = kInitHeight;
		device_init_params.num_frames = 2;

		m_device = display::CreateDevice(device_init_params);

		m_command_list = display::CreateCommandList(m_device);

		//Root signature
		{
			display::RootSignatureDesc root_signature_desc;
			//Create the root signature
			m_root_signature = display::CreateRootSignature(m_device, root_signature_desc);
			
		}

		//Pipeline state
		{
			//Read pixel and vertex shader
			std::vector<char> pixel_shader_buffer = ReadFileToBuffer("colour_shader_ps.fxo");
			std::vector<char> vertex_shader_buffer = ReadFileToBuffer("colour_shader_vs.fxo");

			//Create pipeline state
			display::PipelineStateDesc pipeline_state_desc;
			pipeline_state_desc.root_signature = m_root_signature;

			//Add input layouts
			pipeline_state_desc.input_layout.elements[0] = display::InputElementDesc("POSITION", 0, display::Format::R32G32B32A32_FLOAT, 0, 0);
			pipeline_state_desc.input_layout.elements[1] = display::InputElementDesc("COLOR", 0, display::Format::R32G32B32A32_FLOAT, 0, 16);
			pipeline_state_desc.input_layout.num_elements = 2;


			//Add shaders
			pipeline_state_desc.pixel_shader.data = reinterpret_cast<void*>(pixel_shader_buffer.data());
			pipeline_state_desc.pixel_shader.size = pixel_shader_buffer.size();

			pipeline_state_desc.vertex_shader.data = reinterpret_cast<void*>(vertex_shader_buffer.data());
			pipeline_state_desc.vertex_shader.size = vertex_shader_buffer.size();

			//Add render targets
			pipeline_state_desc.num_render_targets = 1;
			pipeline_state_desc.render_target_format[0] = display::Format::R8G8B8A8_UNORM;

			//Create
			m_pipeline_state = display::CreatePipelineState(m_device, pipeline_state_desc);
		}

		//Vertex buffer
		{
			struct VertexData
			{
				float position[4];
				float colour[4];
			};

			VertexData vertex_data[3] =
			{
				{{-1.f, 1.f, 1.f, 1.f},{1.f, 0.f, 0.f, 0.f}},
				{{3.f, 1.f, 1.f, 1.f},{0.f, 1.f, 0.f, 0.f}},
				{{-1.f, -3.f, 1.f, 1.f},{0.f, 0.f, 1.f, 0.f}}
			};

			display::VertexBufferDesc vertex_buffer_desc;
			vertex_buffer_desc.init_data = vertex_data;
			vertex_buffer_desc.size = sizeof(vertex_data);
			vertex_buffer_desc.stride = sizeof(VertexData);

			m_vertex_buffer = display::CreateVertexBuffer(m_device, vertex_buffer_desc);
		}

		//Constant buffer
		{
			struct SomeData
			{
				std::array<float, 32> data;
			};

			SomeData data;

			display::ConstantBufferDesc constant_buffer_desc;
			constant_buffer_desc.access = display::Access::Dynamic;
			constant_buffer_desc.init_data = &data;
			constant_buffer_desc.size = sizeof(data);
			m_constant_buffer = display::CreateConstantBuffer(m_device, constant_buffer_desc);
		}

		//Unordered access buffer
		{
			struct SomeElement
			{
				std::array<float, 16> data;
			};

			display::UnorderedAccessBufferDesc unordered_access_buffer_desc;
			unordered_access_buffer_desc.element_size = sizeof(SomeElement);
			unordered_access_buffer_desc.element_count = 32;

			m_unordered_access_buffer = display::CreateUnorderedAccessBuffer(m_device, unordered_access_buffer_desc);
		}

		//Texture
		{
			std::vector<char> texture_buffer = ReadFileToBuffer("texture.dds");

			//Read header
			DDS_HEADER* dds_head = reinterpret_cast<DDS_HEADER*>(&texture_buffer[0]);

			if (dds_head->dwMagic == DDS_MAGIC_NUMBER)
			{
				display::ShaderResourceDesc shader_resource_desc;

				shader_resource_desc.format = display::Format::R8G8B8A8_UNORM_SRGB;
				shader_resource_desc.width = dds_head->dwWidth;
				shader_resource_desc.heigth = dds_head->dwHeight;
				shader_resource_desc.mips = dds_head->dwMipMapCount;
				shader_resource_desc.init_data = reinterpret_cast<void*>(&texture_buffer[sizeof(DDS_HEADER)]);
				shader_resource_desc.slice_pitch = shader_resource_desc.size = texture_buffer.size() - sizeof(DDS_HEADER);

				m_texture = display::CreateShaderResource(m_device, shader_resource_desc);
			}
		}
		
	}
	void OnDestroy() override
	{
		//Free handles
		display::DestroyCommandList(m_device, m_command_list);
		display::DestroyRootSignature(m_device, m_root_signature);
		display::DestroyPipelineState(m_device, m_pipeline_state);
		display::DestroyVertexBuffer(m_device, m_vertex_buffer);
		display::DestroyConstantBuffer(m_device, m_constant_buffer);
		display::DestroyUnorderedAccessBuffer(m_device, m_unordered_access_buffer);
		display::DestroyShaderResource(m_device, m_texture);

		display::DestroyDevice(m_device);
	}
	void OnTick() override
	{

		display::BeginFrame(m_device);

		//Open command list
		display::OpenCommandList(m_device, m_command_list);

		//Set BackBuffer
		display::WeakRenderTargetHandle back_buffer = display::GetBackBuffer(m_device);
		display::SetRenderTargets(m_device, m_command_list, 1, &back_buffer, nullptr);

		//Clear
		const float clear_colour[] = { rand() / (RAND_MAX + 1.f) , 0.2f, 0.4f, 1.0f };
		display::ClearRenderTargetColour(m_device, m_command_list, back_buffer, clear_colour);

		//Set root signature
		display::SetRootSignature(m_device, m_command_list, m_root_signature);

		//Set viewport
		display::SetViewport(m_device, m_command_list, display::Viewport(500.f, 500.f));

		//Set Scissor Rect
		display::SetScissorRect(m_device, m_command_list, display::Rect(0, 0, 500, 500));

		//Set pipeline state
		display::SetPipelineState(m_device, m_command_list, m_pipeline_state);

		//Set vertex buffer
		display::WeakVertexBufferHandle weak_vertex_buffer = m_vertex_buffer;
		display::SetVertexBuffers(m_device, m_command_list, 0, 1, &weak_vertex_buffer);

		//Draw
		display::Draw(m_device, m_command_list, 0, 3, display::PrimitiveTopology::TriangleList);

		//Close command list
		display::CloseCommandList(m_device, m_command_list);

		//Execute command list
		display::ExecuteCommandList(m_device, m_command_list);

		//Present
		display::Present(m_device);

		display::EndFrame(m_device);
	}
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	void* param = reinterpret_cast<void*>(&hInstance);

	HelloWorldGame hello_world_game;

	return platform::Run("Hello world", param, HelloWorldGame::kInitWidth, HelloWorldGame::kInitHeight, &hello_world_game);
}