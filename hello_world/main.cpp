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
	constexpr static uint32_t kInitWidth = 500;
	constexpr static uint32_t kInitHeight = 500;

	uint32_t m_width;
	uint32_t m_height;

	display::Device* m_device;

	//Test 1 (Render a texture to a render target and render to back buffer)
	struct Test1
	{
		display::CommandListHandle m_command_list;

		display::RootSignatureHandle m_root_signature;
		display::PipelineStateHandle m_pipeline_state;
		display::VertexBufferHandle m_vertex_buffer;

		display::ShaderResourceHandle m_texture;
		display::RenderTargetHandle m_render_target;
		display::DepthBufferHandle m_depth_buffer;

		display::DescriptorTableHandle m_texture_descriptor_table;
		display::DescriptorTableHandle m_render_target_descriptor_table;
		display::SamplerDescriptorTableHandle m_sampler_descriptor_table;
	};

	//Test 2 (Render 10 quads using constant buffers)
	struct alignas(16) Test2ConstantBuffer
	{
		float position[4];
		float color[4];
		float size[4];
	};
	struct Test2
	{
		static constexpr size_t kNumQuads = 10;
		display::CommandListHandle m_command_list;

		display::RootSignatureHandle m_root_signature;
		display::PipelineStateHandle m_pipeline_state;
		display::VertexBufferHandle m_vertex_buffer;
		display::IndexBufferHandle m_index_buffer;

		display::ConstantBufferHandle m_constant_buffer[kNumQuads];

		display::DescriptorTableHandle m_constant_descriptor_table[kNumQuads];
	};
	struct Test3
	{
		static constexpr size_t kNumQuads = 10;
		display::CommandListHandle m_command_list;

		display::RootSignatureHandle m_root_signature;
		display::PipelineStateHandle m_pipeline_state;
		display::VertexBufferHandle m_vertex_buffer_instance;
	};
	struct Test4
	{
		static constexpr size_t kNumQuads = 10;
		display::CommandListHandle m_command_list;
		
		display::RootSignatureHandle m_root_signature;
		display::PipelineStateHandle m_pipeline_state;
		display::RootSignatureHandle m_compute_root_signature;
		display::ConstantBufferHandle m_compute_constant_buffer;
		display::DescriptorTableHandle m_compute_constant_descriptor_table;
		display::PipelineStateHandle m_compute_pipeline_state;
	};

	//Tests
	Test1 m_test_1;
	Test2 m_test_2;
	Test3 m_test_3;
	Test4 m_test_4;


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

		m_test_1.m_command_list = display::CreateCommandList(m_device, "Test1");

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
			m_test_1.m_root_signature = display::CreateRootSignature(m_device, root_signature_desc, "Test 1");
			
		}

		//Pipeline state
		{
			//Read pixel and vertex shader
			std::vector<char> pixel_shader_buffer = ReadFileToBuffer("texture_shader_ps.fxo");
			std::vector<char> vertex_shader_buffer = ReadFileToBuffer("texture_shader_vs.fxo");

			//Create pipeline state
			display::PipelineStateDesc pipeline_state_desc;
			pipeline_state_desc.root_signature = m_test_1.m_root_signature;

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
			m_test_1.m_pipeline_state = display::CreatePipelineState(m_device, pipeline_state_desc, "simple texture");
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

			m_test_1.m_vertex_buffer = display::CreateVertexBuffer(m_device, vertex_buffer_desc, "fullscreen_quad");
		}

		//Texture
		{
			std::vector<char> texture_buffer = ReadFileToBuffer("texture.dds");

			m_test_1.m_texture = display::CreateTextureResource(m_device, reinterpret_cast<void*>(&texture_buffer[0]), texture_buffer.size(), "texture.dds");
		}

		//Descriptor tables
		{
			display::DescriptorTableDesc descriptor_table_desc;
			descriptor_table_desc.AddDescriptor(m_test_1.m_texture);

			m_test_1.m_texture_descriptor_table = display::CreateDescriptorTable(m_device, descriptor_table_desc);

			display::DescriptorTableDesc descriptor_table_render_target_desc;
			descriptor_table_render_target_desc.AddDescriptor(m_test_1.m_render_target);

			m_test_1.m_render_target_descriptor_table = display::CreateDescriptorTable(m_device, descriptor_table_desc);

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

			m_test_1.m_sampler_descriptor_table = display::CreateSamplerDescriptorTable(m_device, sampler_descriptor_table_desc);
		}
		
		//RenderTarget
		{
			display::RenderTargetDesc render_target_desc;
			render_target_desc.format = display::Format::R8G8B8A8_UNORM;
			render_target_desc.width = 512;
			render_target_desc.heigth = 512;
			
			m_test_1.m_render_target = display::CreateRenderTarget(m_device, render_target_desc, "render target test");

			display::DepthBufferDesc depth_buffer_desc;
			depth_buffer_desc.width = 512;
			depth_buffer_desc.heigth = 512;

			m_test_1.m_depth_buffer = display::CreateDepthBuffer(m_device, depth_buffer_desc);
		}

		//Test 2

		m_test_2.m_command_list = display::CreateCommandList(m_device, "Test2");

		//Root signature
		{
			display::RootSignatureDesc root_signature_desc;
			root_signature_desc.num_root_parameters = 1;
			root_signature_desc.root_parameters[0].type = display::RootSignatureParameterType::DescriptorTable;
			root_signature_desc.root_parameters[0].table.num_ranges = 1;
			root_signature_desc.root_parameters[0].table.range[0].base_shader_register = 0;
			root_signature_desc.root_parameters[0].table.range[0].size = 1;
			root_signature_desc.root_parameters[0].table.range[0].type = display::DescriptorTableParameterType::ConstantBuffer;
			root_signature_desc.root_parameters[0].visibility = display::ShaderVisibility::All;

			root_signature_desc.num_static_samplers = 0;
			
			//Create the root signature
			m_test_2.m_root_signature = display::CreateRootSignature(m_device, root_signature_desc, "Test 2");

		}

		//Pipeline state
		{
			//Read pixel and vertex shader
			std::vector<char> pixel_shader_buffer = ReadFileToBuffer("constant_buffer_shader_ps.fxo");
			std::vector<char> vertex_shader_buffer = ReadFileToBuffer("constant_buffer_shader_vs.fxo");

			//Create pipeline state
			display::PipelineStateDesc pipeline_state_desc;
			pipeline_state_desc.root_signature = m_test_2.m_root_signature;

			//Add input layouts
			pipeline_state_desc.input_layout.elements[0] = display::InputElementDesc("POSITION", 0, display::Format::R32G32B32A32_FLOAT, 0, 0);
			pipeline_state_desc.input_layout.num_elements = 1;


			//Add shaders
			pipeline_state_desc.pixel_shader.data = reinterpret_cast<void*>(pixel_shader_buffer.data());
			pipeline_state_desc.pixel_shader.size = pixel_shader_buffer.size();

			pipeline_state_desc.vertex_shader.data = reinterpret_cast<void*>(vertex_shader_buffer.data());
			pipeline_state_desc.vertex_shader.size = vertex_shader_buffer.size();

			//Add render targets
			pipeline_state_desc.num_render_targets = 1;
			pipeline_state_desc.render_target_format[0] = display::Format::R8G8B8A8_UNORM;

			//Create
			m_test_2.m_pipeline_state = display::CreatePipelineState(m_device, pipeline_state_desc, "constant buffer driven quad");
		}
		//Vertex buffer
		{
			struct VertexData
			{
				float position[4];
			};

			VertexData vertex_data[4] =
			{
				{1.f, 1.f, 1.f, 1.f},
				{-1.f, 1.f, 1.f, 1.f},
				{1.f, -1.f, 1.f, 1.f},
				{-1.f, -1.f, 1.f, 1.f}
			};

			display::VertexBufferDesc vertex_buffer_desc;
			vertex_buffer_desc.init_data = vertex_data;
			vertex_buffer_desc.size = sizeof(vertex_data);
			vertex_buffer_desc.stride = sizeof(VertexData);

			m_test_2.m_vertex_buffer = display::CreateVertexBuffer(m_device, vertex_buffer_desc, "quad");
		}

		//Index buffer
		{
			uint16_t index_buffer_data[6] = { 0, 2, 1, 1, 2, 3 };
			display::IndexBufferDesc index_buffer_desc;
			index_buffer_desc.init_data = index_buffer_data;
			index_buffer_desc.size = sizeof(index_buffer_data);

			m_test_2.m_index_buffer = display::CreateIndexBuffer(m_device, index_buffer_desc, "quad_index_buffer");
		}

		//Constant buffer
		{
			Test2ConstantBuffer constant_buffer = {};
			constant_buffer.color[0] = constant_buffer.color[1] = constant_buffer.color[2] = constant_buffer.color[3] = 1.f;
			constant_buffer.size[0] = 0.1f;
			display::ConstantBufferDesc constant_buffer_desc;
			constant_buffer_desc.access = display::Access::Dynamic;
			constant_buffer_desc.init_data = &constant_buffer;
			constant_buffer_desc.size = sizeof(Test2ConstantBuffer);
			for (size_t i = 0; i < Test2::kNumQuads; ++i)
			{
				m_test_2.m_constant_buffer[i] = display::CreateConstantBuffer(m_device, constant_buffer_desc);
			}
		}

		//Descriptor tables
		{
			for (size_t i = 0; i < Test2::kNumQuads; ++i)
			{
				display::DescriptorTableDesc descriptor_table_desc;
				descriptor_table_desc.access = display::Access::Dynamic;
				descriptor_table_desc.AddDescriptor(m_test_2.m_constant_buffer[i]);

				m_test_2.m_constant_descriptor_table[i] = display::CreateDescriptorTable(m_device, descriptor_table_desc);
			}
		}

		//Test 3

		m_test_3.m_command_list = display::CreateCommandList(m_device, "Test3");

		//Root signature
		{
			display::RootSignatureDesc root_signature_desc;
			root_signature_desc.num_root_parameters = 0;
			root_signature_desc.num_static_samplers = 0;

			//Create the root signature
			m_test_3.m_root_signature = display::CreateRootSignature(m_device, root_signature_desc, "Test 3");

		}

		//Pipeline state
		{
			//Read pixel and vertex shader
			std::vector<char> pixel_shader_buffer = ReadFileToBuffer("instance_shader_ps.fxo");
			std::vector<char> vertex_shader_buffer = ReadFileToBuffer("instance_shader_vs.fxo");

			//Create pipeline state
			display::PipelineStateDesc pipeline_state_desc;
			pipeline_state_desc.root_signature = m_test_3.m_root_signature;

			//Add input layouts
			pipeline_state_desc.input_layout.elements[0] = display::InputElementDesc("POSITION", 0, display::Format::R32G32B32A32_FLOAT, 0, 0);
			pipeline_state_desc.input_layout.elements[1] = display::InputElementDesc("TEXCOORD", 0, display::Format::R32G32B32A32_FLOAT, 1, 0, display::InputType::Instance);
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
			m_test_3.m_pipeline_state = display::CreatePipelineState(m_device, pipeline_state_desc, "instance driven quad");
		}
		//Vertex buffer
		{
			struct VertexData
			{
				float position[4];
			};

			VertexData vertex_data[Test3::kNumQuads] = {};
	
			display::VertexBufferDesc vertex_buffer_desc;
			vertex_buffer_desc.access = display::Access::Dynamic;
			vertex_buffer_desc.init_data = vertex_data;
			vertex_buffer_desc.size = sizeof(vertex_data);
			vertex_buffer_desc.stride = sizeof(VertexData);

			m_test_3.m_vertex_buffer_instance = display::CreateVertexBuffer(m_device, vertex_buffer_desc, "instance");
		}

		//Test4
		m_test_4.m_command_list = display::CreateCommandList(m_device, "Test4");
		
		//Root signature
		{
			display::RootSignatureDesc root_signature_desc;
			root_signature_desc.num_root_parameters = 1;
			root_signature_desc.root_parameters[0].type = display::RootSignatureParameterType::DescriptorTable;
			root_signature_desc.root_parameters[0].table.num_ranges = 1;
			root_signature_desc.root_parameters[0].table.range[0].base_shader_register = 0;
			root_signature_desc.root_parameters[0].table.range[0].size = 1;
			root_signature_desc.root_parameters[0].table.range[0].type = display::DescriptorTableParameterType::ShaderResource;
			root_signature_desc.root_parameters[0].visibility = display::ShaderVisibility::All;

			root_signature_desc.num_static_samplers = 0;

			//Create the root signature
			m_test_4.m_root_signature = display::CreateRootSignature(m_device, root_signature_desc, "Test 4");

		}

		{
			//Compute pipeline, compile shader for testing
			const char* shader_code =
				"StructuredBuffer<float4> compute_params: t0;\
				struct PSInput\
				{\
					float4 position : SV_POSITION;\
					float4 color : COLOR;\
				};\
				\
				PSInput main_vs(float4 position : POSITION, uint instance_id : SV_InstanceID)\
				{\
					PSInput result; \
					float4 instance_data = compute_params[instance_id];\
					result.position.xy = position.xy * instance_data.z + instance_data.xy;\
					result.position.zw = position.zw;\
					result.color = instance_data.wwww;\
					return result;\
				}\
				float4 main_ps(PSInput input) : SV_TARGET\
				{\
					return input.color;\
				}";

			std::vector<char> vertex_shader;
			std::vector<char> pixel_shader;

			display::CompileShaderDesc compile_shader_desc;
			compile_shader_desc.code = shader_code;
			compile_shader_desc.entry_point = "main_vs";
			compile_shader_desc.target = "vs_5_0";
			display::CompileShader(m_device, compile_shader_desc, vertex_shader);

			compile_shader_desc.code = shader_code;
			compile_shader_desc.entry_point = "main_ps";
			compile_shader_desc.target = "ps_5_0";
			display::CompileShader(m_device, compile_shader_desc, pixel_shader);


			//Create pipeline state
			display::PipelineStateDesc pipeline_state_desc;
			pipeline_state_desc.root_signature = m_test_4.m_root_signature;

			//Add input layouts
			pipeline_state_desc.input_layout.elements[0] = display::InputElementDesc("POSITION", 0, display::Format::R32G32B32A32_FLOAT, 0, 0);
			pipeline_state_desc.input_layout.num_elements = 1;


			//Add shaders
			pipeline_state_desc.pixel_shader.data = reinterpret_cast<void*>(pixel_shader.data());
			pipeline_state_desc.pixel_shader.size = pixel_shader.size();

			pipeline_state_desc.vertex_shader.data = reinterpret_cast<void*>(vertex_shader.data());
			pipeline_state_desc.vertex_shader.size = vertex_shader.size();

			//Add render targets
			pipeline_state_desc.num_render_targets = 1;
			pipeline_state_desc.render_target_format[0] = display::Format::R8G8B8A8_UNORM;

			//Create
			m_test_4.m_pipeline_state = display::CreatePipelineState(m_device, pipeline_state_desc, "compute driven quad");
		}

		{
			//Create compute root signature
			display::RootSignatureDesc root_signature_desc;
			root_signature_desc.num_root_parameters = 1;
			root_signature_desc.root_parameters[0].type = display::RootSignatureParameterType::ConstantBuffer;
			root_signature_desc.root_parameters[0].root_param.shader_register = 0;
			root_signature_desc.root_parameters[0].visibility = display::ShaderVisibility::All;

			root_signature_desc.num_static_samplers = 0;

			//Create the root signature
			m_test_4.m_compute_root_signature = display::CreateRootSignature(m_device, root_signature_desc, "Test 4 Compute");
		}

	}
	void OnDestroy() override
	{
		//Free handles
		display::DestroyCommandList(m_device, m_test_1.m_command_list);
		display::DestroyRootSignature(m_device, m_test_1.m_root_signature);
		display::DestroyPipelineState(m_device, m_test_1.m_pipeline_state);
		display::DestroyVertexBuffer(m_device, m_test_1.m_vertex_buffer);
		display::DestroyShaderResource(m_device, m_test_1.m_texture);
		display::DestroyRenderTarget(m_device, m_test_1.m_render_target);
		display::DestroyDepthBuffer(m_device, m_test_1.m_depth_buffer);
		display::DestroyDescriptorTable(m_device, m_test_1.m_texture_descriptor_table);
		display::DestroyDescriptorTable(m_device, m_test_1.m_render_target_descriptor_table);
		display::DestroySamplerDescriptorTable(m_device, m_test_1.m_sampler_descriptor_table);

		display::DestroyCommandList(m_device, m_test_2.m_command_list);
		display::DestroyRootSignature(m_device, m_test_2.m_root_signature);
		display::DestroyPipelineState(m_device, m_test_2.m_pipeline_state);
		display::DestroyVertexBuffer(m_device, m_test_2.m_vertex_buffer);
		display::DestroyIndexBuffer(m_device, m_test_2.m_index_buffer);
		for (size_t i = 0; i < Test2::kNumQuads; ++i)
		{
			display::DestroyConstantBuffer(m_device, m_test_2.m_constant_buffer[i]);
			display::DestroyDescriptorTable(m_device, m_test_2.m_constant_descriptor_table[i]);
		}

		display::DestroyCommandList(m_device, m_test_3.m_command_list);
		display::DestroyRootSignature(m_device, m_test_3.m_root_signature);
		display::DestroyPipelineState(m_device, m_test_3.m_pipeline_state);
		display::DestroyVertexBuffer(m_device, m_test_3.m_vertex_buffer_instance);

		display::DestroyCommandList(m_device, m_test_4.m_command_list);
		display::DestroyRootSignature(m_device, m_test_4.m_root_signature);
		display::DestroyPipelineState(m_device, m_test_4.m_pipeline_state);
		display::DestroyRootSignature(m_device, m_test_4.m_compute_root_signature);

		display::DestroyDevice(m_device);
	}
	void OnTick(double total_time, float elapsed_time) override
	{

		display::BeginFrame(m_device);

		//Test 1
		{
			//Open command list
			display::Context* context = display::OpenCommandList(m_device, m_test_1.m_command_list);

			//Set Render Target
			context->SetRenderTargets(1, &m_test_1.m_render_target, display::WeakDepthBufferHandle());

			//Clear
			const float clear_colour[] = { 0.f, 0.f, 0.f, 0.f };
			context->ClearRenderTargetColour( m_test_1.m_render_target, clear_colour);

			//Set root signature
			context->SetRootSignature(display::Pipe::Graphics, m_test_1.m_root_signature);

			//Set pipeline state
			context->SetPipelineState(m_test_1.m_pipeline_state);

			//Set viewport
			context->SetViewport(display::Viewport(static_cast<float>(512 / 2), static_cast<float>(512 / 2)));

			//Set Scissor Rect
			context->SetScissorRect(display::Rect(0, 0, 512 / 2, 512 / 2));

			//Set vertex buffer
			context->SetVertexBuffers(0, 1, &m_test_1.m_vertex_buffer);

			//Resource binding
			context->SetDescriptorTable(display::Pipe::Graphics, 0, m_test_1.m_texture_descriptor_table);

			//Draw
			display::DrawDesc draw_desc;
			draw_desc.vertex_count = 3;
			context->Draw(draw_desc);

			//Use render target as texture
			context->RenderTargetTransition(1, &m_test_1.m_render_target, display::ResourceState::PixelShaderResource);
			//Set BackBuffer
			context->SetRenderTargets(1, &display::GetBackBuffer(m_device), display::WeakDepthBufferHandle());

			context->ClearRenderTargetColour(display::GetBackBuffer(m_device), clear_colour);

			//Set viewport
			context->SetViewport(display::Viewport(static_cast<float>(m_width / 2), static_cast<float>(m_height / 2)));

			//Set Scissor Rect
			context->SetScissorRect(display::Rect(0, 0, m_width, m_height));

			//Resource binding
			context->SetDescriptorTable(display::Pipe::Graphics, 0, m_test_1.m_render_target_descriptor_table);

			//Draw
			context->Draw(draw_desc);


			//Close command list
			display::CloseCommandList(m_device, context);
		}

		//Test 2
		{
			//Open command list
			display::Context* context = display::OpenCommandList(m_device, m_test_2.m_command_list);

			//Set Render Target
			context->SetRenderTargets(1, &display::GetBackBuffer(m_device), display::WeakDepthBufferHandle());

			//Set viewport
			display::Viewport viewport(static_cast<float>(m_width / 2), static_cast<float>(m_height / 2));
			viewport.top_left_x = 0;
			viewport.top_left_y = static_cast<float>(m_height / 2);
			context->SetViewport(viewport);

			//Set Scissor Rect
			context->SetScissorRect(display::Rect(0, 0, m_width, m_height));

			//Set root signature
			context->SetRootSignature(display::Pipe::Graphics, m_test_2.m_root_signature);

			//Set pipeline state
			context->SetPipelineState(m_test_2.m_pipeline_state);

			//Set vertex buffer
			context->SetVertexBuffers(0, 1, &m_test_2.m_vertex_buffer);

			//Set index buffer
			context->SetIndexBuffer(m_test_2.m_index_buffer);

			for (size_t i = 0; i < Test2::kNumQuads; ++i)
			{
				Test2ConstantBuffer constant_buffer = {};
				float quad = static_cast<float>(i) / Test2::kNumQuads;
				float angle = static_cast<float>(total_time + 3.f * quad);
				constant_buffer.position[0] = 0.5f * cosf(angle);
				constant_buffer.position[1] = 0.5f * sinf(angle);
				constant_buffer.color[0] = constant_buffer.color[1] = constant_buffer.color[2] = constant_buffer.color[3] = 0.5f + 0.5f * quad;
				constant_buffer.size[0] = 0.01f + 0.02f * quad;

				//Update constant buffer
				display::UpdateResourceBuffer(m_device, m_test_2.m_constant_buffer[i], &constant_buffer, sizeof(constant_buffer));
				//Resource binding
				context->SetDescriptorTable(display::Pipe::Graphics, 0, m_test_2.m_constant_descriptor_table[i]);

				//Draw
				display::DrawIndexedDesc draw_desc;
				draw_desc.index_count = 6;
				context->DrawIndexed( draw_desc);
			}

			//Close command list
			display::CloseCommandList(m_device, context);
		}

		{
			//Open command list
			display::Context* context = display::OpenCommandList(m_device, m_test_3.m_command_list);

			//Set Render Target
			context->SetRenderTargets(1, &display::GetBackBuffer(m_device), display::WeakDepthBufferHandle());

			//Set viewport
			display::Viewport viewport(static_cast<float>(m_width / 2), static_cast<float>(m_height / 2));
			viewport.top_left_x = static_cast<float>(m_width / 2);
			viewport.top_left_y = 0;
			context->SetViewport(viewport);

			//Set Scissor Rect
			context->SetScissorRect(display::Rect(0, 0, m_width, m_height));

			//Set root signature
			context->SetRootSignature(display::Pipe::Graphics, m_test_3.m_root_signature);

			//Set pipeline state
			context->SetPipelineState(m_test_3.m_pipeline_state);

			//Set vertex buffer
			context->SetVertexBuffers(0, 1, &m_test_2.m_vertex_buffer);

			//Set instance buffer
			context->SetVertexBuffers(1, 1, &m_test_3.m_vertex_buffer_instance);

			//Set index buffer
			context->SetIndexBuffer(m_test_2.m_index_buffer);

			struct InstanceBuffer
			{
				float data[4];
			};

			std::array<InstanceBuffer, Test3::kNumQuads> instance_buffer;

			for (size_t i = 0; i < Test3::kNumQuads; ++i)
			{
				auto& instance = instance_buffer[i];
				float quad = static_cast<float>(i) / Test3::kNumQuads;
				float angle = static_cast<float>(total_time + 3.f * quad);
				instance.data[0] = 0.5f * cosf(angle);
				instance.data[1] = 0.5f * sinf(angle);
				instance.data[2] = 0.01f + 0.02f * quad;
				instance.data[3] = 0.5f + 0.5f * quad;
			}

			//Update vertex buffer
			display::UpdateResourceBuffer(m_device, m_test_3.m_vertex_buffer_instance, &instance_buffer[0], sizeof(instance_buffer));

			//Draw
			display::DrawIndexedInstancedDesc draw_desc;
			draw_desc.index_count = 6;
			draw_desc.instance_count = Test3::kNumQuads;
			context->DrawIndexedInstanced(draw_desc);

			//Close command list
			display::CloseCommandList(m_device, context);
		}

		//Execute command list
		display::ExecuteCommandList(m_device, m_test_1.m_command_list);
		display::ExecuteCommandList(m_device, m_test_2.m_command_list);
		display::ExecuteCommandList(m_device, m_test_3.m_command_list);

		display::EndFrame(m_device);
	}

	void OnSizeChange(uint32_t width, uint32_t height, bool minimized) override
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