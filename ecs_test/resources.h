//////////////////////////////////////////////////////////////////////////
// Cute engine - resources for the ECS test
//////////////////////////////////////////////////////////////////////////
#ifndef ECS_RESOURCES_h
#define ECS_RESOURCES_h

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

#endif