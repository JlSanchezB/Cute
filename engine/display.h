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
	
	//Adaptor
	

	//Device
	struct Device;


	//Swap chain


}
#endif DISPLAY_H_