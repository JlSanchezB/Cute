//////////////////////////////////////////////////////////////////////////
// Cute engine - Display layer
//////////////////////////////////////////////////////////////////////////
#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <core/handle_pool.h>
#include "display_handle.h"
#include "display_enum.h"
#include "display_desc.h"
#include <optional>

namespace display
{
	//Device
	struct Device;

	struct DeviceInitParams
	{
		uint8_t num_frames;
		uint32_t width;
		uint32_t height;
		bool tearing = false;
		bool vsync = false;
		uint32_t adapter_index = -1;
		size_t upload_buffer_max_size = 256 * 1024;

		//Debug
		bool debug = false;
		bool debug_shaders = false;
	};

	inline uint32_t CalculateGroupCount(uint32_t num_lanes, uint32_t lanes_for_group)
	{
		return 1 + (num_lanes - 1) / lanes_for_group;
	}

	struct Context;

	Device* CreateDevice(const DeviceInitParams& params);
	void DestroyDevice(Device* device);

	//Get last error message
	const char* GetLastErrorMessage(Device* device);

	//Present
	void Present(Device* device);

	//Begin/End Frame
	void BeginFrame(Device* device);
	void EndFrame(Device* device);

	//Get Last Processed frame in the GPU, used for syning resources
	uint64_t GetLastCompletedGPUFrame(Device* device);

	//Command List
	CommandListHandle CreateCommandList(Device* device, const char* name = nullptr);
	void DestroyCommandList(Device* device, CommandListHandle& handle);

	//Open command list, begin recording. Return the context for recording
	Context* OpenCommandList(Device* device, const WeakCommandListHandle& handle);
	//Close command list, stop recording
	void CloseCommandList(Device* device, Context* context);

	//Execute command list
	void ExecuteCommandList(Device* device, const WeakCommandListHandle& handle);

	//Execute command lists
	void ExecuteCommandLists(Device* device, const std::vector<WeakCommandListHandle>& handles);
	
	//Back buffer access
	WeakTexture2DHandle GetBackBuffer(Device* device);

	//Create root signature
	RootSignatureHandle CreateRootSignature(Device* device, const RootSignatureDesc& root_signature_desc, const char* name = nullptr);
	//Destroy root signature
	void DestroyRootSignature(Device * device, RootSignatureHandle& root_signature_handle);

	//Create pipeline state
	PipelineStateHandle CreatePipelineState(Device* device, const PipelineStateDesc& pipeline_state_desc, const char* name = nullptr);
	//Destroy pipeline state
	void DestroyPipelineState(Device* device, PipelineStateHandle& handle);

	//Create compute pipeline state
	PipelineStateHandle CreateComputePipelineState(Device* device, const ComputePipelineStateDesc& compute_pipeline_state_desc, const char* name = nullptr);
	//Destroy compute pipeline state
	void DestroyComputePipelineState(Device* device, PipelineStateHandle& handle);

	//Reload updated shaders
	void ReloadUpdatedShaders(Device* device);

	//Create descriptor table
	DescriptorTableHandle CreateDescriptorTable(Device* device, const DescriptorTableDesc& descriptor_table_desc);
	//Destroy descriptor table
	void DestroyDescriptorTable(Device * device, DescriptorTableHandle& handle);
	//Update descriptor table
	void UpdateDescriptorTable(Device* device, const WeakDescriptorTableHandle& handle, const DescriptorTableDesc::Descritor* descriptor_table, size_t descriptor_count);

	//Create sampler Descriptor table
	SamplerDescriptorTableHandle CreateSamplerDescriptorTable(Device* device, const SamplerDescriptorTableDesc& sampler_descriptor_table);
	//Destroy sampler descriptor table
	void DestroySamplerDescriptorTable(Device * device, SamplerDescriptorTableHandle& handle);

	//Create buffer 1D
	BufferHandle CreateBuffer(Device* device, const BufferDesc& buffer_desc, const char* name);
	void DestroyBuffer(Device* device, BufferHandle& handle);

	//Create texture2D
	Texture2DHandle CreateTexture2D(Device* device, const Texture2DDesc & texture_2d_desc, const char* name);
	void DestroyTexture2D(Device* device, Texture2DHandle& handle);

	//Update resource buffer (only Access::Dynamic)
	using UpdatableResourceHandle = std::variant<WeakBufferHandle>;
	void UpdateResourceBuffer(Device* device, const UpdatableResourceHandle& handle, const void* data, size_t size);

	//Get Resource memory (only Access::Dynamic or Access::Upload)
	using DirectAccessResourceHandle = std::variant<WeakBufferHandle>;
	void* GetResourceMemoryBuffer(Device* device, const DirectAccessResourceHandle& handle);

	//Get last ReadBack memory written (only Access:ReadBack)
	using ReadBackResourceHandle = std::variant<WeakBufferHandle>;
	void* GetLastWrittenResourceMemoryBuffer(Device* device, const ReadBackResourceHandle& handle);

	//Pipe used
	enum class Pipe : uint8_t
	{
		Graphics,
		Compute
	};

	//Resource barrier
	struct ResourceBarrier
	{
		ResourceBarrierType type;

		using ResourceHandle = std::variant<WeakBufferHandle, WeakTexture2DHandle>;
		ResourceHandle resource;

		TranstitionState state_before;
		TranstitionState state_after;

		ResourceBarrier(const WeakBufferHandle& handle)
		{
			type = ResourceBarrierType::UnorderAccess;
			resource = handle;
		}

		ResourceBarrier(const ResourceHandle& handle, const TranstitionState& _state_before, const TranstitionState& _state_after)
		{
			type = ResourceBarrierType::Transition;
			resource = handle;
			state_before = _state_before;
			state_after = _state_after;
		}
	};

	//Context
	struct Context
	{
		//Get device
		Device* GetDevice();

		//Set render target
		void SetRenderTargets(uint8_t num_targets, AsRenderTarget* render_target_array, AsDepthBuffer depth_stencil);

		//Clear Render Target
		void ClearRenderTargetColour(const AsRenderTarget& render_target, const float colour[4]);

		//Clear Depth Stencil
		void ClearDepthStencil(const AsDepthBuffer& depth_stencil, const ClearType& clear_type, std::optional<float> depth, std::optional <uint8_t> stencil);

		//Set Viewport
		void SetViewport(const Viewport& viewport);

		//Set Scissor
		void SetScissorRect(const Rect scissor_rect);

		//Set root signature
		void SetRootSignature(const Pipe& pipe, const WeakRootSignatureHandle& root_signature);

		//Set pipeline state
		void SetPipelineState(const WeakPipelineStateHandle& pipeline_state);

		//Set Vertex buffers
		void SetVertexBuffers(uint8_t start_slot_index, uint8_t num_vertex_buffers, WeakBufferHandle* vertex_buffers);

		//Set Index Buffer
		void SetIndexBuffer(const WeakBufferHandle& index_buffer);

		//Set constants
		void SetConstants(const Pipe& pipe, uint8_t root_parameter, const void* data, size_t num_constants);

		//Set constant buffer
		void SetConstantBuffer(const Pipe& pipe, uint8_t root_parameter, const WeakBufferHandle& constant_buffer);

		//Set unordered access buffer
		void SetUnorderedAccessBuffer(const Pipe& pipe, uint8_t root_parameter, const WeakBufferHandle& unordered_access_buffer);

		//Set shader resource
		void SetShaderResource(const Pipe& pipe, uint8_t root_parameter, const WeakBufferHandle& shader_resource);

		//Set descriptor table
		void SetDescriptorTable(const Pipe& pipe, uint8_t root_parameter, const WeakDescriptorTableHandle& descriptor_table);

		//Set descriptor table
		void SetDescriptorTable(const Pipe& pipe, uint8_t root_parameter, const WeakSamplerDescriptorTableHandle& sampler_descriptor_table);

		//Draw
		void Draw(const DrawDesc& draw_desc);

		//Draw Indexed
		void DrawIndexed(const DrawIndexedDesc& draw_desc);

		//Draw Indexed Instanced
		void DrawIndexedInstanced(const DrawIndexedInstancedDesc& draw_desc);

		//Indirect Draw Indexed
		void IndirectDrawIndexed(const IndirectDrawIndexedDesc& draw_desc);

		//Indirect Draw Indexed Instanced
		void IndirectDrawIndexedInstanced(const IndirectDrawIndexedInstancedDesc& draw_desc);

		//Execute compute
		void ExecuteCompute(const ExecuteComputeDesc& execute_compute_desc);

		//Indirect Execute Compute
		void IndirectExecuteCompute(const IndirectExecuteComputeDesc& execute_compute_desc);

		//Resource barriers
		void AddResourceBarriers(const std::vector<ResourceBarrier>& resource_barriers);

		//Update a Buffer with data, the data needs to be copied (never read) into the returned pointed (only works with Access::Static)
		void* UpdateBufferResource(display::BufferHandle dest_resource, size_t dest_offset, size_t size);
	};

	template<typename HANDLE>
	void DestroyHandle(Device* device, HANDLE& handle)
	{
		if (handle.IsValid())
		{
			DestroyHandleInternal(device, handle);
		}
		else
		{
			core::LogWarning("Trying to destroy a invalid display handle");
		}
	}

	template<typename HANDLE>
	void DestroyHandleInternal(Device* device, HANDLE& handle);

	template<>
	inline void DestroyHandleInternal(Device* device, RootSignatureHandle& handle)
	{
		DestroyRootSignature(device, handle);
	}

	template<>
	inline void DestroyHandleInternal(Device* device, PipelineStateHandle& handle)
	{
		DestroyPipelineState(device, handle);
	}

	template<>
	inline void DestroyHandleInternal(Device* device, DescriptorTableHandle& handle)
	{
		DestroyDescriptorTable(device, handle);
	}


	template<>
	inline void DestroyHandleInternal(Device* device, CommandListHandle& handle)
	{
		DestroyCommandList(device, handle);
	}

	template<>
	inline void DestroyHandleInternal(Device* device, BufferHandle& handle)
	{
		DestroyBuffer(device, handle);
	}

	template<>
	inline void DestroyHandleInternal(Device* device, Texture2DHandle& handle)
	{
		DestroyTexture2D(device, handle);
	}

}
#endif DISPLAY_H_