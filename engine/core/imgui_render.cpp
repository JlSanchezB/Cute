#include "imgui\imgui_render.h"

namespace
{
	//Resources for rendering imgui
	display::RootSignatureHandle g_rootsignature;
	display::PipelineStateHandle g_pipeline_state;
	display::ShaderResourceHandle g_texture;
	display::ConstantBufferHandle g_constant_buffer;
	display::VertexBufferHandle g_vertex_buffer;
	display::IndexBufferHandle g_index_buffer;
	display::DescriptorTableHandle g_descriptor_table;
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
	g_rootsignature = display::CreateRootSignature(device, rootsignature_desc, "imguid");

	//Create pipeline state

	//Create constant buffer

	//Create texture

	//Create Vertex buffer (inited in some size and it will grow by demand)

	//Create Index buffer

	//Descritor table
}

void imgui_render::DestroyResources(display::Device * device)
{
}

void imgui_render::Draw(display::Device * device, const display::CommandListHandle & command_list_handle)
{
}
