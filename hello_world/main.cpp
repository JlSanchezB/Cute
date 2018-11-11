#include <core/platform.h>
#include <display/display.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif
#include <windows.h>
#include <fstream>

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

	size_t m_width;
	size_t m_height;

	display::Device* m_device;

	display::CommandListHandle m_command_list;

	display::RootSignatureHandle m_root_signature;
	display::PipelineStateHandle m_pipeline_state;
	display::VertexBufferHandle m_vertex_buffer;

	display::ConstantBufferHandle m_constant_buffer;

	display::UnorderedAccessBufferHandle m_unordered_access_buffer;

	display::ShaderResourceHandle m_texture;
	display::RenderTargetHandle m_render_target;
	display::DepthBufferHandle m_depth_buffer;

	display::DescriptorTableHandle m_texture_descriptor_table;
	display::DescriptorTableHandle m_render_target_descriptor_table;
	display::SamplerDescriptorTableHandle m_sampler_descriptor_table;


	void OnInit() override
	{
		display::DeviceInitParams device_init_params;

		device_init_params.debug = true;
		device_init_params.width = kInitWidth;
		device_init_params.height = kInitHeight;
		device_init_params.tearing = true;
		device_init_params.num_frames = 3;

		m_device = display::CreateDevice(device_init_params);

		SetDevice(m_device);

		m_command_list = display::CreateCommandList(m_device);

		//Root signature
		{
			display::RootSignatureDesc root_signature_desc;
			root_signature_desc.num_root_parameters = 1;
			root_signature_desc.root_parameters[0].type = display::RootSignatureParameterType::DescriptorTable;
			root_signature_desc.root_parameters[0].table.num_ranges = 1;
			root_signature_desc.root_parameters[0].table.range[0].base_shader_register = 0;
			root_signature_desc.root_parameters[0].table.range[0].size = 1;
			root_signature_desc.root_parameters[0].table.range[0].type = display::DescriptorTableParameterType::ShaderResource;
			root_signature_desc.root_parameters[0].visibility = display::ShaderVisibility::Pixel;

			root_signature_desc.num_static_samplers = 4;
			//Point Clamp
			root_signature_desc.static_samplers[0].shader_register = 0;
			root_signature_desc.static_samplers[0].visibility = display::ShaderVisibility::Pixel;
			//Linear Clamp
			root_signature_desc.static_samplers[1].shader_register = 1;
			root_signature_desc.static_samplers[1].visibility = display::ShaderVisibility::Pixel;
			root_signature_desc.static_samplers[1].filter = display::Filter::Linear;
			//Point Wrap
			root_signature_desc.static_samplers[2].shader_register = 2;
			root_signature_desc.static_samplers[2].visibility = display::ShaderVisibility::Pixel;
			root_signature_desc.static_samplers[2].address_u = root_signature_desc.static_samplers[2].address_v = display::TextureAddressMode::Wrap;
			//Linear Wrap
			root_signature_desc.static_samplers[3].shader_register = 3;
			root_signature_desc.static_samplers[3].visibility = display::ShaderVisibility::Pixel;
			root_signature_desc.static_samplers[3].address_u = root_signature_desc.static_samplers[3].address_v = display::TextureAddressMode::Wrap;
			root_signature_desc.static_samplers[3].filter = display::Filter::Linear;

			//Create the root signature
			m_root_signature = display::CreateRootSignature(m_device, root_signature_desc);
			
		}

		//Pipeline state
		{
			//Read pixel and vertex shader
			std::vector<char> pixel_shader_buffer = ReadFileToBuffer("texture_shader_ps.fxo");
			std::vector<char> vertex_shader_buffer = ReadFileToBuffer("texture_shader_vs.fxo");

			//Create pipeline state
			display::PipelineStateDesc pipeline_state_desc;
			pipeline_state_desc.root_signature = m_root_signature;

			//Add input layouts
			pipeline_state_desc.input_layout.elements[0] = display::InputElementDesc("POSITION", 0, display::Format::R32G32B32A32_FLOAT, 0, 0);
			pipeline_state_desc.input_layout.elements[1] = display::InputElementDesc("TEXCOORD", 0, display::Format::R32G32_FLOAT, 0, 16);
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
				float tex[2];
			};

			VertexData vertex_data[3] =
			{
				{{-1.f, 1.f, 1.f, 1.f},{0.f, 0.f}},
				{{3.f, 1.f, 1.f, 1.f},{2.f, 0.f}},
				{{-1.f, -3.f, 1.f, 1.f},{0.f, 2.f}}
			};

			display::VertexBufferDesc vertex_buffer_desc;
			vertex_buffer_desc.init_data = vertex_data;
			vertex_buffer_desc.size = sizeof(vertex_data);
			vertex_buffer_desc.stride = sizeof(VertexData);

			m_vertex_buffer = display::CreateVertexBuffer(m_device, vertex_buffer_desc, "fullscreen_quad");
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

			m_texture = display::CreateTextureResource(m_device, reinterpret_cast<void*>(&texture_buffer[0]), texture_buffer.size(), "texture.dds");
		}

		//Descriptor tables
		{
			display::DescriptorTableDesc descriptor_table_desc;
			descriptor_table_desc.AddDescriptor(m_texture);

			m_texture_descriptor_table = display::CreateDescriptorTable(m_device, descriptor_table_desc);

			display::DescriptorTableDesc descriptor_table_render_target_desc;
			descriptor_table_render_target_desc.AddDescriptor(m_render_target);

			m_render_target_descriptor_table = display::CreateDescriptorTable(m_device, descriptor_table_desc);

			display::SamplerDescriptorTableDesc sampler_descriptor_table_desc;
			sampler_descriptor_table_desc.num_descriptors = 4;
			//Point Clamp
			
			//Linear Clamp
			sampler_descriptor_table_desc.descriptors[1].filter = display::Filter::Linear;
			//Point Wrap
			sampler_descriptor_table_desc.descriptors[2].address_u = sampler_descriptor_table_desc.descriptors[2].address_v = display::TextureAddressMode::Wrap;
			//Linear Wrap
			sampler_descriptor_table_desc.descriptors[3].address_u = sampler_descriptor_table_desc.descriptors[3].address_v = display::TextureAddressMode::Wrap;
			sampler_descriptor_table_desc.descriptors[3].filter = display::Filter::Linear;

			m_sampler_descriptor_table = display::CreateSamplerDescriptorTable(m_device, sampler_descriptor_table_desc);
		}
		
		//RenderTarget
		{
			display::RenderTargetDesc render_target_desc;
			render_target_desc.format = display::Format::R8G8B8A8_UNORM;
			render_target_desc.width = 512;
			render_target_desc.heigth = 512;
			
			m_render_target = display::CreateRenderTarget(m_device, render_target_desc, "render target test");

			display::DepthBufferDesc depth_buffer_desc;
			depth_buffer_desc.width = 512;
			depth_buffer_desc.heigth = 512;

			m_depth_buffer = display::CreateDepthBuffer(m_device, depth_buffer_desc);
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
		display::DestroyRenderTarget(m_device, m_render_target);
		display::DestroyDepthBuffer(m_device, m_depth_buffer);
		display::DestroyDescriptorTable(m_device, m_texture_descriptor_table);
		display::DestroyDescriptorTable(m_device, m_render_target_descriptor_table);
		display::DestroySamplerDescriptorTable(m_device, m_sampler_descriptor_table);

		display::DestroyDevice(m_device);
	}
	void OnTick() override
	{

		display::BeginFrame(m_device);

		//Open command list
		display::OpenCommandList(m_device, m_command_list);

		//Set Render Target
		display::WeakRenderTargetHandle render_target = m_render_target;
		display::SetRenderTargets(m_device, m_command_list, 1, &render_target, display::WeakDepthBufferHandle());

		//Clear
		const float clear_colour[] = { 0.f, 0.f, 0.f, 0.f };
		display::ClearRenderTargetColour(m_device, m_command_list, m_render_target, clear_colour);

		//Set root signature
		display::SetRootSignature(m_device, m_command_list, m_root_signature);

		//Set pipeline state
		display::SetPipelineState(m_device, m_command_list, m_pipeline_state);

		//Set viewport
		display::SetViewport(m_device, m_command_list, display::Viewport(static_cast<float>(512 / 2), static_cast<float>(512 / 2)));

		//Set Scissor Rect
		display::SetScissorRect(m_device, m_command_list, display::Rect(0, 0, 512 / 2, 512 / 2));

		//Set vertex buffer
		display::WeakVertexBufferHandle weak_vertex_buffer = m_vertex_buffer;
		display::SetVertexBuffers(m_device, m_command_list, 0, 1, &weak_vertex_buffer);

		//Resource binding
		display::SetDescriptorTable(m_device, m_command_list, 0, m_texture_descriptor_table);

		//Draw
		display::Draw(m_device, m_command_list, 0, 3, display::PrimitiveTopology::TriangleList);

		//Use render target as texture
		display::RenderTargetTransition(m_device, m_command_list, 1, &render_target, display::ResourceState::PixelShaderResource);
		//Set BackBuffer
		display::WeakRenderTargetHandle back_buffer = display::GetBackBuffer(m_device);
		display::SetRenderTargets(m_device, m_command_list, 1, &back_buffer, display::WeakDepthBufferHandle());

		//Resource binding
		display::SetDescriptorTable(m_device, m_command_list, 0, m_render_target_descriptor_table);

		//Set viewport
		display::SetViewport(m_device, m_command_list, display::Viewport(static_cast<float>(m_width / 2), static_cast<float>(m_height / 2)));

		//Set Scissor Rect
		display::SetScissorRect(m_device, m_command_list, display::Rect(0, 0, m_width / 2, m_height / 2));

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

	void OnSizeChange(size_t width, size_t height, bool minimized) override
	{
		m_width = width;
		m_height = height;
	}
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	void* param = reinterpret_cast<void*>(&hInstance);

	HelloWorldGame hello_world_game;

	return platform::Run("Hello world", param, HelloWorldGame::kInitWidth, HelloWorldGame::kInitHeight, &hello_world_game);
}