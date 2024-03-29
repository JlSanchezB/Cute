#include "display_common.h"
#include <ext/imgui/imgui.h>
#include <utility>
#include <locale.h>
#include <core/profile.h>
#include <fstream>
#include <unordered_set>
#include <helpers/imgui_helper.h>
#include <regex>
#include <core/control_variables.h>
#include <string>
#include <core/platform.h>

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 608; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = u8".\\D3D12\\"; }

namespace
{
	std::string GetLine(const std::string& source_code, const uint32_t line)
	{
		size_t start = 0;
		size_t end = 0;

		for (size_t i = 0; i < line; ++i)
		{
			end = source_code.find('\n', start);
			if (end == std::string::npos)
			{
				//Bad
				return "Line not found";
			}
			start = end + 1;
		}
		//Find the end
		end = source_code.find('\n', start + 1);

		return source_code.substr(start, end - start - 1); //Removing the return
	}

	struct AssertPosition
	{
		uint32_t shader_file_id;
		uint32_t line_num;

		bool operator==(const AssertPosition& a) const
		{
			return shader_file_id == a.shader_file_id && line_num == a.line_num;
		}
	};
}

namespace std {
	template<>
	struct hash<AssertPosition> {
		inline size_t operator()(const AssertPosition& x) const {
			return x.line_num + x.shader_file_id;
		}
	};
}

namespace display
{
	_Use_decl_annotations_
	void GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter, uint32_t adapter_index)
	{
		ComPtr<IDXGIAdapter1> adapter;
		*ppAdapter = nullptr;

		if (adapter_index != -1)
		{
			//Try to use the adapter index
			if (DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapter_index, &adapter))
			{
				DXGI_ADAPTER_DESC1 desc;
				adapter->GetDesc1(&desc);

				if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				{
					// Don't select the Basic Render Driver adapter.
					// If you want a software adapter, pass in "/warp" on the command line.	
				}
				else
				{
					// Check to see if the adapter supports Direct3D 12, but don't create the
					// actual device yet.
					if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
					{
						*ppAdapter = adapter.Detach();
						return;
					}
				}
			}

			//If it doesn't work, just use the first valid
			core::LogInfo("Adapter index %i can not be initied, using the first valid", adapter_index);
		}
		for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				// Don't select the Basic Render Driver adapter.
				// If you want a software adapter, pass in "/warp" on the command line.
				continue;
			}

			// Check to see if the adapter supports Direct3D 12, but don't create the
			// actual device yet.
			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
			{
				if (adapter_index != -1)
				{
					core::LogInfo("Valid adapter found (%i)", adapterIndex);
				}
				break;
			}
		}

		*ppAdapter = adapter.Detach();
	}

	// Wait for pending GPU work to complete.
	void WaitForGpu(display::Device* device)
	{
		// Schedule a Signal command in the queue.
		ThrowIfFailed(device->m_command_queue->Signal(device->m_fence.Get(), device->m_frame_resources[device->m_frame_index].fence_value));

		// Wait until the fence has been processed.
		ThrowIfFailed(device->m_fence->SetEventOnCompletion(device->m_frame_resources[device->m_frame_index].fence_value, device->m_fence_event));
		WaitForSingleObjectEx(device->m_fence_event, INFINITE, FALSE);

		// Increment the fence value for the current frame.
		device->m_frame_resources[device->m_frame_index].fence_value++;

		//That increase was produced because a wait
		device->m_fence_wait_offset++;
	}

	// Prepare to render the next frame.
	void MoveToNextFrame(display::Device* device)
	{
		// Schedule a Signal command in the queue.
		const UINT64 currentFenceValue = device->m_frame_resources[device->m_frame_index].fence_value;
		ThrowIfFailed(device->m_command_queue->Signal(device->m_fence.Get(), currentFenceValue));

		// Update the frame index.
		device->m_frame_index = device->m_swap_chain->GetCurrentBackBufferIndex();

		// If the next frame is not ready to be rendered yet, wait until it is ready.
		if (device->m_fence->GetCompletedValue() < device->m_frame_resources[device->m_frame_index].fence_value)
		{
			ThrowIfFailed(device->m_fence->SetEventOnCompletion(device->m_frame_resources[device->m_frame_index].fence_value, device->m_fence_event));
			WaitForSingleObjectEx(device->m_fence_event, INFINITE, FALSE);
		}

		// Set the fence value for the next frame.
		device->m_frame_resources[device->m_frame_index].fence_value = currentFenceValue + 1;
	}

	void DisplayImguiStats(Device* device, bool* activated)
	{
		if (ImGui::Begin("Display Stats", activated, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Adapter (%s)", device->m_adapter_description);
			ImGui::Text("Resolution (%i,%i), frames (%i)", device->m_width, device->m_height, device->m_frame_resources.size());
			ImGui::Text("windowed(%s), tearing(%s), vsync(%s)", (device->m_windowed) ? "true" : "false", (device->m_tearing) ? "true" : "false", (device->m_vsync) ? "true" : "false");
			ImGui::Separator();
			char buffer[1024];
			helpers::FormatMemory(buffer, 1024, device->uploaded_memory_frame);
			ImGui::Text("Uploaded memory each frame (%s)", buffer);
			helpers::FormatMemory(buffer, 1024, device->m_upload_buffer_max_size);
			ImGui::Text("Number of pooled upload buffers (%d), size for each (%s)", device->m_upload_buffer_pool.size(), buffer);
			ImGui::Separator();
			ImGui::Text("Command list handles (%zu/%zu)", device->m_command_list_pool.Size(), device->m_command_list_pool.MaxSize());
			ImGui::Text("Root signature handles (%zu/%zu)", device->m_root_signature_pool.Size(), device->m_root_signature_pool.MaxSize());
			ImGui::Text("Pipeline state handles (%zu/%zu)", device->m_pipeline_state_pool.Size(), device->m_pipeline_state_pool.MaxSize());
			ImGui::Text("Descriptor table handles (%zu/%zu)", device->m_descriptor_table_pool.Size(), device->m_descriptor_table_pool.MaxSize());
			ImGui::Text("Sampler descriptor table handles (%zu/%zu)", device->m_sampler_descriptor_table_pool.Size(), device->m_sampler_descriptor_table_pool.MaxSize());

			if (ImGui::TreeNode("Buffer handles ","Buffer handles (%zu/%zu)", device->m_buffer_pool.Size(), device->m_buffer_pool.MaxSize()))
			{
				device->m_buffer_pool.VisitSlow([&](WeakBufferHandle handle)
					{
						const auto& buffer = device->Get(handle);
						ImGui::Text("Buffer (%s), ShaderResource(%d), UAV(%d), Updatable(%d)", buffer.name, (buffer.shader_access) ? 1 : 0, (buffer.UAV) ? 1 : 0, (buffer.memory_data) ? 1 : 0);
					});
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Texture2D handles ", "Texture2D handles (%zu/%zu)", device->m_texture_2d_pool.Size(), device->m_texture_2d_pool.MaxSize()))
			{
				device->m_texture_2d_pool.VisitSlow([&](WeakTexture2DHandle handle)
					{
						const auto& texture = device->Get(handle);
						ImGui::Text("Buffer (%s), UAV(%d), RenderTarget(%d), DepthBuffer(%d)", texture.name, (texture.UAV) ? 1 : 0, (texture.RenderTarget) ? 1 : 0, (texture.DepthBuffer) ? 1 : 0);
					});
				ImGui::TreePop();
			}
			ImGui::Separator();
			D3D12MA::TotalStatistics stadistics;
			device->m_allocator->CalculateStatistics(&stadistics);
			D3D12MA::Budget local_budget;
			device->m_allocator->GetBudget(&local_budget, nullptr);
			char buffer2[1024];
			helpers::FormatMemory(buffer, 1024, local_budget.UsageBytes);
			helpers::FormatMemory(buffer2, 1024, local_budget.BudgetBytes);
			ImGui::Text("Allocator stats: Usage(%s), Budget(%s)",
				buffer, buffer2);
			for (size_t i = 0; i < 3; ++i)
			{
				switch (i)
				{
				case 0: ImGui::Text("	Default Heap"); break;
				case 1: ImGui::Text("	Upload Heap"); break;
				case 2: ImGui::Text("	ReadBack Heap"); break;
				};
				D3D12MA::DetailedStatistics& detail_stats = stadistics.HeapType[i];
				D3D12MA::Statistics& stats = stadistics.HeapType[i].Stats;

				helpers::FormatMemory(buffer, 1024, stats.AllocationBytes);
				helpers::FormatMemory(buffer2, 1024, stats.BlockBytes);
				ImGui::Text("		Num Allocations(%u), Allocated(%s), Num Heaps(%u), Heap Allocated(%s)",
					stats.AllocationCount, buffer, stats.BlockCount, buffer2);
				ImGui::Text("		Unused Range Count(%u), Allocation Size Min(%llu), Allocation Size Max(%llu), Unused Range Size Min(%llu), Unused Range Size Max(%llu)",
					detail_stats.UnusedRangeCount, detail_stats.AllocationSizeMin, detail_stats.AllocationSizeMax, detail_stats.UnusedRangeSizeMin, detail_stats.UnusedRangeSizeMax);
			}

			ImGui::End();	
		}
	}

	void UploadDevelopmentShaderBuffer(display::Device* device)
	{
		if (device->m_development_shaders)
		{
			bool descriptor_needs_update = false;

			//Check if the size is sufficient
			if (device->m_development_shaders_control_variables_size == 0 || device->m_control_variables.size() >= device->m_development_shaders_control_variables_size)
			{
				//Delete old buffers
				if (device->m_development_shaders_control_variables_buffer.IsValid())
					display::DestroyBuffer(device, device->m_development_shaders_control_variables_buffer);

				device->m_development_shaders_control_variables_size = std::max(64ull, device->m_control_variables.size() * 2);

				//Create the control variables buffer
				display::BufferDesc buffer_desc = display::BufferDesc::CreateStructuredBuffer(display::Access::Static, static_cast<uint32_t>(device->m_development_shaders_control_variables_size), 4, false);
				device->m_development_shaders_control_variables_buffer = display::CreateBuffer(device, buffer_desc, "Development Shaders Control Variables Buffer");

				descriptor_needs_update = true;
			}

			if (device->m_development_shaders_counters_size == 0 || device->m_counters.size() >= device->m_development_shaders_counters_size)
			{
				//Delete old buffers
				if (device->m_development_shaders_counters_buffer.IsValid())
					display::DestroyBuffer(device, device->m_development_shaders_counters_buffer);
				if (device->m_development_shaders_counters_readback_buffer.IsValid())
					display::DestroyBuffer(device, device->m_development_shaders_counters_readback_buffer);

				device->m_development_shaders_counters_size = std::max(64ull, device->m_counters.size() * 2);

				//Create development shaders buffer supporting UAV
				display::BufferDesc buffer_desc = display::BufferDesc::CreateStructuredBuffer(display::Access::Static, static_cast<uint32_t>(device->m_development_shaders_counters_size), 4, true);
				device->m_development_shaders_counters_buffer = display::CreateBuffer(device, buffer_desc, "Development Shaders Counters Buffer");

				//Create development shaders read back buffer, so the data can go from the GPU to the CPU as needed
				buffer_desc = display::BufferDesc::CreateStructuredBuffer(display::Access::ReadBack, static_cast<uint32_t>(device->m_development_shaders_counters_size), 4);
				device->m_development_shaders_counters_readback_buffer = display::CreateBuffer(device, buffer_desc, "Development Shaders Counters ReadBack Buffer");

				descriptor_needs_update = true;
			}

			if (device->m_development_shaders_commands_size == 0 || device->m_development_shaders_commands_used_size >= device->m_development_shaders_commands_size * 2 / 3)
			{
				//Grow if the used size is more than 2/3 the size of the commands buffer

				//Delete old buffers
				if (device->m_development_shaders_commands_buffer.IsValid())
					display::DestroyBuffer(device, device->m_development_shaders_commands_buffer);
				if (device->m_development_shaders_commands_readback_buffer.IsValid())
					display::DestroyBuffer(device, device->m_development_shaders_commands_readback_buffer);

				device->m_development_shaders_commands_size = std::max(1024ull, device->m_development_shaders_commands_used_size * 2);

				//Create development shaders buffer supporting UAV
				display::BufferDesc buffer_desc = display::BufferDesc::CreateStructuredBuffer(display::Access::Static, static_cast<uint32_t>(device->m_development_shaders_commands_size), 4, true);
				device->m_development_shaders_commands_buffer = display::CreateBuffer(device, buffer_desc, "Development Shaders Commands Buffer");

				//Create development shaders read back buffer, so the data can go from the GPU to the CPU as needed
				buffer_desc = display::BufferDesc::CreateStructuredBuffer(display::Access::ReadBack, static_cast<uint32_t>(device->m_development_shaders_commands_size), 4);
				device->m_development_shaders_commands_readback_buffer = display::CreateBuffer(device, buffer_desc, "Development Shaders Commands ReadBack Buffer");

				descriptor_needs_update = true;
			}

			//Reset the header, control variables and stats of the buffer

			//Open command list
			display::Context* context = OpenCommandList(device, device->m_resource_command_list);
			
			//Upload the control variables
			{
				context->AddResourceBarriers({ display::ResourceBarrier(device->m_development_shaders_control_variables_buffer, display::TranstitionState::AllShaderResource, display::TranstitionState::CopyDest) });

				uint32_t* upload_buffer_data = reinterpret_cast<uint32_t*>(context->UpdateBufferResource(device->m_development_shaders_control_variables_buffer, 0, device->m_control_variables.size() * sizeof(uint32_t)));

				//Upload data
				for (auto& it : device->m_control_variables)
				{
					//Get values from the CPU control variables
					switch (it.second.default_value.index())
					{
					case 0:
						upload_buffer_data[it.second.index] = *reinterpret_cast<uint32_t*>(std::get<float*>(it.second.control_variable_ptr));
						break;
					case 1:
						upload_buffer_data[it.second.index] = *(std::get<uint32_t*>(it.second.control_variable_ptr));
						break;
					case 2:
						upload_buffer_data[it.second.index] = *std::get<bool*>(it.second.control_variable_ptr) ? 1 : 0;
						break;
					}
				}
				context->AddResourceBarriers({ display::ResourceBarrier(device->m_development_shaders_control_variables_buffer, display::TranstitionState::CopyDest, display::TranstitionState::AllShaderResource) });
			}
			//Reset counters
			{
				context->AddResourceBarriers({ display::ResourceBarrier(device->m_development_shaders_counters_buffer, display::TranstitionState::UnorderedAccess, display::TranstitionState::CopyDest) });

				uint32_t* upload_buffer_data = reinterpret_cast<uint32_t*>(context->UpdateBufferResource(device->m_development_shaders_counters_buffer, 0, device->m_counters.size() * sizeof(uint32_t)));

				for (size_t i = 0; i < device->m_counters.size(); ++i)
				{
					upload_buffer_data[i] = 0; //All stats reset to zero
				}

				context->AddResourceBarriers({ display::ResourceBarrier(device->m_development_shaders_counters_buffer, display::TranstitionState::CopyDest, display::TranstitionState::UnorderedAccess) });
			}
			//Reset commands size to zero (just the first integer)
			{
				context->AddResourceBarriers({ display::ResourceBarrier(device->m_development_shaders_commands_buffer, display::TranstitionState::UnorderedAccess, display::TranstitionState::CopyDest) });

				uint32_t* upload_buffer_data = reinterpret_cast<uint32_t*>(context->UpdateBufferResource(device->m_development_shaders_commands_buffer, 0, 2 * sizeof(uint32_t)));

				upload_buffer_data[0] = 2; //All commands reset to zero (the start index is the second uint)
				upload_buffer_data[1] = static_cast<uint32_t>(device->m_development_shaders_commands_size - 2); //Total command size

				context->AddResourceBarriers({ display::ResourceBarrier(device->m_development_shaders_commands_buffer, display::TranstitionState::CopyDest, display::TranstitionState::UnorderedAccess) });
			}

			//If buffers have been recreated, build the descriptor
			if (descriptor_needs_update)
			{
				if (!device->m_development_shaders_descriptor_table.IsValid())
				{
					//Create the descriptor
					display::DescriptorTableDesc descriptors;
					descriptors.AddDescriptor(device->m_development_shaders_control_variables_buffer);
					descriptors.AddDescriptor(AsUAVBuffer(device->m_development_shaders_counters_buffer));
					descriptors.AddDescriptor(AsUAVBuffer(device->m_development_shaders_commands_buffer));
		
					device->m_development_shaders_descriptor_table = CreateDescriptorTable(device, descriptors);
				}
				else
				{
					//Update
					display::DescriptorTableDesc descriptors;
					descriptors.AddDescriptor(device->m_development_shaders_control_variables_buffer);
					descriptors.AddDescriptor(AsUAVBuffer(device->m_development_shaders_counters_buffer));
					descriptors.AddDescriptor(AsUAVBuffer(device->m_development_shaders_commands_buffer));
					UpdateDescriptorTable(device, device->m_development_shaders_descriptor_table, descriptors.descriptors.data(), descriptors.num_descriptors);
				}		
			}

			//Close command list
			CloseCommandList(device, context);

			//Execute the command list
			ExecuteCommandList(device, device->m_resource_command_list);
		}
	}

	void ReadBackDevelopmentShaderBuffer(display::Device* device)
	{
		if (device->m_development_shaders && device->m_development_shaders_counters_buffer.IsValid() && device->m_counters.size() > 0)
		{
			//Copy counters to the read back buffer
			{
				//Open command list
				display::Context* context = OpenCommandList(device, device->m_resource_command_list);

				context->AddResourceBarriers({ display::ResourceBarrier(device->m_development_shaders_counters_buffer, display::TranstitionState::UnorderedAccess, display::TranstitionState::CopySource) });

				context->CopyBuffer(device->m_development_shaders_counters_readback_buffer, 0, device->m_development_shaders_counters_buffer, 0, device->m_counters.size() * 4);

				context->AddResourceBarriers({ display::ResourceBarrier(device->m_development_shaders_counters_buffer, display::TranstitionState::CopySource, display::TranstitionState::UnorderedAccess) });
				
				//Close command list
				CloseCommandList(device, context);

				//Execute the command list
				ExecuteCommandList(device, device->m_resource_command_list);
			}

			//Update counters
			const uint32_t* counters_buffer = reinterpret_cast<const uint32_t*>(GetLastWrittenResourceMemoryBuffer(device, device->m_development_shaders_counters_readback_buffer));

			for (auto& counter : device->m_counters)
			{
				counter.second.counter.Set(counters_buffer[counter.second.index]);
			}
		}
		if (device->m_development_shaders && device->m_development_shaders_commands_buffer.IsValid())
		{
			//Copy commands to the read back buffer
			{
				//Open command list
				display::Context* context = OpenCommandList(device, device->m_resource_command_list);

				context->AddResourceBarriers({ display::ResourceBarrier(device->m_development_shaders_commands_buffer, display::TranstitionState::UnorderedAccess, display::TranstitionState::CopySource) });

				context->CopyBuffer(device->m_development_shaders_commands_readback_buffer, 0, device->m_development_shaders_commands_buffer, 0, device->m_development_shaders_commands_size * 4);

				context->AddResourceBarriers({ display::ResourceBarrier(device->m_development_shaders_commands_buffer, display::TranstitionState::CopySource, display::TranstitionState::UnorderedAccess) });

				//Close command list
				CloseCommandList(device, context);

				//Execute the command list
				ExecuteCommandList(device, device->m_resource_command_list);
			}

			//Read commands
			const uint32_t* commands_buffer = reinterpret_cast<const uint32_t*>(GetLastWrittenResourceMemoryBuffer(device, device->m_development_shaders_commands_readback_buffer));

			//Execute commands from the GPU (CurrentSize, MaxSize)
			//Command List:
			// ASSERT 0, FileID, LineNumber
			device->m_development_shaders_commands_used_size = commands_buffer[0];
			uint32_t commands_total = commands_buffer[0];

			//Merge asserts
			core::FastMap<AssertPosition, uint32_t> assert_merged;

			//Process commands to collect the information
			size_t command_offset = 2;
			while (command_offset < commands_total)
			{
				//Read command
				switch (commands_buffer[command_offset])
				{
				case 0: //ASSERT
				{
					AssertPosition assert_position{ commands_buffer[command_offset + 1], commands_buffer[command_offset + 2] };
					auto& it = assert_merged.Find(assert_position);
					if (it)
					{
						//Increase the assert found
						(*it)++;
					}
					else
					{
						assert_merged.Insert(assert_position, 0);
					}
					
					command_offset += 3;
					break;
				}
				default:
					core::LogWarning("Incorrect command ID read from the command development shaders");
					break;
				}
			}

			//Print the information in the log for each merged log
			assert_merged.VisitNamed([&](const AssertPosition& assert_position, const uint32_t& assert_count)
				{
					//Get the file name
					device->m_shader_files.VisitNamed([&](const std::string& file_name, const Device::ShaderFile& shader_file)
						{
							if (assert_position.shader_file_id == shader_file.file_id)
							{
								//Extract the line
								std::string line = GetLine(shader_file.source_code, assert_position.line_num);

								//Print
								core::LogWarning("GPU ASSERTs triggered: Shader<%s>, Line<%d>, Code<%s>, Count<%d>", file_name.c_str(), assert_position.line_num, line.c_str(), assert_count);
							}
						});
				});
		}
	}
}

//Access to platform::GetHwnd()
namespace platform
{
	extern HWND GetHwnd();
	extern void PresentCallback(display::Context* context);
}

namespace display
{
	Device* CreateDevice(const DeviceInitParams& params)
	{
		Device* device = new Device;

		UINT dxgiFactoryFlags = 0;

		// Enable the debug layer (requires the Graphics Tools "optional feature").
		// NOTE: Enabling the debug layer after device creation will invalidate the active device.
		if (params.debug)
		{
			ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();

				// Enable additional debug layers.
				dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
		}

		ComPtr<IDXGIFactory4> factory;
		if (FAILED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory))))
		{
			core::LogError("DX12 error creating the DXGI Factory");
			delete device;
			return nullptr;
		}

		ComPtr<IDXGIAdapter1> hardwareAdapter;
		GetHardwareAdapter(factory.Get(), &hardwareAdapter, params.adapter_index);

		//Get Adapter descriptor
		hardwareAdapter->GetDesc1(&device->m_adapter_desc);
		setlocale(LC_ALL, "en_US.utf8");
		size_t num_converted_characters;
		wcstombs_s(&num_converted_characters, device->m_adapter_description, device->m_adapter_desc.Description, 128);

		if (FAILED(D3D12CreateDevice(
			hardwareAdapter.Get(),
			D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(&device->m_native_device))))
		{
			core::LogError("DX12 error creating the device");
			delete device;
			return nullptr;
		}

		core::LogInfo("DX12 device created in adapter <%s>", device->m_adapter_description);

		// Describe and create the command queue.
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		if (FAILED(device->m_native_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&device->m_command_queue))))
		{
			core::LogError("DX12 error creating the command queue");
			delete device;
			return nullptr;
		}

		// Describe and create the swap chain.
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.BufferCount = static_cast<UINT>(params.num_frames);
		swapChainDesc.Width = static_cast<UINT>(params.width);
		swapChainDesc.Height = static_cast<UINT>(params.height);
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc.Count = 1;

		//Windows settings
		device->m_tearing = params.tearing;
		device->m_windowed = true;
		device->m_vsync = params.vsync;
		device->m_width = params.width;
		device->m_height = params.height;
		device->m_debug_shaders = params.debug_shaders;
		device->m_development_shaders = params.development_shaders;

		swapChainDesc.Flags = params.tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

		ComPtr<IDXGISwapChain1> swap_chain;
		if (FAILED(factory->CreateSwapChainForHwnd(
			device->m_command_queue.Get(),		// Swap chain needs the queue so that it can force a flush on it.
			platform::GetHwnd(),
			&swapChainDesc,
			nullptr,
			nullptr,
			&swap_chain)))
		{
			core::LogError("DX12 error creating the swap chain");
			delete device;
			return nullptr;
		}

		if (params.tearing)
		{
			// When tearing support is enabled we will handle ALT+Enter key presses in the
			// window message loop rather than let DXGI handle it by calling SetFullscreenState.
			factory->MakeWindowAssociation(platform::GetHwnd(), DXGI_MWA_NO_ALT_ENTER);
		}

		if (FAILED(swap_chain.As(&device->m_swap_chain)))
		{
			core::LogError("DX12 error copying the swap chain");
			delete device;
			return nullptr;
		}
		device->m_frame_index = device->m_swap_chain->GetCurrentBackBufferIndex();

		//Alloc pools
		D3D12_DESCRIPTOR_HEAP_TYPE render_target_heap_types[2] = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV , D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };
		device->m_command_list_pool.Init(500, 10, params.num_frames);
		device->m_root_signature_pool.Init(100, 10, params.num_frames);
		device->m_pipeline_state_pool.Init(2000, 100, params.num_frames);
		device->m_descriptor_table_pool.Init(2000, 100, params.num_frames, 8, device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		device->m_sampler_descriptor_table_pool.Init(200, 10, params.num_frames, 8, device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

		D3D12_DESCRIPTOR_HEAP_TYPE buffer_heap_types[2] = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV};
		device->m_buffer_pool.InitMultipleHeaps(100, 10, params.num_frames, device, 2, buffer_heap_types);

		D3D12_DESCRIPTOR_HEAP_TYPE texture_2d_heap_types[4] = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_TYPE_DSV };
		device->m_texture_2d_pool.InitMultipleHeaps(100, 10, params.num_frames, device, 4, texture_2d_heap_types);

		//Create frame resources
		device->m_frame_resources.resize(params.num_frames);

		//Alloc handle for the back buffer
		device->m_back_buffer_render_target = device->m_texture_2d_pool.Alloc();

		//Ring buffer
		Texture2DHandle* handle_ptr = &device->m_back_buffer_render_target;

		for (size_t i = 0; i < params.num_frames; ++i)
		{
			auto& frame_resource = device->m_frame_resources[i];

			//Create back buffer for each frame
			auto& render_target = device->m_texture_2d_pool[*handle_ptr];
			ThrowIfFailed(device->m_swap_chain->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&render_target.resource)));
			device->m_native_device->CreateRenderTargetView(render_target.resource.Get(), nullptr, device->m_texture_2d_pool.GetDescriptor(*handle_ptr, Texture2D::kRenderTargetDescriptorIndex));
			render_target.current_state = D3D12_RESOURCE_STATE_PRESENT;
			render_target.RenderTarget = true;
			render_target.name = "BackBuffer";
			SetObjectName(render_target.resource.Get(), "BackBuffer");

			ThrowIfFailed(device->m_native_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame_resource.command_allocator)));

			//Link to the next resource
			if (i != params.num_frames - 1)
			{
				render_target.next_handle = device->m_texture_2d_pool.Alloc();
				handle_ptr = &render_target.next_handle;
			}
		}

		// Create synchronization objects for deferred delete resources
		{
			ThrowIfFailed(device->m_native_device->CreateFence(device->m_resource_deferred_delete_index, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&device->m_resource_deferred_delete_fence)));
			device->m_resource_deferred_delete_index++;

			device->m_resource_deferred_delete_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (device->m_fence_event == nullptr)
			{
				ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
			}
		}

		//Create command lists
		{
			device->m_present_command_list = CreateCommandList(device, "Present");
			device->m_resource_command_list = CreateCommandList(device, "ResourceUploading");
		}

		// Create synchronization objects and wait until assets have been uploaded to the GPU.
		{
			//Create sync fences
			ThrowIfFailed(device->m_native_device->CreateFence(device->m_frame_resources[device->m_frame_index].fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&device->m_fence)));
			device->m_frame_resources[device->m_frame_index].fence_value++;

			// Create an event handle to use for frame synchronization.
			device->m_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (device->m_fence_event == nullptr)
			{
				ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
			}

			// Wait for the command list to execute; we are reusing the same command 
			// list in our main loop but for now, we just want to wait for setup to 
			// complete before continuing.
			WaitForGpu(device);
		}

		//Create indirect command signatures
		{
			D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[1] = {};
			argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

			D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
			commandSignatureDesc.pArgumentDescs = argumentDescs;
			commandSignatureDesc.NumArgumentDescs = _countof(argumentDescs);
			commandSignatureDesc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);

			ThrowIfFailed(device->m_native_device->CreateCommandSignature(&commandSignatureDesc, NULL, IID_PPV_ARGS(&device->m_indirect_draw_indexed_command_signature)));
		}
		{
			D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[1] = {};
			argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

			D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
			commandSignatureDesc.pArgumentDescs = argumentDescs;
			commandSignatureDesc.NumArgumentDescs = _countof(argumentDescs);
			commandSignatureDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);

			ThrowIfFailed(device->m_native_device->CreateCommandSignature(&commandSignatureDesc, NULL, IID_PPV_ARGS(&device->m_indirect_draw_indexed_instanced_command_signature)));
		}
		{
			D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[1] = {};
			argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;

			D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
			commandSignatureDesc.pArgumentDescs = argumentDescs;
			commandSignatureDesc.NumArgumentDescs = _countof(argumentDescs);
			commandSignatureDesc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);

			ThrowIfFailed(device->m_native_device->CreateCommandSignature(&commandSignatureDesc, NULL, IID_PPV_ARGS(&device->m_indirect_execute_compute_command_signature)));
		}

		//Create shader compilation
		HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&device->m_shader_utils));
		if (FAILED(hr))
		{
			core::LogError("DX12 error creating the DX utils");
		}

		hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&device->m_shader_compiler));
		if (FAILED(hr))
		{
			core::LogError("DX12 error creating the shader compiler");
		}

		hr = device->m_shader_utils->CreateDefaultIncludeHandler(device->m_shader_default_include_handler.GetAddressOf());
		if (FAILED(hr))
		{
			core::LogError("DX12 error creating the default include handler");
		}
	
		///Create memory allocator
		D3D12MA::ALLOCATOR_DESC allocator_desc = {};
		allocator_desc.pDevice = device->m_native_device.Get();
		allocator_desc.pAdapter = hardwareAdapter.Get();

		hr = D3D12MA::CreateAllocator(&allocator_desc, &device->m_allocator);
		if (FAILED(hr))
		{
			core::LogError("DX12 error creating memory allocator");
		}

		device->m_upload_buffer_max_size = params.upload_buffer_max_size;

		return device;
	}

	void DestroyDevice(Device* device)
	{
		// Ensure that the GPU is no longer referencing resources that are about to be
		// cleaned up by the destructor.
		WaitForGpu(device);

		//Destroy development shaders if built
		if (device->m_development_shaders_control_variables_buffer.IsValid())
			display::DestroyBuffer(device, device->m_development_shaders_control_variables_buffer);
		if (device->m_development_shaders_counters_buffer.IsValid())
			display::DestroyBuffer(device, device->m_development_shaders_counters_buffer);
		if (device->m_development_shaders_counters_readback_buffer.IsValid())
			display::DestroyBuffer(device, device->m_development_shaders_counters_readback_buffer);
		if (device->m_development_shaders_commands_buffer.IsValid())
			display::DestroyBuffer(device, device->m_development_shaders_commands_buffer);
		if (device->m_development_shaders_commands_readback_buffer.IsValid())
			display::DestroyBuffer(device, device->m_development_shaders_commands_readback_buffer);
		if (device->m_development_shaders_descriptor_table.IsValid())
			display::DestroyDescriptorTable(device, device->m_development_shaders_descriptor_table);

		//Destroy deferred delete resources
		DeletePendingResources(device);

		//Destroy upload buffer pool
		DestroyUploadBufferPool(device);

		CloseHandle(device->m_fence_event);
		CloseHandle(device->m_resource_deferred_delete_event);

		//Destroy back buffers
		DeleteRingResource(device, device->m_back_buffer_render_target, device->m_texture_2d_pool);

		//Destroy  command lists
		device->m_command_list_pool.Free(device->m_present_command_list);
		device->m_command_list_pool.Free(device->m_resource_command_list);

		//Destroy pools
		device->m_command_list_pool.Destroy();
		device->m_root_signature_pool.Destroy();
		device->m_pipeline_state_pool.Destroy();
		device->m_descriptor_table_pool.Destroy();
		device->m_sampler_descriptor_table_pool.Destroy();

		device->m_buffer_pool.Destroy();
		device->m_texture_2d_pool.Destroy();

		//Empty reload pipeline state buffers
		device->m_pipeline_reload_data.clear();

		//Release the allocator
		device->m_allocator->Release();

		delete device;
	}

	const char * GetLastErrorMessage(Device * device)
	{
		return device->m_last_error_message;
	}

	//Change size
	void ChangeWindowSize(Device * device, size_t width, size_t height, bool minimized)
	{
		// Determine if the swap buffers and other resources need to be resized or not.
		if ((width != device->m_width || height != device->m_height) && !minimized)
		{
			// Flush all current GPU commands.
			WaitForGpu(device);

			// Release the resources holding references to the swap chain (requirement of
			// IDXGISwapChain::ResizeBuffers) and reset the frame fence values to the
			// current fence value.
			WeakTexture2DHandle back_buffer_handle = device->m_back_buffer_render_target;
			for (size_t i = 0; i < device->m_frame_resources.size(); ++i)
			{
				device->Get(GetRingResource(device, back_buffer_handle, i)).resource.Reset();
				device->m_frame_resources[i].fence_value = device->m_frame_resources[device->m_frame_index].fence_value;
			}

			// Resize the swap chain to the desired dimensions.
			DXGI_SWAP_CHAIN_DESC desc = {};
			device->m_swap_chain->GetDesc(&desc);
			ThrowIfFailed(device->m_swap_chain->ResizeBuffers(static_cast<UINT>(device->m_frame_resources.size()),
				static_cast<UINT>(width), static_cast<UINT>(height), desc.BufferDesc.Format, desc.Flags));

			BOOL fullscreenState;
			ThrowIfFailed(device->m_swap_chain->GetFullscreenState(&fullscreenState, nullptr));
			device->m_windowed = !fullscreenState;

			//Recapture the back buffers
			for (size_t i = 0; i < device->m_frame_resources.size(); ++i)
			{
				auto& frame_resource = device->m_frame_resources[i];
				auto& frame_back_buffer_handle = GetRingResource(device, back_buffer_handle, i);
				auto& render_target = device->Get(frame_back_buffer_handle);
				ThrowIfFailed(device->m_swap_chain->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&render_target.resource)));
				device->m_native_device->CreateRenderTargetView(render_target.resource.Get(), nullptr, device->m_texture_2d_pool.GetDescriptor(frame_back_buffer_handle, Texture2D::kRenderTargetDescriptorIndex));
				render_target.current_state = D3D12_RESOURCE_STATE_PRESENT;
			}

			// Reset the frame index to the current back buffer index.
			device->m_frame_index = device->m_swap_chain->GetCurrentBackBufferIndex();

			device->m_width = width;
			device->m_height = height;
		}
	}

	//Is tearing enabled
	bool IsTearingEnabled(Device* device)
	{
		return device->m_tearing;
	}

	//Used for the fullscreen tearing implementation
	bool GetCurrentDisplayRect(Device* device, Rect& rect)
	{
		try
		{
			if (device->m_swap_chain)
			{
				RECT fullscreenWindowRect;
				ComPtr<IDXGIOutput> pOutput;
				ThrowIfFailed(device->m_swap_chain->GetContainingOutput(&pOutput));
				DXGI_OUTPUT_DESC Desc;
				ThrowIfFailed(pOutput->GetDesc(&Desc));
				fullscreenWindowRect = Desc.DesktopCoordinates;

				rect.bottom = fullscreenWindowRect.bottom;
				rect.top = fullscreenWindowRect.top;
				rect.left = fullscreenWindowRect.left;
				rect.right = fullscreenWindowRect.right;

				return true;
			}
		}
		catch (...)
		{

		}

		return false;
	}

	//Present
	void Present(Device* device)
	{
		Context* context = OpenCommandList(device, device->m_present_command_list);
		{
			PROFILE_SCOPE_GPU(context, "Display", 0xFFFF00FF, "Present");

			auto dx12_context = reinterpret_cast<DX12Context*>(context);

			auto& command_list = dx12_context->command_list;

			//Call framework to render UI/Debug
			platform::PresentCallback(context);

			// Indicate that the back buffer will now be used to present.
			auto& back_buffer = device->Get(GetRingResource(device, WeakTexture2DHandle(device->m_back_buffer_render_target), device->m_frame_index));
			if (back_buffer.current_state != D3D12_RESOURCE_STATE_PRESENT)
			{
				command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(back_buffer.resource.Get(), back_buffer.current_state, D3D12_RESOURCE_STATE_PRESENT));
				back_buffer.current_state = D3D12_RESOURCE_STATE_PRESENT;
			}
		}
		CloseCommandList(device, context);

		//Execute command list
		ExecuteCommandList(device, device->m_present_command_list);

		// When using sync interval 0, it is recommended to always pass the tearing
		// flag when it is supported, even when presenting in windowed mode.
		// However, this flag cannot be used if the app is in fullscreen mode as a
		// result of calling SetFullscreenState.
		UINT sync_interval = (device->m_vsync) ? 1: 0;
		UINT present_flags = (device->m_tearing && device->m_windowed && !device->m_vsync) ? DXGI_PRESENT_ALLOW_TEARING : 0;

		// Present the frame.
		ThrowIfFailed(device->m_swap_chain->Present(sync_interval, present_flags));

		MoveToNextFrame(device);
	}

	//Begin/End Frame
	void BeginFrame(Device* device)
	{
		//During the first frame, we should not wait in the begin frame, as resource upload can happen before
		if (!device->m_before_first_frame)
		{
			// Command list allocators can only be reset when the associated 
			// command lists have finished execution on the GPU; apps should use 
			// fences to determine GPU execution progress.
			ThrowIfFailed(GetCommandAllocator(device)->Reset());
		}
		else
		{
			device->m_before_first_frame = false;
		}

		//Delete deferred handles
		device->m_command_list_pool.NextFrame();
		device->m_root_signature_pool.NextFrame();
		device->m_pipeline_state_pool.NextFrame();
		device->m_descriptor_table_pool.NextFrame();
		device->m_sampler_descriptor_table_pool.NextFrame();

		device->m_buffer_pool.NextFrame();
		device->m_texture_2d_pool.NextFrame();

		//Delete deferred resources
		DeletePendingResources(device);

		//Reset all the active upload buffers
		UploadBufferReset(device);

		//Upload development shaders buffer
		UploadDevelopmentShaderBuffer(device);

		//Reset stats
		device->uploaded_memory_frame = 0;
	}

	void EndFrame(Device* device)
	{
		//Readback development shaders
		ReadBackDevelopmentShaderBuffer(device);
	}

	uint64_t GetLastCompletedGPUFrame(Device* device)
	{
		//Returns the value of the frame fence, means that GPU is done with the returned frame
		//It removes the increments of the fence because WaitToGPU
		return static_cast<uint64_t>(device->m_fence->GetCompletedValue()) - device->m_fence_wait_offset;
	}

	//Context
	CommandListHandle CreateCommandList(Device* device, const char* name)
	{
		CommandListHandle handle = device->m_command_list_pool.Alloc();
		ThrowIfFailed(device->m_native_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, GetCommandAllocator(device).Get(), nullptr, IID_PPV_ARGS(&device->Get(handle).resource)));

		// Command lists are created in the recording state, but there is nothing
		// to record yet. The main loop expects it to be closed, so close it now.
		ThrowIfFailed(device->Get(handle).resource->Close());

		SetObjectName(device->Get(handle).resource.Get(),name);

		return handle;
	}
	void DestroyCommandList(Device* device, CommandListHandle& handle)
	{
		device->m_command_list_pool.Free(handle);
	}

	//Open context, begin recording
	Context*  OpenCommandList(Device* device, const WeakCommandListHandle& handle)
	{
		auto& command_list = device->Get(handle).resource;
		// However, when ExecuteCommandList() is called on a particular command 
		// list, that command list can then be reset at any time and must be before 
		// re-recording.
		ThrowIfFailed(command_list->Reset(GetCommandAllocator(device).Get(), nullptr));

		//Set descriptor table heaps
		ID3D12DescriptorHeap* descriptor_table[1];
		descriptor_table[0] = device->m_descriptor_table_pool.GetHeap();
		command_list->SetDescriptorHeaps(1, descriptor_table);

		//Create a new context
		DX12Context* context = device->m_context_pool.Alloc();
		context->device = device;
		context->command_list = command_list;
		context->current_graphics_root_signature = WeakRootSignatureHandle();
		context->current_compute_root_signature = WeakRootSignatureHandle();

		return context;
	}
	//Close context, stop recording
	void CloseCommandList(Device* device, Context* context)
	{
		assert(context);
		auto dx12_context = reinterpret_cast<DX12Context*>(context);
		
		dx12_context->command_list->Close();

		//Delete
		device->m_context_pool.Free(dx12_context);
	}

	void ExecuteCommandList(Device * device, const WeakCommandListHandle& handle)
	{
		auto& command_list = device->Get(handle).resource;

		// Execute the command list.
		ID3D12CommandList* ppCommandLists[] = { command_list.Get()};
		device->m_command_queue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	}

	void ExecuteCommandLists(Device* device, const std::vector<WeakCommandListHandle>& handles)
	{
		std::vector<ID3D12CommandList*> command_lists;
		command_lists.resize(handles.size());

		for (size_t i = 0; i < handles.size(); ++i)
		{
			command_lists[i] = device->Get(handles[i]).resource.Get();
		}

		// Execute the command lists
		device->m_command_queue->ExecuteCommandLists(static_cast<UINT>(command_lists.size()), command_lists.data());
	}

	//Get back buffer (ring resource)
	WeakTexture2DHandle GetBackBuffer(Device* device)
	{
		return device->m_back_buffer_render_target;
	}

	RootSignatureHandle CreateRootSignature(Device * device, const RootSignatureDesc& root_signature_desc, const char* name)
	{
		RootSignatureHandle handle = device->m_root_signature_pool.Alloc();

		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

		// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (FAILED(device->m_native_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}


		D3D12_STATIC_SAMPLER_DESC static_samplers[kMaxNumStaticSamplers] = {};
		for (size_t i = 0; i < root_signature_desc.num_static_samplers; ++i)
		{
			static_samplers[i] = Convert(root_signature_desc.static_samplers[i]);
		}

		CD3DX12_ROOT_PARAMETER1 root_parameters[kMaxNumRootParameters];
		CD3DX12_DESCRIPTOR_RANGE1 range[kMaxNumRootParameters][RootSignatureTable::kNumMaxRanges];
		for (size_t i = 0; i < root_signature_desc.num_root_parameters; ++i)
		{
			auto& source_property = root_signature_desc.root_parameters[i];
			switch (source_property.type)
			{
			case RootSignatureParameterType::Constants:
				root_parameters[i].InitAsConstants(static_cast<UINT>(source_property.root_param.num_constants),static_cast<UINT>(source_property.root_param.shader_register), 0, Convert(source_property.visibility));
				break;
			case RootSignatureParameterType::ConstantBuffer:
				root_parameters[i].InitAsConstantBufferView(static_cast<UINT>(source_property.root_param.shader_register), 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, Convert(source_property.visibility));
				break;
			case RootSignatureParameterType::UnorderedAccessBuffer:
				root_parameters[i].InitAsUnorderedAccessView(static_cast<UINT>(source_property.root_param.shader_register), 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, Convert(source_property.visibility));
				break;
			case RootSignatureParameterType::ShaderResource:
				root_parameters[i].InitAsShaderResourceView(static_cast<UINT>(source_property.root_param.shader_register), 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, Convert(source_property.visibility));
				break;
			case RootSignatureParameterType::DescriptorTable:
				{
					for (size_t range_index = 0; range_index < source_property.table.num_ranges; ++range_index)
					{
						auto& range_desc = source_property.table.range[range_index];
						range[i][range_index].Init(Convert(range_desc.type), static_cast<UINT>(range_desc.size), static_cast<UINT>(range_desc.base_shader_register));
					}
				
					root_parameters[i].InitAsDescriptorTable(static_cast<UINT>(source_property.table.num_ranges), &range[i][0], Convert(source_property.visibility));
				}
				break;
			}	
		}

		if (device->m_development_shaders)
		{
			range[root_signature_desc.num_root_parameters][0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1000);
			range[root_signature_desc.num_root_parameters][1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 1000);

			//Create slot for the descriptor table that represent the shader development
			root_parameters[root_signature_desc.num_root_parameters].InitAsDescriptorTable(2, &range[root_signature_desc.num_root_parameters][0], D3D12_SHADER_VISIBILITY_ALL);
		}

		D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		UINT num_root_parameters = static_cast<UINT>(root_signature_desc.num_root_parameters);
		if (device->m_development_shaders)
		{
			num_root_parameters++;
		}
		rootSignatureDesc.Init_1_1(num_root_parameters, root_parameters, static_cast<UINT>(root_signature_desc.num_static_samplers), static_samplers, flags);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		if (FAILED(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error)))
		{
			device->m_root_signature_pool.Free(handle);
			SetLastErrorMessage(device, "Error serializing root signature <%s>", reinterpret_cast<const char*>(error->GetBufferPointer()));
			return RootSignatureHandle();
		}
		if (FAILED(device->m_native_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&device->Get(handle).resource))))
		{
			device->m_root_signature_pool.Free(handle);
			SetLastErrorMessage(device, "Error creating root signature <%s>", name);
			return RootSignatureHandle();
		}
		
		device->Get(handle).desc = root_signature_desc;

		SetObjectName(device->Get(handle).resource.Get(), name);

		return handle;
	}

	void DestroyRootSignature(Device * device, RootSignatureHandle & root_signature_handle)
	{
		device->m_root_signature_pool.Free(root_signature_handle);
	}

	static std::vector<char> ReadFileToBuffer(const char* file)
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
	static std::wstring FromChar(const char* input)
	{
		return std::wstring(input, input + strlen(input));
	}
	static std::string ToChar(std::wstring input)
	{
		std::string str;
		std::transform(input.begin(), input.end(), std::back_inserter(str), [](wchar_t c) {
			return (char)c;
			});

		return str;
	}

	class CustomIncludeHandler : public IDxcIncludeHandler
	{
		Device* m_device;
		std::unordered_set<std::string>* m_included_files;
	public:
		HRESULT STDMETHODCALLTYPE LoadSource(_In_ LPCWSTR pFilename, _COM_Outptr_result_maybenull_ IDxcBlob** ppIncludeSource) override
		{
			ComPtr<IDxcBlobEncoding> pEncoding;
			HRESULT hr = m_device->m_shader_utils->LoadFile(pFilename, nullptr, pEncoding.GetAddressOf());
			if (SUCCEEDED(hr))
			{
				m_included_files->insert(ToChar(pFilename));
				*ppIncludeSource = pEncoding.Detach();
			}
			else
			{
				*ppIncludeSource = nullptr;
			}
			return hr;
		}

		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) override
		{
			return m_device->m_shader_default_include_handler->QueryInterface(riid, ppvObject);
		}

		ULONG STDMETHODCALLTYPE AddRef(void) override { return 0; }
		ULONG STDMETHODCALLTYPE Release(void) override { return 0; }

		CustomIncludeHandler(Device* device, std::unordered_set<std::string>* included_files) : m_device(device), m_included_files(included_files)
		{
		}
	};

	static bool CompileShader(Device* device, const CompileShaderDesc& compile_shader_desc, ComPtr<IDxcBlob>& shader_blob, std::unordered_set<std::string>& include_set, bool allow_reload = true)
	{
		ComPtr<ID3DBlob> blob;

		std::unique_ptr<D3D_SHADER_MACRO[]> defines;
		if (compile_shader_desc.defines.size() > 0)
		{
			defines = std::make_unique<D3D_SHADER_MACRO[]>(compile_shader_desc.defines.size() + 1);
			memcpy(defines.get(), compile_shader_desc.defines.data(), sizeof(D3D_SHADER_MACRO) * compile_shader_desc.defines.size());
			//Add null terminated
			defines.get()[compile_shader_desc.defines.size()].Name = nullptr;
			defines.get()[compile_shader_desc.defines.size()].Definition = nullptr;
		}

		bool reload = true;

		//It allows to reload during init
		do
		{
			HRESULT hr;
			DxcBuffer source_buffer;
			source_buffer.Encoding = 0;
			std::vector<char> source_shader_blob;

			if (compile_shader_desc.file_name)
			{
				//Use the file name
				source_shader_blob = ReadFileToBuffer(compile_shader_desc.file_name);
				source_buffer.Ptr = source_shader_blob.data();
				source_buffer.Size = source_shader_blob.size();
			}
			else if (compile_shader_desc.shader_code)
			{
				source_buffer.Ptr = compile_shader_desc.shader_code;
				source_buffer.Size = strnlen_s(compile_shader_desc.shader_code, 512 * 1024);
			}
			else
			{
				//Error compiling
				SetLastErrorMessage(device, "Error compiling shader <%s>, filename or shader_code was not defined", compile_shader_desc.name);
				return false;
			}

			std::string shader_prefix;

			//Functions needed for all shaders
			shader_prefix += "bool IsNaN(float a) { return (asuint(a) & 0x7fffffff) > 0x7f800000; };";
			shader_prefix += "bool2 IsNaN(float2 a) { return bool2(IsNaN(a.x), IsNaN(a.y)); };";
			shader_prefix += "bool3 IsNaN(float3 a) { return bool3(IsNaN(a.x), IsNaN(a.y), IsNaN(a.z)); };";
			shader_prefix += "bool4 IsNaN(float4 a) { return bool4(IsNaN(a.x), IsNaN(a.y), IsNaN(a.z), IsNaN(a.w)); };";

			if (device->m_development_shaders)
			{
				//Calculate the shader file id
				uint32_t shader_file_id = 0xFFFFFFFF; //Internal, no filename
				if (compile_shader_desc.file_name)
				{
					auto& it = device->m_shader_files.Find(compile_shader_desc.file_name);
					if (it)
					{
						//Just update the source code and get the shader file id
						shader_file_id = it->file_id;
						it->source_code = std::string(reinterpret_cast<const char*>(source_buffer.Ptr), source_buffer.Size);
					}
					else
					{
						//Needs a new one
						shader_file_id = static_cast<uint32_t>(device->m_shader_files.size() + 1);
						std::string source_code;
						source_code.append(reinterpret_cast<const char*>(source_buffer.Ptr), source_buffer.Size);
						device->m_shader_files.Insert(compile_shader_desc.file_name, Device::ShaderFile{ std::move(source_code), shader_file_id });
					}
				}


				//Define the shader development buffers
				shader_prefix += "StructuredBuffer<uint> ControlVariablesBuffer : register(t1000);\n";
				shader_prefix += "RWStructuredBuffer<uint> CountersBuffer : register(u1000);\n";
				shader_prefix += "RWStructuredBuffer<uint> CommandsBuffer : register(u1001);\n";

				//CONTROL VARIABLES
				{
					//Undefine then, are going to be define as offsets in a buffer
					shader_prefix += "#define CONTROL_VARIABLE(type, group, name, default_value)\n";

					//Look for control variables "CONTROL_VARIABLE(" in the shader code
					std::string shader_code = reinterpret_cast<const char*>(source_buffer.Ptr);

					std::regex pattern("CONTROL_VARIABLE\\((\\w+), (\\w+), (\\w+), (\\w+)\\)");
					std::smatch match;

					while (std::regex_search(shader_code, match, pattern))
					{
						//Collect control variable name and type

						const std::string& type_variable_string = match[1].str();
						const std::string& group = match[2].str();
						const std::string& control_variable = match[3].str();
						const std::string& default_value_string = match[4].str();

						std::variant<float, uint32_t, bool> default_value;
						if (type_variable_string == "float")
						{
							default_value = std::stof(default_value_string);
						}
						else if (type_variable_string == "uint")
						{
							default_value = (uint32_t)std::stoul(default_value_string);
						}
						else if (type_variable_string == "bool")
						{
							default_value = (default_value_string == "true");
						}
						else
						{
							default_value = false;
							core::LogWarning("The control variable <%s> type <%s> is not a supported one (float, uint, bool). Reset to bool.", control_variable.c_str(), type_variable_string.c_str());
						}

						auto control_variable_item = device->m_control_variables.Find(control_variable);
						size_t index;
						if (control_variable_item)
						{
							//Check if the default variable is ok
							if (default_value != control_variable_item->default_value)
							{
								core::LogWarning("The control variable <%s> is define with different default values in different shaders. The behaviour is undefined.", control_variable.c_str());
							}
							index = control_variable_item->index;
						}
						else
						{
							//We need to add a new element to the map
							index = device->m_control_variables.size();

							switch (default_value.index())
							{
							case 0:
								device->m_control_variables.Insert(control_variable, Device::ControlVariable{ index, default_value, core::RegisterControlVariable(std::get<float>(default_value), core::ControlVariableGroupName(group.c_str()), core::ControlVariableName(control_variable.c_str()), core::ConsoleVariableType::Render) });
								break;
							case 1:
								device->m_control_variables.Insert(control_variable, Device::ControlVariable{ index, default_value, core::RegisterControlVariable(std::get<uint32_t>(default_value), core::ControlVariableGroupName(group.c_str()), core::ControlVariableName(control_variable.c_str()), core::ConsoleVariableType::Render) });
								break;
							case 2:
								device->m_control_variables.Insert(control_variable, Device::ControlVariable{ index, default_value, core::RegisterControlVariable(std::get<bool>(default_value), core::ControlVariableGroupName(group.c_str()), core::ControlVariableName(control_variable.c_str()), core::ConsoleVariableType::Render) });
								break;
							}
						}

						switch (default_value.index())
						{
						case 0:
							shader_prefix += std::string("#define ") + control_variable + " asfloat(ControlVariablesBuffer[" + std::to_string(index) + "])\n";
							break;
						case 1:
							shader_prefix += std::string("#define ") + control_variable + " ControlVariablesBuffer[" + std::to_string(index) + "]\n";
							break;
						case 2:
							shader_prefix += std::string("#define ") + control_variable + " (ControlVariablesBuffer[" + std::to_string(index) + "] != 0)\n";
							break;
						}

						//Continue to the rest of the shader
						shader_code = match.suffix();
					}
				}

				//COUNTERS
				{
					//Look for control variables "COUNTER(" in the shader code
					std::string shader_code = reinterpret_cast<const char*>(source_buffer.Ptr);

					std::regex pattern("COUNTER\\((\\w+), (\\w+)\\)");
					std::smatch match;

					//Undefine then, are going to be define as offsets in a buffer
					shader_prefix += "#define COUNTER(group, variable)\n";

					while (std::regex_search(shader_code, match, pattern))
					{
						//Collect control variable
						const std::string& group = match[1].str();
						const std::string& variable = match[2].str();

						auto stat_index = device->m_counters.Find(variable);
						size_t index;
						if (stat_index)
						{
							index = stat_index->index;
						}
						else
						{
							//We need to add a new element to the map
							index = device->m_counters.size();
							device->m_counters.Insert(variable, Device::Counter{ index, core::CounterGroupName(group.c_str()), core::CounterName(variable.c_str()), core::CounterType::Render, true });
						}

						shader_prefix += std::string("#define ") + variable + "_index " + std::to_string(index) + "\n";

						//Continue to the rest of the shader
						shader_code = match.suffix();
					}

					//Define stats operators
					shader_prefix += "#define COUNTER_INC(variable) {uint retvalue; InterlockedAdd(CountersBuffer[variable##_index], 1, retvalue);}\n";
					shader_prefix += "#define COUNTER_INC_VALUE(variable, value) {uint retvalue; InterlockedAdd(CountersBuffer[variable##_index], value, retvalue);}\n";
				}
				//ASSERTS
				{
					//Asserts are command zero and it has two more parameters (fileID and line)
					shader_prefix += "#define assert(test) {if (!(test)){ uint offset_command; InterlockedAdd(CommandsBuffer[0], 3, offset_command); if ((offset_command + 3) < CommandsBuffer[1]){ CommandsBuffer[offset_command] = 0; CommandsBuffer[offset_command + 1] = " +
						std::to_string(shader_file_id) + "; CommandsBuffer[offset_command + 2] = __LINE__;} else {InterlockedAdd(CommandsBuffer[0], -3, offset_command);} }}\n";
				}
				//Reset the line count, so the assert can work
				const char* file_name = (compile_shader_desc.file_name) ? compile_shader_desc.file_name : compile_shader_desc.name;
				shader_prefix += std::string("#line 0 \"") + file_name + "\"\n";
			}
			else
			{
				//CONTROL VARIABLES
				shader_prefix += "#define CONTROL_VARIABLE(type, name, default_value) const type name = defaul_value;\n";
				//STATS
				shader_prefix += "#define COUNTER(variable, group, name)\n";
				shader_prefix += "#define COUNTER_INC(variable)\n";
				shader_prefix += "#define COUNTER_INC_VALUE(variable, value)\n";
				//ASSERTS
				shader_prefix += "#define assert(test)\n";

				//Reset the line count
				const char* file_name = (compile_shader_desc.file_name) ? compile_shader_desc.file_name : compile_shader_desc.name;
				shader_prefix += std::string("#line 0 \"") + file_name + "\"\n";
			}

			//Add the shader prefix before the code
			shader_prefix.append(reinterpret_cast<const char*>(source_buffer.Ptr), source_buffer.Size);

			source_buffer.Ptr = shader_prefix.data();
			source_buffer.Size = shader_prefix.size();

			std::vector<LPCWSTR> arguments;
			std::wstring entry_point = FromChar(compile_shader_desc.entry_point);
			std::wstring target = FromChar(compile_shader_desc.target);

			arguments.push_back(L"-E");
			arguments.push_back(entry_point.c_str());

			arguments.push_back(L"-T");
			arguments.push_back(target.c_str());

			arguments.push_back(DXC_ARG_WARNINGS_ARE_ERRORS); //-WX

			if (device->m_debug_shaders)
			{
				arguments.push_back(L"-Qembed_debug");
				arguments.push_back(DXC_ARG_DEBUG); //-Zi
				arguments.push_back(DXC_ARG_DEBUG_NAME_FOR_SOURCE);
			}

			if (device->m_development_shaders)
			{
				arguments.push_back(DXC_ARG_IEEE_STRICTNESS); //-Gis
			}

			std::vector<std::wstring> defines_array;
			for (auto& define_it : compile_shader_desc.defines)
			{
				defines_array.push_back(FromChar(define_it.first) + L"=" + FromChar(define_it.second));
			}

			for (auto& define : defines_array)
			{
				arguments.push_back(L"-D");
				arguments.push_back(define.c_str());
			}

			//Create custom include handler
			CustomIncludeHandler custom_include_handler(device, &include_set);

			ComPtr<IDxcResult> shader_results;
			hr = device->m_shader_compiler->Compile(&source_buffer, arguments.data(), static_cast<uint32_t>(arguments.size()), &custom_include_handler, IID_PPV_ARGS(shader_results.GetAddressOf()));

			if (SUCCEEDED(hr))
			{
				//Get status
				shader_results->GetStatus(&hr);

				if (SUCCEEDED(hr))
				{
					shader_results->GetResult(&shader_blob);

					return true;
				}
				else
				{
					//Error Handling
					ComPtr<IDxcBlobUtf8> errors;
					shader_results->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errors.GetAddressOf()), nullptr);
					if (errors && errors->GetStringLength() > 0)
					{
						//Error compiling
						SetLastErrorMessage(device, "Error compiling shader <%s> errors <%s>", compile_shader_desc.name, static_cast<char*>(errors->GetBufferPointer()));
					}

					if (allow_reload)
					{

						std::string msg = std::string("Error compiling shader ") + compile_shader_desc.name + "with errors:\n" + static_cast<char*>(errors->GetBufferPointer()) + "\n";
						msg += "Do you want to retry to load the shader?\n";

						reload = platform::ShowModalDialog("Shader Compilation failed during init", msg.c_str());
					}
					else
					{
						reload = false;
					}
				}
			}
			else
			{
				//Error compiling
				SetLastErrorMessage(device, "Error compiling shader <%s>, invalid arguments", compile_shader_desc.name);
				//This error doesn't support reloading
				reload = false;
			}
		}
		while (reload && allow_reload && device->m_development_shaders);

		return false;
	}

	PipelineStateHandle CreatePipelineState(Device * device, const PipelineStateDesc & pipeline_state_desc, const char* name)
	{
		PipelineStateHandle handle = device->m_pipeline_state_pool.Alloc();

		auto& pipeline_state = device->Get(handle);

		//Fill the DX12 structs using our data, keep it for reloading
		D3D12_GRAPHICS_PIPELINE_STATE_DESC DX12_pipeline_state_desc = { 0 };

		std::vector<D3D12_INPUT_ELEMENT_DESC> input_elements;
		for (size_t i = 0; i < pipeline_state_desc.input_layout.num_elements; i++)
		{
			auto& source_input_element = pipeline_state_desc.input_layout.elements[i];

			D3D12_INPUT_ELEMENT_DESC input_element;
			input_element.SemanticName = source_input_element.semantic_name;
			input_element.SemanticIndex = static_cast<UINT>(source_input_element.semantic_index);
			input_element.Format = Convert(source_input_element.format);
			input_element.InputSlot = static_cast<UINT>(source_input_element.input_slot);
			input_element.AlignedByteOffset = static_cast<UINT>(source_input_element.aligned_offset);
			input_element.InputSlotClass = Convert(source_input_element.input_type);
			input_element.InstanceDataStepRate = static_cast<UINT>(source_input_element.instance_step_rate);

			input_elements.push_back(input_element);
		}
		DX12_pipeline_state_desc.InputLayout = { input_elements.data(), static_cast<UINT>(pipeline_state_desc.input_layout.num_elements)};

		DX12_pipeline_state_desc.pRootSignature = device->Get(pipeline_state_desc.root_signature).resource.Get();

		std::unordered_set<std::string> vertex_shader_include_set;

		ComPtr<IDxcBlob> vertex_shader_blob;
		if (!CompileShader(device, pipeline_state_desc.vertex_shader, vertex_shader_blob, vertex_shader_include_set))
		{
			device->m_pipeline_state_pool.Free(handle);
			SetLastErrorMessage(device, "Error compiling vertex shader in graphics pipeline state <%s>", name);
			return PipelineStateHandle();
		}

		DX12_pipeline_state_desc.VS.pShaderBytecode = vertex_shader_blob->GetBufferPointer();
		DX12_pipeline_state_desc.VS.BytecodeLength = vertex_shader_blob->GetBufferSize();

		std::unordered_set<std::string> pixel_shader_include_set;

		ComPtr<IDxcBlob> pixel_shader_blob;
		if (!CompileShader(device, pipeline_state_desc.pixel_shader, pixel_shader_blob, pixel_shader_include_set))
		{
			device->m_pipeline_state_pool.Free(handle);
			SetLastErrorMessage(device, "Error compiling pixel shader in graphics pipeline state <%s>", name);
			return PipelineStateHandle();
		}

		DX12_pipeline_state_desc.PS.pShaderBytecode = pixel_shader_blob->GetBufferPointer();
		DX12_pipeline_state_desc.PS.BytecodeLength = pixel_shader_blob->GetBufferSize();
		
		D3D12_RASTERIZER_DESC rasterizer_state;
		rasterizer_state.FillMode = Convert(pipeline_state_desc.rasteritation_state.fill_mode);
		rasterizer_state.CullMode = Convert(pipeline_state_desc.rasteritation_state.cull_mode);
		rasterizer_state.FrontCounterClockwise = true;
		rasterizer_state.DepthBias = static_cast<UINT>(pipeline_state_desc.rasteritation_state.depth_bias);
		rasterizer_state.DepthBiasClamp = pipeline_state_desc.rasteritation_state.depth_bias_clamp;
		rasterizer_state.SlopeScaledDepthBias = pipeline_state_desc.rasteritation_state.slope_depth_bias;
		rasterizer_state.DepthClipEnable = pipeline_state_desc.rasteritation_state.depth_clip_enable;
		rasterizer_state.MultisampleEnable = pipeline_state_desc.rasteritation_state.multisample_enable;
		rasterizer_state.AntialiasedLineEnable = false;
		rasterizer_state.ForcedSampleCount = static_cast<UINT>(pipeline_state_desc.rasteritation_state.forced_sample_count);
		rasterizer_state.ConservativeRaster = (pipeline_state_desc.rasteritation_state.convervative_mode) ? D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON : D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;					
		rasterizer_state.AntialiasedLineEnable = pipeline_state_desc.antialiasing_lines;
		DX12_pipeline_state_desc.RasterizerState = rasterizer_state;

		D3D12_BLEND_DESC blend_desc;
		blend_desc.AlphaToCoverageEnable = pipeline_state_desc.blend_desc.alpha_to_coverage_enable;
		blend_desc.IndependentBlendEnable = pipeline_state_desc.blend_desc.independent_blend_enable;
		for (size_t i = 0; i < kMaxNumRenderTargets; i++)
		{
			auto& dest = blend_desc.RenderTarget[i];
			const auto& source = pipeline_state_desc.blend_desc.render_target_blend[i];
			
			dest.BlendEnable = source.blend_enable;
			dest.LogicOpEnable = false;
			dest.SrcBlend = Convert(source.src_blend);
			dest.DestBlend = Convert(source.dest_blend);
			dest.BlendOp = Convert(source.blend_op);
			dest.SrcBlendAlpha = Convert(source.alpha_src_blend);
			dest.DestBlendAlpha = Convert(source.alpha_dest_blend);
			dest.BlendOpAlpha = Convert(source.alpha_blend_op);
			dest.LogicOp = D3D12_LOGIC_OP_NOOP;
			dest.RenderTargetWriteMask = source.write_mask;
		}
		DX12_pipeline_state_desc.BlendState = blend_desc;

		DX12_pipeline_state_desc.DepthStencilState.DepthEnable = pipeline_state_desc.depth_enable;
		DX12_pipeline_state_desc.DepthStencilState.DepthFunc = Convert(pipeline_state_desc.depth_func);
		DX12_pipeline_state_desc.DepthStencilState.DepthWriteMask = (pipeline_state_desc.depth_write) ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		
		DX12_pipeline_state_desc.DepthStencilState.StencilEnable = pipeline_state_desc.stencil_enable;

		DX12_pipeline_state_desc.SampleMask = UINT_MAX;
		
		DX12_pipeline_state_desc.PrimitiveTopologyType = Convert(pipeline_state_desc.primitive_topology_type);

		DX12_pipeline_state_desc.NumRenderTargets = static_cast<UINT>(pipeline_state_desc.num_render_targets);
		for (size_t i = 0; i < kMaxNumRenderTargets; i++)
		{
			if (i < pipeline_state_desc.num_render_targets)
			{
				DX12_pipeline_state_desc.RTVFormats[i] = Convert(pipeline_state_desc.render_target_format[i]);
			}
			else
			{
				DX12_pipeline_state_desc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
			}
		}
		DX12_pipeline_state_desc.DSVFormat = Convert(pipeline_state_desc.depth_stencil_format);
		DX12_pipeline_state_desc.SampleDesc.Count = static_cast<UINT>(pipeline_state_desc.sample_count);

		//Create pipeline state
		if (FAILED(device->m_native_device->CreateGraphicsPipelineState(&DX12_pipeline_state_desc, IID_PPV_ARGS(&pipeline_state.resource))))
		{
			device->m_pipeline_state_pool.Free(handle);
			SetLastErrorMessage(device, "Error creating graphics pipeline state <%s>", name);
			return PipelineStateHandle();
		}

		SetObjectName(pipeline_state.resource.Get(), name);
		
		//Create the reload data
		device->m_pipeline_reload_data.emplace_back(name, handle, pipeline_state_desc, DX12_pipeline_state_desc, input_elements, vertex_shader_include_set, pixel_shader_include_set);

		return handle;
	}

	void DestroyPipelineState(Device * device, PipelineStateHandle & handle)
	{
		device->m_pipeline_state_pool.Free(handle);
	}

	PipelineStateHandle CreateComputePipelineState(Device * device, const ComputePipelineStateDesc & compute_pipeline_state_desc, const char * name)
	{
		PipelineStateHandle handle = device->m_pipeline_state_pool.Alloc();

		auto& pipeline_state = device->Get(handle);

		//Fill the DX12 structs using our data, keep it for reloading
		D3D12_COMPUTE_PIPELINE_STATE_DESC DX12_pipeline_state_desc = { 0 };

		DX12_pipeline_state_desc.pRootSignature = device->Get(compute_pipeline_state_desc.root_signature).resource.Get();

		std::unordered_set<std::string> include_set;

		ComPtr<IDxcBlob> shader_blob;
		if (!CompileShader(device, compute_pipeline_state_desc.compute_shader, shader_blob, include_set))
		{
			device->m_pipeline_state_pool.Free(handle);
			SetLastErrorMessage(device, "Error compiling compute shader in compute pipeline state <%s>", name);
			return PipelineStateHandle();
		}
		
		DX12_pipeline_state_desc.CS.pShaderBytecode = shader_blob->GetBufferPointer();
		DX12_pipeline_state_desc.CS.BytecodeLength = shader_blob->GetBufferSize();

		//Create pipeline state
		if (FAILED(device->m_native_device->CreateComputePipelineState(&DX12_pipeline_state_desc, IID_PPV_ARGS(&pipeline_state.resource))))
		{
			device->m_pipeline_state_pool.Free(handle);
			SetLastErrorMessage(device, "Error creating compute pipeline state <%s>", name);
			return PipelineStateHandle();
		}

		SetObjectName(pipeline_state.resource.Get(), name);
		
		//Create the reload data
		device->m_pipeline_reload_data.emplace_back(name, handle, compute_pipeline_state_desc, DX12_pipeline_state_desc, include_set);

		return handle;
	}

	void DestroyComputePipelineState(Device * device, PipelineStateHandle & handle)
	{
		device->m_pipeline_state_pool.Free(handle);
	}

	void ReloadUpdatedShaders(Device* device)
	{
		core::LogInfo("Reloading shaders...");

		//List all the pipeline states and check if they need to reload
		for (auto& reload_pipeline : device->m_pipeline_reload_data)
		{
			//Check files
			bool updated = false;
			if (reload_pipeline.VertexShaderCompileReloadData.NeedsUpdate()) updated = true;
			if (reload_pipeline.PixelShaderCompileReloadData.NeedsUpdate()) updated = true;
			if (reload_pipeline.ComputeShaderCompileReloadData.NeedsUpdate()) updated = true;

			if (updated)
			{
				std::visit(
					overloaded
					{
						[&](D3D12_GRAPHICS_PIPELINE_STATE_DESC& graphics_pipeline_desc)
						{
							std::unordered_set<std::string> vertex_shader_include_set;

							ComPtr<IDxcBlob> vertex_shader_blob;
							if (!CompileShader(device, reload_pipeline.VertexShaderCompileReloadData.GetCompileShaderDescriptor(), vertex_shader_blob, vertex_shader_include_set, false))
							{
								core::LogWarning("Error reloading graphics pipeline state <%s> with errors <%s>, keeping last working one", reload_pipeline.name.c_str(), GetLastErrorMessage(device));
								return;
							}
							graphics_pipeline_desc.VS.pShaderBytecode = vertex_shader_blob->GetBufferPointer();
							graphics_pipeline_desc.VS.BytecodeLength = vertex_shader_blob->GetBufferSize();

							std::unordered_set<std::string> pixel_shader_include_set;

							ComPtr<IDxcBlob> pixel_shader_blob;
							if (!CompileShader(device, reload_pipeline.PixelShaderCompileReloadData.GetCompileShaderDescriptor(), pixel_shader_blob, pixel_shader_include_set, false))
							{
								core::LogWarning("Error reloading graphics pipeline state <%s> with errors <%s>, keeping last working one", reload_pipeline.name.c_str(), GetLastErrorMessage(device));
								return;
							}
							graphics_pipeline_desc.PS.pShaderBytecode = pixel_shader_blob->GetBufferPointer();
							graphics_pipeline_desc.PS.BytecodeLength = pixel_shader_blob->GetBufferSize();
							
							//reset the semantic names
							for (size_t i = 0; i < reload_pipeline.input_elements.size(); i++) reload_pipeline.input_elements[i].SemanticName = reload_pipeline.semantic_names[i].c_str();
							graphics_pipeline_desc.InputLayout = { reload_pipeline.input_elements.data(), static_cast<UINT>(reload_pipeline.input_elements.size())};
							

							graphics_pipeline_desc.pRootSignature = device->Get(reload_pipeline.root_signature_handle).resource.Get();

							ComPtr<ID3D12PipelineState> new_pipeline_state;
							if (FAILED(device->m_native_device->CreateGraphicsPipelineState(&graphics_pipeline_desc, IID_PPV_ARGS(&new_pipeline_state))))
							{
								core::LogWarning("Error reloading graphics pipeline state <%s> with errors <%s>, keeping last working one", reload_pipeline.name.c_str(), GetLastErrorMessage(device));
							}
							else
							{
								//Add to the old reloaded pipeline
								AddDeferredDeleteResource(device, device->Get(reload_pipeline.handle).resource);

								//Update
								device->Get(reload_pipeline.handle).resource.Swap(new_pipeline_state);

								SetObjectName(device->Get(reload_pipeline.handle).resource.Get(), reload_pipeline.name.c_str());

								reload_pipeline.VertexShaderCompileReloadData.UpdateIncludeSet(vertex_shader_include_set);
								reload_pipeline.PixelShaderCompileReloadData.UpdateIncludeSet(pixel_shader_include_set);

								reload_pipeline.VertexShaderCompileReloadData.UpdateTimeStamp();
								reload_pipeline.PixelShaderCompileReloadData.UpdateTimeStamp();

								core::LogInfo("Reloading graphics pipeline state <%s>", reload_pipeline.name.c_str());
							}
						},
						[&](D3D12_COMPUTE_PIPELINE_STATE_DESC& compute_pipeline_desc)
						{
							std::unordered_set<std::string> include_set;

							ComPtr<IDxcBlob> shader_blob;
							if (!CompileShader(device, reload_pipeline.ComputeShaderCompileReloadData.GetCompileShaderDescriptor(), shader_blob, include_set, false))
							{
								core::LogWarning("Error reloading compute pipeline state <%s> with errors<%s>, keeping last working one", reload_pipeline.name.c_str(), GetLastErrorMessage(device));
								return;
							}
							compute_pipeline_desc.CS.pShaderBytecode = shader_blob->GetBufferPointer();
							compute_pipeline_desc.CS.BytecodeLength = shader_blob->GetBufferSize();
							compute_pipeline_desc.pRootSignature = device->Get(reload_pipeline.root_signature_handle).resource.Get();

							ComPtr<ID3D12PipelineState> new_pipeline_state;
							if (FAILED(device->m_native_device->CreateComputePipelineState(&compute_pipeline_desc, IID_PPV_ARGS(&new_pipeline_state))))
							{
								core::LogWarning("Error reloading compute pipeline state <%s> with errors<%s>, keeping last working one", reload_pipeline.name.c_str(), GetLastErrorMessage(device));
							}
							else
							{
								//Add to the old reloaded pipeline
								AddDeferredDeleteResource(device, device->Get(reload_pipeline.handle).resource);

								//Update
								device->Get(reload_pipeline.handle).resource.Swap(new_pipeline_state);

								SetObjectName(device->Get(reload_pipeline.handle).resource.Get(), reload_pipeline.name.c_str());

								reload_pipeline.ComputeShaderCompileReloadData.UpdateIncludeSet(include_set);
								reload_pipeline.ComputeShaderCompileReloadData.UpdateTimeStamp();

								core::LogInfo("Reloading compute pipeline state <%s>", reload_pipeline.name.c_str());

							}
						}
					}, reload_pipeline.pipeline_desc);
			}
		}
	}
}