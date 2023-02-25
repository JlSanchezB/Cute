//////////////////////////////////////////////////////////////////////////
// Cute engine - resources for the ECS test
//////////////////////////////////////////////////////////////////////////
#ifndef ECS_RESOURCES_h
#define ECS_RESOURCES_h

struct DisplayResource
{
	display::BufferHandle m_quad_vertex_buffer;
	display::BufferHandle m_quad_index_buffer;
	display::RootSignatureHandle m_root_signature;
	display::PipelineStateHandle m_grass_pipeline_state;
	display::PipelineStateHandle m_gazelle_pipeline_state;
	display::PipelineStateHandle m_lion_pipeline_state;
	display::BufferHandle m_zoom_position;

	void Load(display::Device* device)
	{
		//Create root signature
		display::RootSignatureDesc root_signature_desc;
		root_signature_desc.root_parameters[0].type = display::RootSignatureParameterType::ConstantBuffer;
		root_signature_desc.root_parameters[0].visibility = display::ShaderVisibility::Vertex;
		root_signature_desc.root_parameters[0].root_param.shader_register = 0;
		root_signature_desc.num_root_parameters = 1;

		m_root_signature = display::CreateRootSignature(device, root_signature_desc, "Root Signature");

		//Create pipeline state
		const char* shader_code =
			"	float4 zoom_position : register(b0);\
				struct PSInput\
				{\
					float4 position : SV_POSITION;\
					float2 coords : TEXCOORD0; \
				};\
				\
				PSInput main_vs(float2 position : POSITION, float4 instance_data : TEXCOORD)\
				{\
					PSInput result; \
					result.position.xy = position.xy * instance_data.w + instance_data.xy; \
					result.position.xy = (result.position.xy - zoom_position.zw) * zoom_position.xy; \
					result.position.zw = float2(0.f, 1.f); \
					result.coords.xy = position.xy; \
					return result;\
				}\
				float4 main_ps(PSInput input) : SV_TARGET\
				{\
					float alpha = smoothstep(1.f, 0.75f, length(input.coords.xy));\
					return float4(0.f, alpha, 0.f, alpha);\
				}";

	
		//Create pipeline state
		display::PipelineStateDesc pipeline_state_desc;
		pipeline_state_desc.root_signature = m_root_signature;

		pipeline_state_desc.vertex_shader.shader_code = shader_code;
		pipeline_state_desc.vertex_shader.entry_point = "main_vs";
		pipeline_state_desc.vertex_shader.target = "vs_6_0";

		pipeline_state_desc.pixel_shader.shader_code = shader_code;
		pipeline_state_desc.pixel_shader.entry_point = "main_ps";
		pipeline_state_desc.pixel_shader.target = "ps_6_0";


		//Add input layouts
		pipeline_state_desc.input_layout.elements[0] = display::InputElementDesc("POSITION", 0, display::Format::R32G32_FLOAT, 0, 0);
		pipeline_state_desc.input_layout.elements[1] = display::InputElementDesc("TEXCOORD", 0, display::Format::R32G32B32A32_FLOAT, 1, 0, display::InputType::Instance);
		pipeline_state_desc.input_layout.num_elements = 2;

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
			"	float4 zoom_position : register(b0);\
				struct PSInput\
				{\
					float4 position : SV_POSITION;\
					float2 coords : TEXCOORD0; \
				};\
				\
				PSInput main_vs(float2 position : POSITION, float4 instance_data : TEXCOORD)\
				{\
					PSInput result; \
					result.position.xy = position.xy * instance_data.w + instance_data.xy; \
					result.position.xy = (result.position.xy - zoom_position.zw) * zoom_position.xy; \
					result.position.zw = float2(0.f, 1.f); \
					result.coords.xy = position.xy; \
					return result;\
				}\
				float4 main_ps(PSInput input) : SV_TARGET\
				{\
					float alpha = smoothstep(1.f, 0.95f, length(input.coords.xy));\
					return float4(alpha, alpha, alpha, alpha);\
				}";


		pipeline_state_desc.vertex_shader.shader_code = gazelle_shader_code;
		pipeline_state_desc.vertex_shader.entry_point = "main_vs";
		pipeline_state_desc.vertex_shader.target = "vs_6_0";

		pipeline_state_desc.pixel_shader.shader_code = gazelle_shader_code;
		pipeline_state_desc.pixel_shader.entry_point = "main_ps";
		pipeline_state_desc.pixel_shader.target = "ps_6_0";

		//Create
		m_gazelle_pipeline_state = display::CreatePipelineState(device, pipeline_state_desc, "Gazelle");

		//Lion
		//Create pipeline state
		const char* lion_shader_code =
			"	float4 zoom_position : register(b0);\
				struct PSInput\
				{\
					float4 position : SV_POSITION;\
					float2 coords : TEXCOORD0; \
				};\
				\
				PSInput main_vs(float2 position : POSITION, float4 instance_data : TEXCOORD)\
				{\
					PSInput result; \
					float2 rotate_position;\
					rotate_position.x = cos(instance_data.z) * position.x - sin(instance_data.z) * position.y;\
					rotate_position.y = sin(instance_data.z) * position.x + cos(instance_data.z) * position.y;\
					result.position.xy = rotate_position.xy * instance_data.w + instance_data.xy; \
					result.position.xy = (result.position.xy - zoom_position.zw) * zoom_position.xy; \
					result.position.zw = float2(0.f, 1.f); \
					result.coords.xy = position.xy; \
					return result;\
				}\
				float4 main_ps(PSInput input) : SV_TARGET\
				{\
					float distance = ( 0.5f - 0.5f * input.coords.y) / abs(input.coords.x + 0.0001f); \
					distance *= smoothstep(1.0f, 0.7f, -input.coords.y); \
					float alpha = smoothstep(0.8f, 0.95f, distance);\
					return float4(alpha, alpha, 0.f, alpha);\
				}";


		pipeline_state_desc.vertex_shader.shader_code = lion_shader_code;
		pipeline_state_desc.vertex_shader.entry_point = "main_vs";
		pipeline_state_desc.vertex_shader.target = "vs_6_0";

		pipeline_state_desc.pixel_shader.shader_code = lion_shader_code;
		pipeline_state_desc.pixel_shader.entry_point = "main_ps";
		pipeline_state_desc.pixel_shader.target = "ps_6_0";

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

			display::BufferDesc vertex_buffer_desc = display::BufferDesc::CreateVertexBuffer(display::Access::Static, sizeof(vertex_data), sizeof(VertexData), vertex_data);
			m_quad_vertex_buffer = display::CreateBuffer(device, vertex_buffer_desc, "quad_vertex_buffer");
		}

		//Quad Index buffer
		{
			uint16_t index_buffer_data[6] = { 0, 2, 1, 1, 2, 3 };
			display::BufferDesc index_buffer_desc = display::BufferDesc::CreateIndexBuffer(display::Access::Static, sizeof(index_buffer_data), display::Format::R16_UINT, index_buffer_data);
			m_quad_index_buffer = display::CreateBuffer(device, index_buffer_desc, "quad_index_buffer");
		}

		//Create constant buffer zoom position
		{
			float zoom_position_buffer[] = {1.f, 1.f, 0.f, 0.f};
		
			display::BufferDesc constant_buffer_desc = display::BufferDesc::CreateConstantBuffer(display::Access::Dynamic, sizeof(zoom_position_buffer), zoom_position_buffer);
			m_zoom_position = display::CreateBuffer(device, constant_buffer_desc, "ZoomConstantBuffer");
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
		display::DestroyHandle(device, m_zoom_position);
	}
};

#endif
