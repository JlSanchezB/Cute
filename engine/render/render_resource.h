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
		DECLARE_RENDER_CLASS("Bool");

		void Load(LoadContext& load_context) override;
	};

	//Texture resource
	class TextureResource : Resource
	{
		display::ShaderResourceHandle m_shader_resource_handle;
	public:
		DECLARE_RENDER_CLASS("Texture");

		void Load(LoadContext& load_context) override;
	};

	//Constant buffer resource
	class ConstantBufferResource : Resource
	{
		display::ConstantBufferHandle m_constant_buffer_handle;
	public:
		DECLARE_RENDER_CLASS("ConstantBuffer");

		void Load(LoadContext& load_context) override;
	};

	//Root signature resource
	class RootSignatureResource : Resource
	{
		display::RootSignatureHandle m_root_signature_handle;
	public:
		DECLARE_RENDER_CLASS("RootSignature");

		void Load(LoadContext& load_context) override;
	};
}
#endif //RENDER_RESOURCE_H_
