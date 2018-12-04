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

	//Render target resource
	class RenderTargetResource : DisplayHandleResource<display::RenderTargetHandle>
	{
	public:
		DECLARE_RENDER_CLASS("RenderTarget");

		void Load(LoadContext& load_context) override;
	};

	//Depth buffer
	class DepthBufferResource : DisplayHandleResource<display::DepthBufferHandle>
	{
	public:
		DECLARE_RENDER_CLASS("DepthBuffer");

		void Load(LoadContext& load_context) override;
	};

	//Graphics pipeline state resource
	class GraphicsPipelineStateResource : DisplayHandleResource<display::PipelineStateHandle>
	{
	public:
		DECLARE_RENDER_CLASS("GraphicsPipelineState");

		void Load(LoadContext& load_context) override;
	};

	//Compute pipeline state resource
	class ComputePipelineStateResource : DisplayHandleResource<display::PipelineStateHandle>
	{
	public:
		DECLARE_RENDER_CLASS("ComputePipelineState");

		void Load(LoadContext& load_context) override;
	};

	//Descriptor table resource
	class DescriptorTableResource : DisplayHandleResource<display::DescriptorTableHandle>
	{
	public:
		DECLARE_RENDER_CLASS("DescriptorTable");

		void Load(LoadContext& load_context) override;
	};
}
#endif //RENDER_RESOURCE_H_
