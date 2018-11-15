#include "imgui_render.h"
#include "imgui/imgui.h"


namespace
{
	//Resources for rendering imgui
	display::RootSignatureHandle g_rootsignature;
	display::PipelineStateHandle g_pipeline_state;
	display::ShaderResourceHandle g_texture;
	display::ConstantBufferHandle g_constant_buffer;
	display::VertexBufferHandle g_vertex_buffer;
	size_t current_vertex_buffer_size = 4000;
	size_t current_index_buffer_size = 4000;
	display::IndexBufferHandle g_index_buffer;
	display::DescriptorTableHandle g_descriptor_table;

	struct ConstantBuffer
	{
		float projection_matrix_0[4];
		float projection_matrix_1[4];
		float projection_matrix_2[4];
		float projection_matrix_3[4];
	};
}

void imgui_render::CreateResources(display::Device * device)
{
	//Create root signature
	display::RootSignatureDesc rootsignature_desc;
	rootsignature_desc.num_root_parameters = 2;
	rootsignature_desc.root_parameters[0].type = display::RootSignatureParameterType::ConstantBuffer;
	rootsignature_desc.root_parameters[0].visibility = display::ShaderVisibility::Vertex;
	rootsignature_desc.root_parameters[0].root_param.shader_register = 0;
	rootsignature_desc.root_parameters[1].type = display::RootSignatureParameterType::DescriptorTable;
	rootsignature_desc.root_parameters[1].visibility = display::ShaderVisibility::Pixel;
	rootsignature_desc.root_parameters[1].root_param.shader_register = 0;
	rootsignature_desc.num_static_samplers = 1;
	rootsignature_desc.static_samplers[0].address_u = rootsignature_desc.static_samplers[0].address_v = rootsignature_desc.static_samplers[0].address_w = display::TextureAddressMode::Wrap;
	rootsignature_desc.static_samplers[0].filter = display::Filter::Linear;
	g_rootsignature = display::CreateRootSignature(device, rootsignature_desc, "imguid");

	//Compile shaders
	static const char* vertex_shader_code =
		"cbuffer vertexBuffer : register(b0) \
            {\
              float4x4 ProjectionMatrix; \
            };\
            struct VS_INPUT\
            {\
              float2 pos : POSITION;\
              float4 col : COLOR0;\
              float2 uv  : TEXCOORD0;\
            };\
            \
            struct PS_INPUT\
            {\
              float4 pos : SV_POSITION;\
              float4 col : COLOR0;\
              float2 uv  : TEXCOORD0;\
            };\
            \
            PS_INPUT main(VS_INPUT input)\
            {\
              PS_INPUT output;\
              output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\
              output.col = input.col;\
              output.uv  = input.uv;\
              return output;\
            }";

	static const char* pixel_shader_code =
		"struct PS_INPUT\
            {\
              float4 pos : SV_POSITION;\
              float4 col : COLOR0;\
              float2 uv  : TEXCOORD0;\
            };\
            SamplerState sampler0 : register(s0);\
            Texture2D texture0 : register(t0);\
            \
            float4 main(PS_INPUT input) : SV_Target\
            {\
              float4 out_col = input.col * texture0.Sample(sampler0, input.uv); \
              return out_col; \
            }";

	std::vector<char> vertex_shader;
	std::vector<char> pixel_shader;

	display::CompileShaderDesc compile_shader_desc;
	compile_shader_desc.code = vertex_shader_code;
	compile_shader_desc.entry_point= "main";
	compile_shader_desc.target = "vs_5_0";
	display::CompileShader(device, compile_shader_desc, vertex_shader);

	compile_shader_desc.code = pixel_shader_code;
	compile_shader_desc.target = "ps_5_0";
	display::CompileShader(device, compile_shader_desc, pixel_shader);

	//Create pipeline state
	display::PipelineStateDesc pipeline_state_desc;
	pipeline_state_desc.root_signature = g_rootsignature;

	//Add input layouts
	pipeline_state_desc.input_layout.elements[0] = display::InputElementDesc("POSITION", 0, display::Format::R32G32_FLOAT, 0, 0);
	pipeline_state_desc.input_layout.elements[1] = display::InputElementDesc("TEXCOORD", 0, display::Format::R32G32_FLOAT, 0, 8);
	pipeline_state_desc.input_layout.elements[2] = display::InputElementDesc("COLOR", 0, display::Format::R8G8B8A8_UNORM, 0, 16);
	pipeline_state_desc.input_layout.num_elements = 3;

	pipeline_state_desc.pixel_shader.data = reinterpret_cast<void*>(pixel_shader.data());
	pipeline_state_desc.pixel_shader.size = pixel_shader.size();

	pipeline_state_desc.vertex_shader.data = reinterpret_cast<void*>(vertex_shader.data());
	pipeline_state_desc.vertex_shader.size = vertex_shader.size();

	pipeline_state_desc.blend_desc.render_target_blend[0].blend_enable = true;
	pipeline_state_desc.blend_desc.render_target_blend[0].src_blend = display::Blend::SrcAlpha;
	pipeline_state_desc.blend_desc.render_target_blend[0].dest_blend = display::Blend::InvSrcAlpha;
	pipeline_state_desc.blend_desc.render_target_blend[0].blend_op = display::BlendOp::Add;

	pipeline_state_desc.num_render_targets = 1;
	pipeline_state_desc.render_target_format[0] = display::Format::R8G8B8A8_UNORM;

	//Create
	g_pipeline_state = display::CreatePipelineState(device, pipeline_state_desc, "imgui");

	//Create constant buffer
	ConstantBuffer constant_buffer;
	display::ConstantBufferDesc constant_buffer_desc;
	constant_buffer_desc.access = display::Access::Dynamic;
	constant_buffer_desc.init_data = &constant_buffer;
	constant_buffer_desc.size = sizeof(constant_buffer);
	g_constant_buffer = display::CreateConstantBuffer(device, constant_buffer_desc, "imgui");

	//Create texture
	ImGuiIO& io = ImGui::GetIO();
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	display::ShaderResourceDesc shader_resource_desc;
	shader_resource_desc.width = width;
	shader_resource_desc.height = height;
	shader_resource_desc.pitch = 4 * width;
	shader_resource_desc.init_data = pixels;
	g_texture = display::CreateShaderResource(device, shader_resource_desc, "imgui");

	//Create Vertex buffer (inited in some size and it will grow by demand)
	display::VertexBufferDesc vertex_buffer_desc;
	vertex_buffer_desc.access = display::Access::Dynamic;
	vertex_buffer_desc.size = current_vertex_buffer_size * 20;
	vertex_buffer_desc.stride = 20;
	g_vertex_buffer = display::CreateVertexBuffer(device, vertex_buffer_desc, "imgui");

	//Create Index buffer
	//display::IndexBufferDesc index_buffer_desc;
	//index_buffer_desc.access = display::Access::Dynamic;
	//index_buffer_desc.size = current_index_buffer_size * 2;
	//g_index_buffer = display::CreateIndexBuffer(device, index_buffer_desc, "imgui");

	//Descritor table
}

void imgui_render::DestroyResources(display::Device * device)
{
}

void imgui_render::Draw(display::Device * device, const display::CommandListHandle & command_list_handle)
{
}
