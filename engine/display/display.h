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
	//TODO: Access to adaptors

	//Device
	struct Device;


	struct DeviceInitParams
	{
		size_t num_frames;
		size_t width;
		size_t height;
		bool debug;
	};

	Device* CreateDevice(const DeviceInitParams& params);
	void DestroyDevice(Device* device);

	//Present
	void Present(Device* device);

	//Begin/End Frame
	void BeginFrame(Device* device);
	void EndFrame(Device* device);

	//Command List
	CommandListHandle CreateCommandList(Device* device);
	void DestroyCommandList(Device* device, CommandListHandle& handle);

	//Open command list, begin recording
	void OpenCommandList(Device* device, const WeakCommandListHandle& handle);
	//Close command list, stop recording
	void CloseCommandList(Device* device, const WeakCommandListHandle& handle);

	//Execute command list
	void ExecuteCommandList(Device* device, const WeakCommandListHandle& handle);

	//Back buffer access
	WeakRenderTargetHandle GetBackBuffer(Device* device);

	//Create root signature from a raw data
	RootSignatureHandle CreateRootSignature(Device* device, void* data, size_t size);
	//Destroy root signature
	void DestroyRootSignature(Device * device, RootSignatureHandle& root_signature_handle);

	//Create pipeline state
	PipelineStateHandle CreatePipelineState(Device* device, const PipelineStateDesc& pipeline_state_desc);
	//Destroy pipeline state
	void DestroyPipelineState(Device* device, PipelineStateHandle& handle);

	//Create vertex buffer
	VertexBufferHandle CreateVertexBuffer(Device* device, void* data, size_t stride, size_t size);
	//Destroy vertex buffer
	void DestroyVertexBuffer(Device* device, VertexBufferHandle& handle);

	//Create vertex buffer
	IndexBufferHandle CreateIndexBuffer(Device* device, void* data, size_t size, Format format);
	//Destroy vertex buffer
	void DestroyIndexBuffer(Device* device, IndexBufferHandle& handle);

	//Context commands

	//Set render target
	void SetRenderTargets(Device* device, const WeakCommandListHandle& command_list, size_t num_targets, WeakRenderTargetHandle* render_target_array, WeakRenderTargetHandle* depth_stencil);

	//Clear
	void ClearRenderTargetColour(Device* device, const WeakCommandListHandle& command_list, const WeakRenderTargetHandle& render_target, const float colour[4]);

	//Set root signature
	void SetRootSignature(Device* device, const WeakCommandListHandle& command_list, const WeakRootSignatureHandle& root_signature);

	//Set pipeline state
	void SetPipelineState(Device* device, const WeakCommandListHandle& command_list, const WeakPipelineStateHandle& pipeline_state);

	//Set Vertex buffers
	void SetVertexBuffers(Device* device, const WeakCommandListHandle& command_list, size_t start_slot_index, size_t num_vertex_buffers, WeakVertexBufferHandle* vertex_buffers);

	//Set Index Buffer
	void SetIndexBuffer(Device* device, const WeakCommandListHandle& command_list, const WeakIndexBufferOHandle& index_buffer);

	//Draw
	void Draw(Device* device, const WeakCommandListHandle& command_list, size_t start_vertex, size_t vertex_count, PrimitiveTopology primitive_topology);

}
#endif DISPLAY_H_