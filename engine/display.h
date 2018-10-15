//////////////////////////////////////////////////////////////////////////
// Cute engine - Display layer
//////////////////////////////////////////////////////////////////////////
#ifndef DISPLAY_H_
#define DISPLAY_H_

#include "handle_pool.h"

namespace display
{
	struct Texture;
	struct RenderTarget;
	struct Shader;

	using TextureHandle = core::Handle<Texture, uint16_t>;
	using RenderTargetHandle = core::Handle<RenderTarget, uint16_t>;
	using ShaderHandle = core::Handle<Shader, uint16_t>;

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
}
#endif DISPLAY_H_