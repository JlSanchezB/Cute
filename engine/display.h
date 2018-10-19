//////////////////////////////////////////////////////////////////////////
// Cute engine - Display layer
//////////////////////////////////////////////////////////////////////////
#ifndef DISPLAY_H_
#define DISPLAY_H_

#include "handle_pool.h"

namespace display
{
	using CommandListHandle = core::Handle<struct CommandList, uint16_t>;
	using TextureHandle = core::Handle<struct Texture, uint16_t>;
	using RenderTargetHandle = core::Handle<struct RenderTarget, uint16_t>;
	using RootSignatureHandle = core::Handle<struct RootSignature, uint16_t>;
	using PSOHandle = core::Handle<struct PSO, uint16_t>;

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
	void OpenCommandList(Device* device, const CommandListHandle& handle);
	//Close command list, stop recording
	void CloseCommandList(Device* device, const CommandListHandle& handle);

	//Execute command list
	void ExecuteCommandList(Device* device, const CommandListHandle& handle);

	//Back buffer access
	RenderTargetHandle GetBackBuffer(Device* device);

	//Create root signature from a raw data
	RootSignatureHandle CreateRootSignature(Device* device, void* data, size_t size);
	//Destroy root signature
	void DestroyRootSignature(Device * device, RootSignatureHandle& root_signature_handle);


	//Context commands
	void SetRenderTargets(Device* device, const CommandListHandle& handle, size_t num_targets, RenderTargetHandle* render_target_array, RenderTargetHandle* depth_stencil);

	void ClearRenderTargetColour(Device* device, const CommandListHandle& handle, const RenderTargetHandle& render_target, const float colour[4]);
}
#endif DISPLAY_H_