//////////////////////////////////////////////////////////////////////////
// Cute engine - Display layer
//////////////////////////////////////////////////////////////////////////
#ifndef DISPLAY_H_
#define DISPLAY_H_

//TODO: Move desc outside of display

namespace display
{
	//Types of resource handles
	enum class HandleType
	{
		Device,
		Adaptor,
		Texture,
		RenderTarget,
		Shader,
	};


	//Init, allocate system
	void Init();
	//Destroy
	void Destroy();

	//Adaptor
	

	//Device
	
	//Swap chain


}
#endif GFX_H_