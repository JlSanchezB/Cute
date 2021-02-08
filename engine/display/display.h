//////////////////////////////////////////////////////////////////////////
// Cute engine - Display layer
//////////////////////////////////////////////////////////////////////////
#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <core/handle_pool.h>
#include "display_handle.h"
#include "display_enum.h"
#include "display_desc.h"

namespace display
{
	//Device
	struct Device;

	struct DeviceInitParams
	{
		uint8_t num_frames;
		uint32_t width;
		uint32_t height;
		bool debug = false;
		bool tearing = false;
		bool vsync = false;
		uint32_t adapter_index = -1;
	};

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
	WeakRenderTargetHandle GetBackBuffer(Device* device);

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

	//Compile shader
	bool CompileShader(Device* device, const CompileShaderDesc& compile_shader_desc, std::vector<char>& shader_blob);

	//Create vertex buffer
	VertexBufferHandle CreateVertexBuffer(Device* device, const VertexBufferDesc& vertex_buffer_desc, const char* name = nullptr);
	//Destroy vertex buffer
	void DestroyVertexBuffer(Device* device, VertexBufferHandle& handle);

	//Create vertex buffer
	IndexBufferHandle CreateIndexBuffer(Device* device, const IndexBufferDesc& index_buffer_desc, const char* name = nullptr);
	//Destroy vertex buffer
	void DestroyIndexBuffer(Device* device, IndexBufferHandle& handle);

	//Create render target
	RenderTargetHandle CreateRenderTarget(Device* device, const RenderTargetDesc& render_target_desc, const char* name = nullptr);
	//Destroy render target
	void DestroyRenderTarget(Device* device, RenderTargetHandle& handle);

	//Create depth buffer
	DepthBufferHandle CreateDepthBuffer(Device* device, const DepthBufferDesc& depth_buffer_desc, const char* name = nullptr);
	//Destroy depth buffer
	void DestroyDepthBuffer(Device* device, DepthBufferHandle& handle);

	//Create constant buffer
	ConstantBufferHandle CreateConstantBuffer(Device* device, const ConstantBufferDesc& constant_buffer_desc, const char* name = nullptr);
	//Destroy constant buffer
	void DestroyConstantBuffer(Device* device, ConstantBufferHandle& handle);

	//Create unordered access buffer
	UnorderedAccessBufferHandle CreateUnorderedAccessBuffer(Device* device, const UnorderedAccessBufferDesc& unordered_access_buffer_desc, const char* name = nullptr);
	//Destroy unordered access buffer
	void DestroyUnorderedAccessBuffer(Device* device, UnorderedAccessBufferHandle& handle);

	//Create a generic shader resource
	ShaderResourceHandle CreateShaderResource(Device* device, const ShaderResourceDesc& shader_resource_desc, const char* name = nullptr);
	//Create a texture from platform dependent buffer
	ShaderResourceHandle CreateTextureResource(Device* device, const void* data, size_t size, const char* name = nullptr);
	//Destroy shader resource
	void DestroyShaderResource(Device * device, ShaderResourceHandle& handle);

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

	//Update resource buffer (only Access::Dynamic)
	using UpdatableResourceHandle = std::variant<WeakConstantBufferHandle, WeakVertexBufferHandle, WeakIndexBufferHandle>;
	void UpdateResourceBuffer(Device* device, const UpdatableResourceHandle& handle, const void* data, size_t size);

	//Get Resource memory
	using DirectAccessResourceHandle = std::variant<WeakShaderResourceHandle, WeakConstantBufferHandle, WeakVertexBufferHandle, WeakIndexBufferHandle>;
	void* GetResourceMemoryBuffer(Device* device, const DirectAccessResourceHandle& handle);

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

		using ResourceHandle = std::variant<WeakUnorderedAccessBufferHandle,  WeakRenderTargetHandle, WeakDepthBufferHandle>;
		ResourceHandle resource;

		TranstitionState state_before;
		TranstitionState state_after;

		ResourceBarrier(const WeakUnorderedAccessBufferHandle& handle)
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
		void SetRenderTargets(uint8_t num_targets, WeakRenderTargetHandle* render_target_array, WeakDepthBufferHandle depth_stencil);

		//Clear
		void ClearRenderTargetColour(const WeakRenderTargetHandle& render_target, const float colour[4]);

		//Set Viewport
		void SetViewport(const Viewport& viewport);

		//Set Scissor
		void SetScissorRect(const Rect scissor_rect);

		//Set root signature
		void SetRootSignature(const Pipe& pipe, const WeakRootSignatureHandle& root_signature);

		//Set pipeline state
		void SetPipelineState(const WeakPipelineStateHandle& pipeline_state);

		//Set Vertex buffers
		void SetVertexBuffers(uint8_t start_slot_index, uint8_t num_vertex_buffers, WeakVertexBufferHandle* vertex_buffers);

		//Set Shader resource as a vertex buffer
		void SetShaderResourceAsVertexBuffer(uint8_t start_slot_index, const WeakShaderResourceHandle& shader_resource, const SetShaderResourceAsVertexBufferDesc& desc);

		//Set Index Buffer
		void SetIndexBuffer(const WeakIndexBufferHandle& index_buffer);

		//Set constants
		void SetConstants(const Pipe& pipe, uint8_t root_parameter, const void* data, size_t num_constants);

		//Set constant buffer
		void SetConstantBuffer(const Pipe& pipe, uint8_t root_parameter, const WeakConstantBufferHandle& constant_buffer);

		//Set unordered access buffer
		void SetUnorderedAccessBuffer(const Pipe& pipe, uint8_t root_parameter, const WeakUnorderedAccessBufferHandle& unordered_access_buffer);

		using ShaderResourceSet = std::variant<WeakShaderResourceHandle, WeakUnorderedAccessBufferHandle>;
		//Set shader resource
		void SetShaderResource(const Pipe& pipe, uint8_t root_parameter, const ShaderResourceSet& shader_resource);

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

		//Execute compute
		void ExecuteCompute(const ExecuteComputeDesc& execute_compute_desc);

		//Resource barriers
		void AddResourceBarriers(const std::vector<ResourceBarrier>& resource_barriers);
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
	inline void DestroyHandleInternal(Device* device, UnorderedAccessBufferHandle& handle)
	{
		DestroyUnorderedAccessBuffer(device, handle);
	}

	template<>
	inline void DestroyHandleInternal(Device* device, ConstantBufferHandle& handle)
	{
		DestroyConstantBuffer(device, handle);
	}

	template<>
	inline void DestroyHandleInternal(Device* device, RenderTargetHandle& handle)
	{
		DestroyRenderTarget(device, handle);
	}

	template<>
	inline void DestroyHandleInternal(Device* device, ShaderResourceHandle& handle)
	{
		DestroyShaderResource(device, handle);
	}

	template<>
	inline void DestroyHandleInternal(Device* device, DescriptorTableHandle& handle)
	{
		DestroyDescriptorTable(device, handle);
	}

	template<>
	inline void DestroyHandleInternal(Device* device, VertexBufferHandle& handle)
	{
		DestroyVertexBuffer(device, handle);
	}

	template<>
	inline void DestroyHandleInternal(Device* device, IndexBufferHandle& handle)
	{
		DestroyIndexBuffer(device, handle);
	}

	template<>
	inline void DestroyHandleInternal(Device* device, CommandListHandle& handle)
	{
		DestroyCommandList(device, handle);
	}

	template<>
	inline void DestroyHandleInternal(Device* device, DepthBufferHandle& handle)
	{
		DestroyDepthBuffer(device, handle);
	}

}
#endif DISPLAY_H_