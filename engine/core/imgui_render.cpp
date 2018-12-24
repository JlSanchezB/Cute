#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <windows.h>

#undef max

#include "imgui_render.h"
#include <ext/imgui/imgui.h>
#include <display/display.h>

namespace
{
	//Resources for rendering imgui
	display::RootSignatureHandle g_rootsignature;
	display::PipelineStateHandle g_pipeline_state;
	display::ShaderResourceHandle g_texture;
	display::VertexBufferHandle g_vertex_buffer;
	size_t current_vertex_buffer_size = 4000;
	size_t current_index_buffer_size = 4000;
	display::IndexBufferHandle g_index_buffer;
	display::DescriptorTableHandle g_descriptor_table;

	ImGuiMouseCursor g_LastMouseCursor = ImGuiMouseCursor_COUNT;

	void UpdateMousePos(HWND hWnd)
	{
		ImGuiIO& io = ImGui::GetIO();

		// Set OS mouse position if requested (rarely used, only when ImGuiConfigFlags_NavEnableSetMousePos is enabled by user)
		if (io.WantSetMousePos)
		{
			POINT pos = { (int)io.MousePos.x, (int)io.MousePos.y };
			::ClientToScreen(hWnd, &pos);
			::SetCursorPos(pos.x, pos.y);
		}

		// Set mouse position
		io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
		POINT pos;
		if (::GetActiveWindow() == hWnd && ::GetCursorPos(&pos))
			if (::ScreenToClient(hWnd, &pos))
				io.MousePos = ImVec2((float)pos.x, (float)pos.y);
	}

	bool UpdateMouseCursor(HWND hWnd)
	{
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
			return false;

		ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
		if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor)
		{
			// Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
			::SetCursor(NULL);
		}
		else
		{
			// Show OS mouse cursor
			LPTSTR win32_cursor = IDC_ARROW;
			switch (imgui_cursor)
			{
			case ImGuiMouseCursor_Arrow:        win32_cursor = IDC_ARROW; break;
			case ImGuiMouseCursor_TextInput:    win32_cursor = IDC_IBEAM; break;
			case ImGuiMouseCursor_ResizeAll:    win32_cursor = IDC_SIZEALL; break;
			case ImGuiMouseCursor_ResizeEW:     win32_cursor = IDC_SIZEWE; break;
			case ImGuiMouseCursor_ResizeNS:     win32_cursor = IDC_SIZENS; break;
			case ImGuiMouseCursor_ResizeNESW:   win32_cursor = IDC_SIZENESW; break;
			case ImGuiMouseCursor_ResizeNWSE:   win32_cursor = IDC_SIZENWSE; break;
			case ImGuiMouseCursor_Hand:         win32_cursor = IDC_HAND; break;
			}
			::SetCursor(::LoadCursor(NULL, win32_cursor));
		}
		return true;
	}
}

void imgui_render::Init(HWND hwnd)
{
	// Setup back-end capabilities flags
	ImGuiIO& io = ImGui::GetIO();
	io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
	io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)
	io.ImeWindowHandle = hwnd;

	// Keyboard mapping. ImGui will use those indices to peek into the io.KeysDown[] array that we will update during the application lifetime.
	io.KeyMap[ImGuiKey_Tab] = VK_TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
	io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
	io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
	io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
	io.KeyMap[ImGuiKey_Home] = VK_HOME;
	io.KeyMap[ImGuiKey_End] = VK_END;
	io.KeyMap[ImGuiKey_Insert] = VK_INSERT;
	io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
	io.KeyMap[ImGuiKey_Space] = VK_SPACE;
	io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
	io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
	io.KeyMap[ImGuiKey_A] = 'A';
	io.KeyMap[ImGuiKey_C] = 'C';
	io.KeyMap[ImGuiKey_V] = 'V';
	io.KeyMap[ImGuiKey_X] = 'X';
	io.KeyMap[ImGuiKey_Y] = 'Y';
	io.KeyMap[ImGuiKey_Z] = 'Z';
}

void imgui_render::CreateResources(display::Device * device)
{
	//Create root signature
	display::RootSignatureDesc rootsignature_desc;
	rootsignature_desc.num_root_parameters = 2;
	rootsignature_desc.root_parameters[0].type = display::RootSignatureParameterType::Constants;
	rootsignature_desc.root_parameters[0].visibility = display::ShaderVisibility::Vertex;
	rootsignature_desc.root_parameters[0].root_param.shader_register = 0;
	rootsignature_desc.root_parameters[0].root_param.num_constants = 16;
	rootsignature_desc.root_parameters[1].type = display::RootSignatureParameterType::DescriptorTable;
	rootsignature_desc.root_parameters[1].visibility = display::ShaderVisibility::Pixel;
	rootsignature_desc.root_parameters[1].table.num_ranges = 1;
	rootsignature_desc.root_parameters[1].table.range[0].base_shader_register = 0;
	rootsignature_desc.root_parameters[1].table.range[0].size = 1;
	rootsignature_desc.root_parameters[1].table.range[0].type = display::DescriptorTableParameterType::ShaderResource;

	rootsignature_desc.num_static_samplers = 1;
	rootsignature_desc.static_samplers[0].address_u = rootsignature_desc.static_samplers[0].address_v = rootsignature_desc.static_samplers[0].address_w = display::TextureAddressMode::Wrap;
	rootsignature_desc.static_samplers[0].filter = display::Filter::Linear;
	rootsignature_desc.static_samplers[0].shader_register = 0;
	rootsignature_desc.static_samplers[0].visibility = display::ShaderVisibility::Pixel;
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

	pipeline_state_desc.rasteritation_state.cull_mode = display::CullMode::None;

	pipeline_state_desc.blend_desc.render_target_blend[0].blend_enable = true;
	pipeline_state_desc.blend_desc.render_target_blend[0].src_blend = display::Blend::SrcAlpha;
	pipeline_state_desc.blend_desc.render_target_blend[0].dest_blend = display::Blend::InvSrcAlpha;
	pipeline_state_desc.blend_desc.render_target_blend[0].blend_op = display::BlendOp::Add;
	pipeline_state_desc.blend_desc.render_target_blend[0].alpha_src_blend = display::Blend::InvSrcAlpha;
	pipeline_state_desc.blend_desc.render_target_blend[0].alpha_dest_blend = display::Blend::Zero;
	pipeline_state_desc.blend_desc.render_target_blend[0].alpha_blend_op = display::BlendOp::Add;

	pipeline_state_desc.num_render_targets = 1;
	pipeline_state_desc.render_target_format[0] = display::Format::R8G8B8A8_UNORM;

	//Create
	g_pipeline_state = display::CreatePipelineState(device, pipeline_state_desc, "imgui");

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
	shader_resource_desc.size = width * height * 4;
	shader_resource_desc.mips = 1;
	g_texture = display::CreateShaderResource(device, shader_resource_desc, "imgui");

	//Create Vertex buffer (inited in some size and it will grow by demand)
	display::VertexBufferDesc vertex_buffer_desc;
	vertex_buffer_desc.access = display::Access::Dynamic;
	vertex_buffer_desc.size = current_vertex_buffer_size * sizeof(ImDrawVert);
	vertex_buffer_desc.stride = 20;
	g_vertex_buffer = display::CreateVertexBuffer(device, vertex_buffer_desc, "imgui");

	//Create Index buffer
	display::IndexBufferDesc index_buffer_desc;
	index_buffer_desc.access = display::Access::Dynamic;
	index_buffer_desc.size = current_index_buffer_size * sizeof(ImDrawIdx);
	g_index_buffer = display::CreateIndexBuffer(device, index_buffer_desc, "imgui");

	//Descritor table
	display::DescriptorTableDesc descriptor_table_desc;
	descriptor_table_desc.access = display::Access::Static;
	descriptor_table_desc.AddDescriptor(g_texture);

	g_descriptor_table = display::CreateDescriptorTable(device, descriptor_table_desc);

	static_assert(sizeof(ImTextureID) >= sizeof(g_descriptor_table), "Can't pack descriptor handle into TexID, 32-bit not supported yet.");
	io.Fonts->TexID = (ImTextureID)&g_descriptor_table;
}

void imgui_render::DestroyResources(display::Device * device)
{
	display::DestroyRootSignature(device, g_rootsignature);
	display::DestroyPipelineState(device, g_pipeline_state);
	display::DestroyShaderResource(device, g_texture);
	display::DestroyVertexBuffer(device, g_vertex_buffer);
	display::DestroyIndexBuffer(device, g_index_buffer);
	display::DestroyDescriptorTable(device, g_descriptor_table);
}

void imgui_render::NextFrame(HWND hWnd, float elapsed_time)
{
	ImGuiIO& io = ImGui::GetIO();

	// Setup display size (every frame to accommodate for window resizing)
	RECT rect;
	::GetClientRect(hWnd, &rect);
	io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));

	// Setup time step
	io.DeltaTime = elapsed_time;

	// Read keyboard modifiers inputs
	io.KeyCtrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
	io.KeyShift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
	io.KeyAlt = (::GetKeyState(VK_MENU) & 0x8000) != 0;
	io.KeySuper = false;
	// io.KeysDown[], io.MousePos, io.MouseDown[], io.MouseWheel: filled by the WndProc handler below.

	// Update OS mouse position
	UpdateMousePos(hWnd);

	// Update OS mouse cursor with the cursor requested by imgui
	ImGuiMouseCursor mouse_cursor = io.MouseDrawCursor ? ImGuiMouseCursor_None : ImGui::GetMouseCursor();
	if (g_LastMouseCursor != mouse_cursor)
	{
		g_LastMouseCursor = mouse_cursor;
		UpdateMouseCursor(hWnd);
	}

	//Next frame in ImGui
	ImGui::NewFrame();
}

bool imgui_render::WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui::GetCurrentContext() == NULL)
		return false;

	ImGuiIO& io = ImGui::GetIO();
	switch (msg)
	{
	case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
	{
		int button = 0;
		if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) button = 0;
		if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK) button = 1;
		if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK) button = 2;
		if (!ImGui::IsAnyMouseDown() && ::GetCapture() == NULL)
			::SetCapture(hwnd);
		io.MouseDown[button] = true;
		return false;
	}
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		int button = 0;
		if (msg == WM_LBUTTONUP) button = 0;
		if (msg == WM_RBUTTONUP) button = 1;
		if (msg == WM_MBUTTONUP) button = 2;
		io.MouseDown[button] = false;
		if (!ImGui::IsAnyMouseDown() && ::GetCapture() == hwnd)
			::ReleaseCapture();
		return false;
	}
	case WM_MOUSEWHEEL:
		io.MouseWheel += (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
		return false;
	case WM_MOUSEHWHEEL:
		io.MouseWheelH += (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
		return false;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		if (wParam < 256)
			io.KeysDown[wParam] = 1;
		return false;
	case WM_KEYUP:
	case WM_SYSKEYUP:
		if (wParam < 256)
			io.KeysDown[wParam] = 0;
		return false;
	case WM_CHAR:
		// You can also use ToAscii()+GetKeyboardState() to retrieve characters.
		if (wParam > 0 && wParam < 0x10000)
			io.AddInputCharacter((unsigned short)wParam);
		return false;
	case WM_SETCURSOR:
		if (LOWORD(lParam) == HTCLIENT && UpdateMouseCursor(hwnd))
			return true;
		return false;
	}
	return false;
}

void imgui_render::Draw(display::Context* context)
{
	auto draw_data = ImGui::GetDrawData();

	display::Device* device = context->GetDevice();

	//Set back buffer
	context->SetRenderTargets(1, &display::GetBackBuffer(device), display::WeakDepthBufferHandle());

	//Check if we need to create a vertex buffer
	if (current_vertex_buffer_size < draw_data->TotalVtxCount)
	{
		//Grow
		current_vertex_buffer_size = draw_data->TotalVtxCount + 5000;

		//Destroy old vertex buffer
		display::DestroyVertexBuffer(device, g_vertex_buffer);

		//Create a new one
		display::VertexBufferDesc vertex_buffer_desc;
		vertex_buffer_desc.access = display::Access::Dynamic;
		vertex_buffer_desc.size = current_vertex_buffer_size * sizeof(ImDrawVert);
		vertex_buffer_desc.stride = 20;
		g_vertex_buffer = display::CreateVertexBuffer(device, vertex_buffer_desc, "imgui");
	}

	//Check if we need to create a index buffer
	if (current_index_buffer_size < draw_data->TotalIdxCount)
	{
		//Grow
		current_index_buffer_size = draw_data->TotalIdxCount + 10000;

		//Destroy old index buffer
		display::DestroyIndexBuffer(device, g_index_buffer);

		//Create new one
		display::IndexBufferDesc index_buffer_desc;
		index_buffer_desc.access = display::Access::Dynamic;
		index_buffer_desc.size = current_index_buffer_size * sizeof(ImDrawIdx);
		g_index_buffer = display::CreateIndexBuffer(device, index_buffer_desc, "imgui");
	}

	//Update constant buffer
	float L = draw_data->DisplayPos.x;
	float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
	float T = draw_data->DisplayPos.y;
	float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
	float mvp[4][4] =
	{
		{ 2.0f / (R - L),   0.0f,           0.0f,       0.0f },
		{ 0.0f,         2.0f / (T - B),     0.0f,       0.0f },
		{ 0.0f,         0.0f,           0.5f,       0.0f },
		{ (R + L) / (L - R),  (T + B) / (B - T),    0.5f,       1.0f },
	};

	//Update vertex buffer and index buffer
	std::vector<ImDrawVert> vertex_buffer(draw_data->TotalVtxCount);
	std::vector<ImDrawIdx> index_buffer(draw_data->TotalIdxCount);

	ImDrawVert* vtx_dst = vertex_buffer.data();
	ImDrawIdx* idx_dst = index_buffer.data();
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		vtx_dst += cmd_list->VtxBuffer.Size;
		idx_dst += cmd_list->IdxBuffer.Size;
	}

	display::UpdateResourceBuffer(device, g_vertex_buffer, vertex_buffer.data(), draw_data->TotalVtxCount * sizeof(ImDrawVert));
	display::UpdateResourceBuffer(device, g_index_buffer, index_buffer.data(), draw_data->TotalIdxCount * sizeof(ImDrawIdx));


	//Set all the resources
	context->SetRootSignature(display::Pipe::Graphics, g_rootsignature);
	context->SetPipelineState(g_pipeline_state);
	context->SetVertexBuffers(0, 1, &g_vertex_buffer);
	context->SetIndexBuffer(g_index_buffer);
	//display::SetConstantBuffer(device, command_list_handle, 0, g_constant_buffer);
	context->SetConstants(display::Pipe::Graphics, 0, mvp, 16);
	context->SetViewport(display::Viewport(draw_data->DisplaySize.x, draw_data->DisplaySize.y));

	// Render command lists
	int vtx_offset = 0;
	int idx_offset = 0;
	ImVec2 pos = draw_data->DisplayPos;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback)
			{
				pcmd->UserCallback(cmd_list, pcmd);
			}
			else
			{
				display::Rect rect;
				rect.left = static_cast<size_t>(pcmd->ClipRect.x - pos.x);
				rect.top = static_cast<size_t>(pcmd->ClipRect.y - pos.y);
				rect.right = static_cast<size_t>(pcmd->ClipRect.z - pos.x);
				rect.bottom = static_cast<size_t>(pcmd->ClipRect.w - pos.y);
				context->SetScissorRect( rect);
				
				context->SetDescriptorTable(display::Pipe::Graphics, 1, *reinterpret_cast<display::DescriptorTableHandle*>(pcmd->TextureId));

				display::DrawIndexedDesc draw_desc;
				draw_desc.index_count = pcmd->ElemCount;
				draw_desc.base_vertex = vtx_offset;
				draw_desc.start_index = idx_offset;
				context->DrawIndexed(draw_desc);
			}
			idx_offset += pcmd->ElemCount;
		}
		vtx_offset += cmd_list->VtxBuffer.Size;
	}
}
