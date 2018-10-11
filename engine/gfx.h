//////////////////////////////////////////////////////////////////////////
// Cute engine - Graphics library
//////////////////////////////////////////////////////////////////////////
#ifndef GFX_H_
#define GFX_H_

//TODO: Move desc outside of GFX
//TODO: Move handles outside of GFX

namespace gfx
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

	//Handle of a resource, specialised by a enum TYPE and the size
	template <enum TYPE, typename SIZE>
	struct Handle
	{
		SIZE m_index;
	};

	//List of handles resource types
	using Handle<HandleType::Device, uint8> DeviceHandle;
	using Handle<HandleType::Adaptor, uint8> AdaptorHandle;

	//Error and Assert hooks
	void RegisterErrorHook(void(*hook)(const char*));
	void RegisterAssertHook(void(*hook)(const char*));

	//Init, allocate system
	void Init();
	//Destroy
	void Destroy();

	//Adaptor
	struct AdaptorDesc
	{
		char name[128];
		bool software_implementation;
	};

	size_t GetNumAdaptors();
	AdaptorHandle GetAdaptor(size_t index);
	AdaptorDesc GetAdaptorDesc(AdaptorHandle adaptor_handle);

	//Device
	DeviceHandle CreateDevice(AdaptorHandle adaptor_handle);
	void DestroyDevice(DeviceHandle device_handle);

	//Swap chain


}
#endif GFX_H_