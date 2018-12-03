//////////////////////////////////////////////////////////////////////////
// Cute engine - List of resources defined by default in the render pass system
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_RESOURCE_H_
#define RENDER_RESOURCE_H_

#include "render.h"
#include <display/display.h>

namespace render
{
	//Resource that can be created outside the render pass system, just a display handle
	template<typename HANDLE>
	class DisplayHandleResource : public Resource
	{
		HANDLE m_handle;
	public:
		void Init(HANDLE& handle)
		{
			m_handle = std::move(handle);
		}
		void Destroy(display::Device* device) override
		{
			display::DestroyHandle(device, m_handle);
		}
		HANDLE& GetHandle()
		{
			return m_handle;
		}
	};

	//Bool resource
	class BoolResource : Resource
	{
		bool m_value;
	public:
		DECLARE_RENDER_CLASS("Bool");

		void Load(LoadContext& load_context) override;
	};

	//Texture resource
	class TextureResource : DisplayHandleResource<display::ShaderResourceHandle>
	{
	public:
		DECLARE_RENDER_CLASS("Texture");

		void Load(LoadContext& load_context) override;
	};

	//Constant buffer resource
	class ConstantBufferResource : DisplayHandleResource<display::ConstantBufferHandle>
	{
	public:
		DECLARE_RENDER_CLASS("ConstantBuffer");

		void Load(LoadContext& load_context) override;
	};

	//Root signature resource
	class RootSignatureResource : DisplayHandleResource<display::RootSignatureHandle>
	{
	public:
		DECLARE_RENDER_CLASS("RootSignature");

		void Load(LoadContext& load_context) override;
	};
}
#endif //RENDER_RESOURCE_H_
