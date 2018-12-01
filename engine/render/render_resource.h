//////////////////////////////////////////////////////////////////////////
// Cute engine - List of resources defined by default in the render pass system
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_RESOURCE_H_
#define RENDER_RESOURCE_H_

#include "render.h"
#include <display/display_handle.h>

namespace render
{
	//Bool resource
	class BoolResource : Resource
	{
		bool m_value;
	public:
		void Load(LoadContext* load_context) override;
		const char* Type() const
		{
			return "Bool";
		}
	};

	//Texture resource
	class TextureResource : Resource
	{
		display::ShaderResourceHandle m_shader_resource_handle;
	public:
		void Load(LoadContext* load_context) override;
		const char* Type() const
		{
			return "Texture";
		}
	};

	//Constant buffer resource
	class ConstantBufferResource : Resource
	{
		display::ConstantBufferHandle m_constant_buffer_handle;
	public:
		void Load(LoadContext* load_context) override;
		const char* Type() const
		{
			return "ConstantBuffer";
		}
	};

	//Root signature resource
	class RootSignatureResource : Resource
	{
		display::RootSignatureHandle m_root_signature_handle;
	public:
		void Load(LoadContext* load_context) override;
		const char* Type() const
		{
			return "RootSignature";
		}
	};
}
#endif //RENDER_RESOURCE_H_
