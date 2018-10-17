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
	using ShaderHandle = core::Handle<struct Shader, uint16_t>;

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
	void OpenCommandList(Device* device, CommandListHandle handle);
	//Close command list, stop recording
	void CloseCommandList(Device* device, CommandListHandle handle);

	//Back buffer access
	RenderTargetHandle GetBackBuffer(Device* device);

	//Context
	struct Context
	{
		Device* device;
		CommandListHandle command_list;
	};

}
#endif DISPLAY_H_