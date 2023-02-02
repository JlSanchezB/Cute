#include "box_city_resources.h"
#include "render/render.h"
#include <render_module/render_module_gpu_memory.h>

void BoxCityResources::Load(display::Device* device, render::System* render_system)
{
	auto* gpu_memory = render::GetModule<render::GPUMemoryRenderModule>(render_system, "GPUMemory"_sh32);
	{
		//Create view constant buffer
		display::ConstantBufferDesc view_constant_desc;
		view_constant_desc.size = sizeof(ViewConstantBuffer);
		view_constant_desc.access = display::Access::Dynamic;
		m_view_constant_buffer = display::CreateConstantBuffer(device, view_constant_desc, "ViewConstantBuffer");
	}

	//Box render root signature
	{
		display::RootSignatureDesc desc;
		desc.num_root_parameters = 2;
		desc.root_parameters[0].type = display::RootSignatureParameterType::Constants;
		desc.root_parameters[0].visibility = display::ShaderVisibility::All;
		desc.root_parameters[0].root_param.shader_register = 0;
		desc.root_parameters[0].root_param.num_constants = 1;

		desc.root_parameters[1].type = display::RootSignatureParameterType::DescriptorTable;
		desc.root_parameters[1].visibility = display::ShaderVisibility::All;
		desc.root_parameters[1].table.num_ranges = 2;
		desc.root_parameters[1].table.range[0].base_shader_register = 1;
		desc.root_parameters[1].table.range[0].size = 1;
		desc.root_parameters[1].table.range[0].type = display::DescriptorTableParameterType::ConstantBuffer;
		desc.root_parameters[1].table.range[1].base_shader_register = 0;
		desc.root_parameters[1].table.range[1].size = 3;
		desc.root_parameters[1].table.range[1].type = display::DescriptorTableParameterType::ShaderResource;
		desc.num_static_samplers = 0;

		m_box_render_root_signature = display::CreateRootSignature(device, desc, "BoxRenderRootSignature");
	}
	//Box render graphics PSO
	{
		display::PipelineStateDesc desc;
		desc.root_signature = m_box_render_root_signature;

		desc.input_layout.num_elements = 2;
		desc.input_layout.elements[0].input_slot = 0;
		desc.input_layout.elements[0].semantic_name = "POSITION";
		desc.input_layout.elements[0].semantic_index = 0;
		desc.input_layout.elements[0].format = display::Format::R32G32B32_FLOAT;
		desc.input_layout.elements[1].input_slot = 1;
		desc.input_layout.elements[1].semantic_name = "NORMAL";
		desc.input_layout.elements[1].semantic_index = 0;
		desc.input_layout.elements[1].format = display::Format::R32G32B32_FLOAT;

		desc.vertex_shader.file_name = "box_rendering.hlsl";
		desc.vertex_shader.name = "BoxRendering";
		desc.vertex_shader.entry_point = "vs_box_main";
		desc.vertex_shader.target = "vs_6_6";

		desc.pixel_shader.file_name = "box_rendering.hlsl";
		desc.pixel_shader.name = "BoxRendering";
		desc.pixel_shader.entry_point = "ps_box_main";
		desc.pixel_shader.target = "ps_6_6";

		desc.num_render_targets = 1;
		desc.render_target_format[0] = display::Format::R8G8B8A8_UNORM;

		desc.depth_enable = true;
		desc.depth_write = true;
		desc.depth_stencil_format = display::Format::D32_FLOAT;
		desc.depth_func = display::ComparationFunction::Greater;

		m_box_render_pipeline_state = display::CreatePipelineState(device, desc, "BoxRenderingPipelineState");
	}

	//Box Vertex buffer
	{
		glm::vec3 vertex_position_data[4 * 6];
		glm::vec3 vertex_normal_data[4 * 6];

		//Make the top face
		vertex_position_data[0] = glm::vec3(-1.f, 1.f, 1.f);
		vertex_position_data[1] = glm::vec3(1.f, 1.f, 1.f);
		vertex_position_data[2] = glm::vec3(-1.f, -1.f, 1.f);
		vertex_position_data[3] = glm::vec3(1.f, -1.f, 1.f);
		vertex_normal_data[0] = vertex_normal_data[1] = vertex_normal_data[2] = vertex_normal_data[3] = glm::vec3(0.f, 0.f, 1.f);

		//Make the rest rotating
		for (size_t i = 1; i < 6; ++i)
		{
			glm::mat3x3 rot;
			switch (i)
			{
			case 1:
				rot = glm::rotate(glm::half_pi<float>(), glm::vec3(1.f, 0.f, 0.f));
				break;
			case 2:
				rot = glm::rotate(glm::half_pi<float>(), glm::vec3(0.f, 1.f, 0.f));
				break;
			case 3:
				rot = glm::rotate(-glm::half_pi<float>(), glm::vec3(1.f, 0.f, 0.f));
				break;
			case 4:
				rot = glm::rotate(-glm::half_pi<float>(), glm::vec3(0.f, 1.f, 0.f));
				break;
			case 5:
				rot = glm::rotate(glm::pi<float>(), glm::vec3(1.f, 0.f, 0.f));
				break;
			}

			//Apply rotation
			vertex_position_data[i * 4 + 0] = rot * vertex_position_data[0];
			vertex_position_data[i * 4 + 1] = rot * vertex_position_data[1];
			vertex_position_data[i * 4 + 2] = rot * vertex_position_data[2];
			vertex_position_data[i * 4 + 3] = rot * vertex_position_data[3];
			vertex_normal_data[i * 4 + 0] = vertex_normal_data[i * 4 + 1] = vertex_normal_data[i * 4 + 2] = vertex_normal_data[i * 4 + 3] = rot * vertex_normal_data[0];
		}

		display::VertexBufferDesc vertex_buffer_desc;
		vertex_buffer_desc.init_data = vertex_position_data;
		vertex_buffer_desc.size = sizeof(vertex_position_data);
		vertex_buffer_desc.stride = sizeof(glm::vec3);

		m_box_vertex_position_buffer = display::CreateVertexBuffer(device, vertex_buffer_desc, "box_position_vertex_buffer");

		vertex_buffer_desc.init_data = vertex_normal_data;

		m_box_vertex_normal_buffer = display::CreateVertexBuffer(device, vertex_buffer_desc, "box_normal_vertex_buffer");
	}

	//Quad Index buffer
	{
		uint16_t index_buffer_data[36] = { 0 + 4 * 0, 3 + 4 * 0, 1 + 4 * 0, 0 + 4 * 0, 2 + 4 * 0, 3 + 4 * 0,
			0 + 4 * 1, 3 + 4 * 1, 1 + 4 * 1, 0 + 4 * 1, 2 + 4 * 1, 3 + 4 * 1,
			0 + 4 * 2, 3 + 4 * 2, 1 + 4 * 2, 0 + 4 * 2, 2 + 4 * 2, 3 + 4 * 2,
			0 + 4 * 3, 3 + 4 * 3, 1 + 4 * 3, 0 + 4 * 3, 2 + 4 * 3, 3 + 4 * 3,
			0 + 4 * 4, 3 + 4 * 4, 1 + 4 * 4, 0 + 4 * 4, 2 + 4 * 4, 3 + 4 * 4,
			0 + 4 * 5, 3 + 4 * 5, 1 + 4 * 5, 0 + 4 * 5, 2 + 4 * 5, 3 + 4 * 5 };
		display::IndexBufferDesc index_buffer_desc;
		index_buffer_desc.init_data = index_buffer_data;
		index_buffer_desc.size = sizeof(index_buffer_data);

		m_box_index_buffer = display::CreateIndexBuffer(device, index_buffer_desc, "box_index_buffer");
	}

	//Box culling root signature
	{
		display::RootSignatureDesc desc;
		desc.num_root_parameters = 2;
		desc.root_parameters[0].type = display::RootSignatureParameterType::Constants;
		desc.root_parameters[0].visibility = display::ShaderVisibility::All;
		desc.root_parameters[0].root_param.shader_register = 0;
		desc.root_parameters[0].root_param.num_constants = 2;

		desc.root_parameters[1].type = display::RootSignatureParameterType::DescriptorTable;
		desc.root_parameters[1].visibility = display::ShaderVisibility::All;
		desc.root_parameters[1].table.num_ranges = 3;
		desc.root_parameters[1].table.range[0].base_shader_register = 1;
		desc.root_parameters[1].table.range[0].size = 1;
		desc.root_parameters[1].table.range[0].type = display::DescriptorTableParameterType::ConstantBuffer;
		desc.root_parameters[1].table.range[1].base_shader_register = 0;
		desc.root_parameters[1].table.range[1].size = 2;
		desc.root_parameters[1].table.range[1].type = display::DescriptorTableParameterType::ShaderResource;
		desc.root_parameters[1].table.range[2].base_shader_register = 0;
		desc.root_parameters[1].table.range[2].size = 2;
		desc.root_parameters[1].table.range[2].type = display::DescriptorTableParameterType::UnorderedAccessBuffer;
		desc.num_static_samplers = 0;

		m_box_culling_root_signature = display::CreateRootSignature(device, desc, "BoxCullingRootSignature");
	}

	//Box culling pipeline state
	{
		display::ComputePipelineStateDesc desc;
		desc.compute_shader.name = "BoxCulling";
		desc.compute_shader.entry_point = "box_culling";
		desc.compute_shader.target = "cs_6_6";
		desc.compute_shader.file_name = "box_culling.hlsl";
		desc.root_signature = m_box_culling_root_signature;

		m_box_culling_pipeline_state = display::CreateComputePipelineState(device, desc, "BoxCulling");
	}

	//Box culling clear pipeline state
	{
		display::ComputePipelineStateDesc desc;
		desc.compute_shader.name = "BoxCullingClear";
		desc.compute_shader.entry_point = "clear_indirect_arguments";
		desc.compute_shader.target = "cs_6_6";
		desc.compute_shader.file_name = "box_culling.hlsl";
		desc.root_signature = m_box_culling_root_signature;

		m_box_culling_clear_pipeline_state = display::CreateComputePipelineState(device, desc, "BoxCullingClear");
	}

	{
		//Create indirect culling buffers
		display::UnorderedAccessBufferDesc indirect_box_buffer_desc;
		indirect_box_buffer_desc.type = display::UnorderedAccessBufferType::StructuredBuffer;
		indirect_box_buffer_desc.element_size = sizeof(uint32_t);
		indirect_box_buffer_desc.element_count = kIndirectBoxBufferCount;

		m_indirect_box_buffer = display::CreateUnorderedAccessBuffer(device, indirect_box_buffer_desc, "IndirectBoxBuffer");

		display::UnorderedAccessBufferDesc indirect_parameters_buffer_desc;
		indirect_parameters_buffer_desc.type = display::UnorderedAccessBufferType::StructuredBuffer;
		indirect_parameters_buffer_desc.element_size = sizeof(uint32_t);
		indirect_parameters_buffer_desc.element_count = 5;

		m_indirect_parameters_buffer = display::CreateUnorderedAccessBuffer(device, indirect_parameters_buffer_desc, "IndirectParametersBuffer");
	}

	{
		display::DescriptorTableDesc description_table_desc;
		description_table_desc.num_descriptors = 4;
		description_table_desc.access = display::Access::Dynamic;
		description_table_desc.descriptors[0] = m_view_constant_buffer;
		description_table_desc.descriptors[1] = display::WeakUnorderedAccessBufferHandleAsShaderResource(gpu_memory->GetStaticGPUMemoryResource());
		description_table_desc.descriptors[2] = gpu_memory->GetDynamicGPUMemoryResource();
		description_table_desc.descriptors[3] = display::WeakUnorderedAccessBufferHandleAsShaderResource(m_indirect_box_buffer);

		//Create the descriptor table
		m_box_render_description_table_handle = display::CreateDescriptorTable(device, description_table_desc);
	}

	{
		display::DescriptorTableDesc description_table_desc;
		description_table_desc.num_descriptors = 5;
		description_table_desc.access = display::Access::Dynamic;
		description_table_desc.descriptors[0] = m_view_constant_buffer;
		description_table_desc.descriptors[1] = display::WeakUnorderedAccessBufferHandleAsShaderResource(gpu_memory->GetStaticGPUMemoryResource());
		description_table_desc.descriptors[2] = gpu_memory->GetDynamicGPUMemoryResource();
		description_table_desc.descriptors[3] = m_indirect_parameters_buffer;
		description_table_desc.descriptors[4] = m_indirect_box_buffer;

		//Create the descriptor table
		m_box_culling_description_table_handle = display::CreateDescriptorTable(device, description_table_desc);
	}
}

void BoxCityResources::Unload(display::Device* device)
{
	display::DestroyHandle(device, m_view_constant_buffer);
	display::DestroyHandle(device, m_box_render_description_table_handle);
	display::DestroyHandle(device, m_box_render_root_signature);
	display::DestroyHandle(device, m_box_render_pipeline_state);
	display::DestroyHandle(device, m_box_vertex_position_buffer);
	display::DestroyHandle(device, m_box_vertex_normal_buffer);
	display::DestroyHandle(device, m_box_index_buffer);
	display::DestroyHandle(device, m_box_culling_description_table_handle);
	display::DestroyHandle(device, m_box_culling_root_signature);
	display::DestroyHandle(device, m_box_culling_pipeline_state);
	display::DestroyHandle(device, m_box_culling_clear_pipeline_state);
	display::DestroyHandle(device, m_indirect_box_buffer);
	display::DestroyHandle(device, m_indirect_parameters_buffer);
}
